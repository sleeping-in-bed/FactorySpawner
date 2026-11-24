#include "FactorySpawnerChat.h"
#include "BlueprintCommandFile.h"
#include "BlueprintHttpServer.h"
#include "FGBlueprintSubsystem.h"
#include "Misc/Paths.h"
#include "FGRecipe.h"
#include "Equipment/FGBuildGun.h"
#include "FGCharacterPlayer.h"
#include "Command/CommandSender.h"
#include "BlueprintNaming.h"
#include "BuildableCache.h"
#include "FactoryCommandParser.h"
#include "FactorySpawner.h"
#include "BuildPlanGenerator.h"
#include "ProductionPlan.h"
#include "EngineUtils.h"

AFactorySpawnerChat* AFactorySpawnerChat::Get(UWorld* World)
{
    for (TActorIterator<AFactorySpawnerChat> It(World); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void AFactorySpawnerChat::BeginPlay()
{
    Super::BeginPlay();

    if (!BuildableCache)
        BuildableCache = NewObject<UBuildableCache>(this);

    ResetSubsystemData();
    BlueprintHttpServer::Attach(this);
    FFactorySpawnerModule::ChatLog(
        GetWorld(),
        TEXT("FactorySpawner loaded! Tool to generate commands: https://uniquesimon.github.io/satisfactory-planner/"));
}

void AFactorySpawnerChat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    BlueprintHttpServer::Detach(this);
    ResetSubsystemData();
    BuildableCache = nullptr;

    Super::EndPlay(EndPlayReason);
}

void AFactorySpawnerChat::ResetSubsystemData()
{
    if (BuildableCache)
    {
        BuildableCache->ClearCache();
    }
}

AFactorySpawnerChat::AFactorySpawnerChat()
{
    CommandName = TEXT("FactorySpawner");
    MinNumberOfArguments = 2;
    Usage = FText::FromString(TEXT("/FactorySpawner <count> <machine> [recipe] [clock%] [floor=N], ... "
                                  "[beltTier N], [floorHeight metres], [noFoundations] or /FactorySpawner plan|build Item=rate, "
                                  "[compact], [vertical], [maxHeight metres], [noFoundations], [noSigns], "
                                  "[minMaterials [Item]]. "
                                  "Either form accepts , name Blueprint Name. Use /FactorySpawner run File.txt for a command file."));
}

EExecutionStatus AFactorySpawnerChat::ExecuteCommand_Implementation(UCommandSender* Sender,
                                                                    const TArray<FString>& Arguments,
                                                                    const FString& Label)
{
    FString Joined = FString::Join(Arguments, TEXT(" "));
    Sender->SendChatMessage(FString::Printf(TEXT("/FactorySpawner %s"), *Joined), FLinearColor::Green);

    FBlueprintCommandResult Result;
    return RunBlueprintCommand(MoveTemp(Joined), Result, Sender);
}

EExecutionStatus AFactorySpawnerChat::RunBlueprintCommand(FString Joined, FBlueprintCommandResult& Result,
                                                          UCommandSender* Sender)
{
    check(IsInGameThread());
    Result = {};
    const auto Reply = [Sender](const FString& Message, const FLinearColor& Color = FLinearColor::White)
    {
        if (Sender)
            Sender->SendChatMessage(Message, Color);
    };
    FString& Error = Result.Error;
    auto* World = GetWorld();
    auto* Blueprints = World ? AFGBlueprintSubsystem::Get(World) : nullptr;
    if (!BuildableCache || !Blueprints)
    {
        Error = TEXT("Open a saved game before generating a blueprint.");
        Reply(Error, FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const auto Normalize = [](FString& Text)
    {
        Text.ReplaceInline(TEXT("\r"), TEXT(" "));
        Text.ReplaceInline(TEXT("\n"), TEXT(" "));
        Text.ReplaceInline(TEXT("\t"), TEXT(" "));
        Text.TrimStartAndEndInline();
        if (!Text.RemoveFromStart(TEXT("/FactorySpawner "), ESearchCase::IgnoreCase))
            Text.RemoveFromStart(TEXT("FactorySpawner "), ESearchCase::IgnoreCase);
        Text.TrimStartInline();
    };
    Normalize(Joined);
    if (Joined.RemoveFromStart(TEXT("run "), ESearchCase::IgnoreCase))
    {
        if (GetNetMode() != NM_Standalone)
        {
            Error = TEXT("Command files are available in single-player sessions.");
            Reply(Error, FLinearColor::Red);
            return EExecutionStatus::BAD_ARGUMENTS;
        }
        const FString FileName = Joined.TrimStartAndEnd();
        if (!LoadBlueprintCommandFile(FileName, Joined, Error))
        {
            Reply(Error, FLinearColor::Red);
            return EExecutionStatus::BAD_ARGUMENTS;
        }
        Normalize(Joined);
    }

    FFactoryCommand Command;
    FString CustomName;
    if (!ExtractBlueprintName(Joined, CustomName, Error))
    {
        Reply(Error, FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    FProductionRequest Request;
    const bool bProduction = Joined.StartsWith(TEXT("plan "), ESearchCase::IgnoreCase) || Joined.StartsWith(TEXT("build "), ESearchCase::IgnoreCase);
    const bool bParsed = bProduction ? ParseProductionRequest(Joined, Request, Error)
                                      : FFactoryCommandParser::ParseCommand(Joined, Command, Error);
    if (!bParsed)
    {
        UE_LOG(LogFactorySpawner, Warning, TEXT("%s"), *Error);
        Reply(Error, FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }


    // Determine belt tier to use
    int32 BeltTier = 1;
    const TOptional<int32>& RequestedTier = bProduction ? Request.BeltTier : Command.BeltTier;
    if (RequestedTier.IsSet())
    {
        // Use explicitly provided belt tier from inline parameter
        BeltTier = RequestedTier.GetValue();
    }
    else
    {
        // Auto-detect highest unlocked belt tier
        BeltTier = BuildableCache->GetHighestUnlockedBeltTier(World);
    }

    BuildableCache->SetBeltClass(BeltTier);
    BuildableCache->SetLiftClass(BeltTier);

    // Auto-detect pipeline tier
    int32 PipelineTier = BuildableCache->GetHighestUnlockedPipelineTier(World);
    BuildableCache->SetPipelineClass(PipelineTier);

    Reply(FString::Printf(TEXT("Using Belt Tier: Mk%d, Pipeline Tier: Mk%d"), BeltTier, PipelineTier),
                            FLinearColor::Gray);

    FProductionPlan Plan;
    if (bProduction)
    {
        if (!BuildProductionPlan(World, BuildableCache, Request, BeltTier, PipelineTier, Command, Plan, Error))
        {
            Reply(Error, FLinearColor::Red);
            return EExecutionStatus::UNCOMPLETED;
        }
        Result.Summary = DescribeProduction(Command, Plan);
        UE_LOG(LogFactorySpawner, Display, TEXT("%s"), *Result.Summary);
        Reply(Result.Summary, FLinearColor::Gray);
    }

    Command.BeltTier = BeltTier;
    Command.BlueprintName = MoveTemp(CustomName);
    if (Command.BlueprintName.IsEmpty())
    {
        FString BlueprintLabel = Plan.ProductLabel;
        if (!bProduction)
        {
            TArray<FString> Labels;
            for (const FFactoryCommandToken& Row : Command.Rows)
            {
                const FString Machine = StaticEnum<EBuildable>()->GetNameStringByValue(static_cast<int64>(Row.MachineType));
                FString RowLabel = FString::Printf(TEXT("%d %s"), Row.Count, *Machine);
                if (Row.Recipe.IsSet())
                    RowLabel += TEXT(" ") + Row.Recipe.GetValue();
                if (Row.Floor > 1)
                    RowLabel += FString::Printf(TEXT(" F%d"), Row.Floor);
                Labels.Add(MoveTemp(RowLabel));
            }
            BlueprintLabel = FString::Join(Labels, TEXT(" - "));
        }
        const FString Configuration = bProduction ? Joined.Mid(Joined.Find(TEXT(" ")) + 1) : Joined;
        Command.BlueprintName = MakeAutomaticBlueprintName(BlueprintLabel, Configuration);
    }

    Result.Name = Command.BlueprintName;
    Result.bPreview = bProduction && Request.bPreview;
    if (Result.bPreview)
    {
        Reply(FString::Printf(TEXT("Blueprint name: %s"), *Command.BlueprintName), FLinearColor::Gray);
        if (!Plan.bMinimizeMaterials)
            return EExecutionStatus::COMPLETED;
    }

    if (Plan.bMinimizeMaterials)
        Reply(TEXT("Comparing construction materials for candidate layouts..."), FLinearColor::Gray);
    FBuildPlanGenerator Generator(World, BuildableCache);
    if (!Generator.Generate(Command, Error, bProduction ? &Plan : nullptr, !Result.bPreview))
    {
        UE_LOG(LogFactorySpawner, Warning, TEXT("%s"), *Error);
        Reply(Error, FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }
    Result.MaterialSummary = Generator.GetMaterialSummary();
    if (!Result.MaterialSummary.IsEmpty())
        Reply(Result.MaterialSummary, FLinearColor::Gray);
    if (Result.bPreview)
        return EExecutionStatus::COMPLETED;

    Result.Path = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Blueprints->GetSessionBlueprintPath(), Result.Name + TEXT(".sbp")));

    UE_LOG(LogFactorySpawner, Display, TEXT("Blueprint generated: %s (%d rows)"),
           *Command.BlueprintName, Command.Rows.Num());
    Reply(FString::Printf(TEXT("Blueprint generated: %s"), *Command.BlueprintName), FLinearColor::Green);
    return EExecutionStatus::COMPLETED;
}
