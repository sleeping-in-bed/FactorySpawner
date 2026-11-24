#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "BuildPlanTypes.h"
#include "FactorySpawnerChat.generated.h"

class UBuildPlanGenerator;
class UBuildableCache;

struct FBlueprintCommandResult
{
    FString Name;
    FString Path;
    FString Summary;
    FString MaterialSummary;
    FString Error;
    bool bPreview = false;
};

UCLASS()
class FACTORYSPAWNER_API AFactorySpawnerChat : public AChatCommandInstance
{
    GENERATED_BODY()

  public:
    AFactorySpawnerChat();

    // Static helper to find the subsystem instance in the current world
    static AFactorySpawnerChat* Get(UWorld* World);

    void BeginPlay() override;
    void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    EExecutionStatus ExecuteCommand_Implementation(class UCommandSender* Sender, const TArray<FString>& Arguments,
                                                   const FString& Label) override;

    EExecutionStatus RunBlueprintCommand(FString CommandText, FBlueprintCommandResult& Result,
                                         UCommandSender* Sender = nullptr);

    /** Clears current data (useful when changing savegames) */
    void ResetSubsystemData();

  private:
    /** Cache for buildables and recipes (world-specific) */
    UPROPERTY()
    UBuildableCache* BuildableCache;
};
