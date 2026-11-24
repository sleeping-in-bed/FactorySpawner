#include "FactorySpawnerChat.h"
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
    FFactorySpawnerModule::ChatLog(
        GetWorld(),
        TEXT("FactorySpawner loaded! Tool to generate commands: https://uniquesimon.github.io/satisfactory-planner/"));
}

void AFactorySpawnerChat::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
                                  "[compact], [vertical], [maxHeight metres], [noFoundations], [noSigns]. "
                                  "Either form accepts , name Blueprint Name"));
}

EExecutionStatus AFactorySpawnerChat::ExecuteCommand_Implementation(UCommandSender* Sender,
                                                                    const TArray<FString>& Arguments,
                                                                    const FString& Label)
{
    FString Joined = FString::Join(Arguments, TEXT(" "));
    Sender->SendChatMessage(FString::Printf(TEXT("/FactorySpawner %s"), *Joined), FLinearColor::Green);

    FFactoryCommand Command;
    FString Error;
    FString CustomName;
    if (!ExtractBlueprintName(Joined, CustomName, Error))
    {
        Sender->SendChatMessage(Error, FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    FProductionRequest Request;
    const bool bProduction = Joined.StartsWith(TEXT("plan ")) || Joined.StartsWith(TEXT("build "));
    const bool bParsed = bProduction ? ParseProductionRequest(Joined, Request, Error)
                                      : FFactoryCommandParser::ParseCommand(Joined, Command, Error);
    if (!bParsed)
    {
        UE_LOG(LogFactorySpawner, Warning, TEXT("%s"), *Error);
        Sender->SendChatMessage(Error);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    UWorld* World = GetWorld();

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

    Sender->SendChatMessage(FString::Printf(TEXT("Using Belt Tier: Mk%d, Pipeline Tier: Mk%d"), BeltTier, PipelineTier),
                            FLinearColor::Gray);

    FProductionPlan Plan;
    if (bProduction)
    {
        if (!BuildProductionPlan(World, BuildableCache, Request, BeltTier, PipelineTier, Command, Plan, Error))
        {
            Sender->SendChatMessage(Error);
            return EExecutionStatus::UNCOMPLETED;
        }
        const FString Summary = DescribeProduction(Command, Plan);
        UE_LOG(LogFactorySpawner, Display, TEXT("%s"), *Summary);
        Sender->SendChatMessage(Summary, FLinearColor::Gray);
    }

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

    if (bProduction && Request.bPreview)
    {
        Sender->SendChatMessage(FString::Printf(TEXT("Blueprint name: %s"), *Command.BlueprintName), FLinearColor::Gray);
        return EExecutionStatus::COMPLETED;
    }

    if (!FBuildPlanGenerator(World, BuildableCache).Generate(Command, Error, bProduction ? &Plan : nullptr))
    {
        UE_LOG(LogFactorySpawner, Warning, TEXT("%s"), *Error);
        Sender->SendChatMessage(Error);
        return EExecutionStatus::UNCOMPLETED;
    }
    UE_LOG(LogFactorySpawner, Display, TEXT("Blueprint generated: %s (%d rows)"),
           *Command.BlueprintName, Command.Rows.Num());
    Sender->SendChatMessage(FString::Printf(TEXT("Blueprint generated: %s"), *Command.BlueprintName), FLinearColor::Green);
    return EExecutionStatus::COMPLETED;
}
