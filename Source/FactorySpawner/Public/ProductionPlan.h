#pragma once

#include "BuildPlanTypes.h"

class UBuildableCache;
class UFGItemDescriptor;
class UFGRecipe;

struct FProductionAmount
{
    FString Item;
    double PerMinute;
};

struct FProductionRecipe
{
    FString Id;
    EBuildable Machine;
    TArray<FProductionAmount> Inputs;
    TArray<FProductionAmount> Outputs;
    bool bAlternate = false;
};

struct FProductionRequest
{
    TMap<FString, double> Targets;
    TSet<FString> Imports;
    TSet<FString> ExportByproducts;
    TMap<FString, FString> RecipeChoices;
    TMap<FString, int32> Floors;
    TOptional<int32> BeltTier;
    TOptional<int32> MaxHeightMeters;
    TOptional<FString> MinMaterialsItem; // An empty item selects minimum total construction cost.
    int32 FloorHeight = 0;
    bool bPreview = false;
    bool bCompact = false;
    bool bNoFoundations = false;
    bool bNoSigns = false;
    bool bNoValves = false;
    bool bBurnResidualFuel = false;
};

struct FProductionOrder
{
    int32 RecipeIndex;
    double Potential;
    int32 Floor;
};

struct FProductionRow
{
    TSubclassOf<UFGRecipe> Recipe;
    bool bFixedFloor = false;
    TArray<FProductionAmount> Inputs;
    TArray<FProductionAmount> Outputs;
};

struct FProductionPlan
{
    TArray<FProductionRow> Rows;
    FString ProductLabel;
    TMap<FString, TSubclassOf<UFGItemDescriptor>> Items;
    TSet<FString> ExportByproducts;
    TSet<FString> RequestedProducts;
    TMap<FString, double> Imports;
    TMap<FString, double> Exports;
    double BeltCapacity = 60;
    double PipeCapacity = 300;
    double PeakPowerMW = 0;
    double ResidueToBurn = 0;
    double InternalFuelRate = 0;
    double FuelLimitedPowerMW = 0;
    TOptional<int32> MaxHeightMeters;
    TSubclassOf<UFGItemDescriptor> PreferredMaterial;
    bool bMinimizeMaterials = false;
    bool bNoValves = false;
};

bool ParseProductionRequest(const FString& Command, FProductionRequest& Out, FString& Error);
bool SolveProduction(const TArray<FProductionRecipe>& Recipes, const TSet<FString>& RawResources,
                     const FProductionRequest& Request, TArray<FProductionOrder>& Orders,
                     TMap<FString, double>& Imports, TMap<FString, double>& Exports, FString& Error);
bool BuildProductionPlan(UWorld* World, UBuildableCache* Cache, FProductionRequest Request,
                         int32 BeltTier, int32 PipeTier, FFactoryCommand& Command, FProductionPlan& Plan, FString& Error);
FString DescribeProduction(const FFactoryCommand& Command, const FProductionPlan& Plan);
