#include "BlueprintHttpServer.h"

#include "BlueprintCommandFile.h"
#include "FactorySpawner.h"
#include "FactorySpawnerChat.h"

#include "Containers/StringConv.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "FGBlueprintSubsystem.h"
#include "FGGameMode.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "IPAddress.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    using ECode = EHttpServerResponseCodes;
    constexpr int32 MaxPending = 8;
    constexpr int32 MaxResults = 64;

    bool SendJson(const FHttpResultCallback& Reply, ECode Code, const TSharedRef<FJsonObject>& Object)
    {
        FString Body;
        FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Body));
        auto Response = FHttpServerResponse::Create(Body, TEXT("application/json; charset=utf-8"));
        Response->Code = Code;
        Response->Headers.Add(TEXT("cache-control"), {TEXT("no-store")});
        Reply(MoveTemp(Response));
        return true;
    }

    bool SendError(const FHttpResultCallback& Reply, ECode Code, const FString& Error)
    {
        const auto Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("error"), Error);
        return SendJson(Reply, Code, Object);
    }

    bool IsLocalClient(const FHttpServerRequest& Request)
    {
        return Request.PeerAddress && Request.PeerAddress->ToString(false) == TEXT("127.0.0.1") &&
               !Request.Headers.Contains(TEXT("origin"));
    }

    TSharedPtr<IHttpRouter> OpenLocalRouter(uint32 Port)
    {
        const TCHAR* Section = TEXT("HTTPServer.Listeners");
        const TCHAR* Key = TEXT("ListenerOverrides");
        TArray<FString> Previous;
        GConfig->GetArray(Section, Key, Previous, GEngineIni);
        auto Local = Previous;
        Local.Insert(FString::Printf(TEXT("(Port=%u,BindAddress=127.0.0.1,ReuseAddressAndPort=false)"), Port), 0);
        GConfig->SetArray(Section, Key, Local, GEngineIni);
        ON_SCOPE_EXIT
        {
            GConfig->SetArray(Section, Key, Previous, GEngineIni);
        };
        auto& Server = FHttpServerModule::Get();
        Server.StartAllListeners();
        return Server.GetHttpRouter(Port, true);
    }

    class FBlueprintHttpService
    {
    public:
        bool Start(uint32 Port)
        {
            Router = OpenLocalRouter(Port);
            if (!Router)
                return false;

            SubmitRoute = Router->BindRoute(FHttpPath(TEXT("/blueprints")), EHttpServerRequestVerbs::VERB_POST,
                FHttpRequestHandler::CreateRaw(this, &FBlueprintHttpService::Submit));
            StatusRoute = Router->BindRoute(FHttpPath(TEXT("/blueprints/status")), EHttpServerRequestVerbs::VERB_GET,
                FHttpRequestHandler::CreateRaw(this, &FBlueprintHttpService::Status));
            if (!SubmitRoute || !StatusRoute)
                return false;

            Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FBlueprintHttpService::Tick));
            return true;
        }

        ~FBlueprintHttpService()
        {
            if (Ticker.IsValid())
                FTSTicker::GetCoreTicker().RemoveTicker(Ticker);
            if (SubmitRoute)
                Router->UnbindRoute(SubmitRoute);
            if (StatusRoute)
                Router->UnbindRoute(StatusRoute);
        }

        void Attach(AFactorySpawnerChat* Actor) { CurrentActor = Actor; }

        bool IsAttachedTo(const AFactorySpawnerChat* Actor) const { return CurrentActor.Get(true) == Actor; }

    private:
        struct FJob
        {
            FString Id;
            TWeakObjectPtr<AFactorySpawnerChat> Actor;
            FString Command;
            FBlueprintCommandResult Result;
            bool bFinished = false;
        };

        bool Submit(const FHttpServerRequest& Request, const FHttpResultCallback& Reply)
        {
            check(IsInGameThread());
            if (!IsLocalClient(Request))
                return SendError(Reply, ECode::Forbidden, TEXT("Use a local script or HTTP client."));
            auto* Actor = CurrentActor.Get();
            if (!Actor || !AFGBlueprintSubsystem::Get(Actor->GetWorld()))
                return SendError(Reply, ECode::ServiceUnavail, TEXT("Open a single-player save first."));

            const auto* Types = Request.Headers.Find(TEXT("content-type"));
            FString Mime = Types && !Types->IsEmpty() ? (*Types)[0] : FString();
            int32 ParameterIndex;
            if (Mime.FindChar(TCHAR(';'), ParameterIndex))
                Mime.LeftInline(ParameterIndex);
            if (!Mime.TrimStartAndEnd().Equals(TEXT("application/json"), ESearchCase::IgnoreCase))
                return SendError(Reply, ECode::UnsupportedMedia, TEXT("Use Content-Type: application/json."));
            if (Request.Body.IsEmpty())
                return SendError(Reply, ECode::BadRequest, TEXT("Expected a JSON command."));
            if (Request.Body.Num() > MaxBlueprintCommandBytes)
                return SendError(Reply, ECode::RequestTooLarge, TEXT("Command request exceeds 64 KiB."));

            const FUTF8ToTCHAR Text(reinterpret_cast<const UTF8CHAR*>(Request.Body.GetData()), Request.Body.Num());
            const FString Body(Text.Length(), Text.Get());
            TSharedPtr<FJsonObject> Object;
            FString Command;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Object) || !Object ||
                !Object->HasTypedField<EJson::String>(TEXT("command")) || !Object->TryGetStringField(TEXT("command"), Command) ||
                Command.TrimStartAndEnd().IsEmpty())
                return SendError(Reply, ECode::BadRequest, TEXT("Expected a nonempty string field named command."));

            if (Pending.Num() >= MaxPending)
                return SendError(Reply, ECode::TooManyRequests, TEXT("The blueprint queue is full."));
            const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
            Jobs.Add({Id, Actor, MoveTemp(Command)});
            Pending.Add(Id);
            if (Jobs.Num() > MaxResults)
            {
                const int32 Oldest = Jobs.IndexOfByPredicate([](const FJob& Job) { return Job.bFinished; });
                if (Oldest != INDEX_NONE)
                    Jobs.RemoveAt(Oldest);
            }

            const auto Response = MakeShared<FJsonObject>();
            Response->SetStringField(TEXT("id"), Id);
            Response->SetStringField(TEXT("status"), TEXT("queued"));
            return SendJson(Reply, ECode::Accepted, Response);
        }

        bool Status(const FHttpServerRequest& Request, const FHttpResultCallback& Reply)
        {
            check(IsInGameThread());
            if (!IsLocalClient(Request))
                return SendError(Reply, ECode::Forbidden, TEXT("Use a local script or HTTP client."));
            const FString* Id = Request.QueryParams.Find(TEXT("id"));
            if (!Id || Id->IsEmpty())
                return SendError(Reply, ECode::BadRequest, TEXT("Specify an id."));
            const auto* Job = Jobs.FindByPredicate([&](const FJob& Candidate) { return Candidate.Id == *Id; });
            if (!Job)
                return SendError(Reply, ECode::NotFound, TEXT("Unknown or expired request."));

            const auto Response = MakeShared<FJsonObject>();
            Response->SetStringField(TEXT("id"), Job->Id);
            Response->SetStringField(TEXT("status"), !Job->bFinished ? TEXT("queued")
                : Job->Result.Error.IsEmpty() ? TEXT("completed") : TEXT("failed"));
            if (Job->bFinished)
            {
                Response->SetStringField(TEXT("blueprint_name"), Job->Result.Name);
                Response->SetStringField(TEXT("blueprint_path"), Job->Result.Path);
                Response->SetStringField(TEXT("summary"), Job->Result.Summary);
                Response->SetStringField(TEXT("materials"), Job->Result.MaterialSummary);
                Response->SetStringField(TEXT("error"), Job->Result.Error);
                Response->SetBoolField(TEXT("preview"), Job->Result.bPreview);
            }
            return SendJson(Reply, ECode::Ok, Response);
        }

        bool Tick(float)
        {
            check(IsInGameThread());
            if (Pending.IsEmpty())
                return true;

            const FString Id = MoveTemp(Pending[0]);
            Pending.RemoveAt(0);
            auto* Job = Jobs.FindByPredicate([&](const FJob& Candidate) { return Candidate.Id == Id; });
            check(Job);
            auto* Actor = Job->Actor.Get();
            if (Actor && Actor == CurrentActor.Get())
            {
                const auto Status = Actor->RunBlueprintCommand(MoveTemp(Job->Command), Job->Result);
                if (Status != EExecutionStatus::COMPLETED && Job->Result.Error.IsEmpty())
                    Job->Result.Error = TEXT("Blueprint command failed.");
            }
            else
                Job->Result.Error = TEXT("The original game session was closed.");
            Job->bFinished = true;
            return true;
        }

        TWeakObjectPtr<AFactorySpawnerChat> CurrentActor;
        TSharedPtr<IHttpRouter> Router;
        FHttpRouteHandle SubmitRoute;
        FHttpRouteHandle StatusRoute;
        FTSTicker::FDelegateHandle Ticker;
        TArray<FJob> Jobs;
        TArray<FString> Pending;
    };

    TUniquePtr<FBlueprintHttpService> Service;
}

void BlueprintHttpServer::Attach(AFactorySpawnerChat* Actor)
{
    if (Actor->GetNetMode() != NM_Standalone)
        return;
    const auto* GameMode = Actor->GetWorld()->GetAuthGameMode<AFGGameMode>();
    if (!GameMode || GameMode->IsMainMenuGameMode())
        return;
    if (!Service)
    {
        int32 Port = 8765;
        FParse::Value(FCommandLine::Get(), TEXT("FactorySpawnerHttpPort="), Port);
        if (Port < 1 || Port > 65535)
        {
            UE_LOG(LogFactorySpawner, Warning, TEXT("FactorySpawner HTTP port must be in the range 1-65535."));
            return;
        }
        auto Candidate = MakeUnique<FBlueprintHttpService>();
        if (!Candidate->Start(Port))
        {
            UE_LOG(LogFactorySpawner, Warning, TEXT("Could not start FactorySpawner HTTP on 127.0.0.1:%d."), Port);
            return;
        }
        Service = MoveTemp(Candidate);
        UE_LOG(LogFactorySpawner, Display, TEXT("FactorySpawner HTTP listening on 127.0.0.1:%d."), Port);
    }
    Service->Attach(Actor);
}

void BlueprintHttpServer::Detach(AFactorySpawnerChat* Actor)
{
    if (Service && Service->IsAttachedTo(Actor))
        Service.Reset();
}

void BlueprintHttpServer::Shutdown()
{
    Service.Reset();
}
