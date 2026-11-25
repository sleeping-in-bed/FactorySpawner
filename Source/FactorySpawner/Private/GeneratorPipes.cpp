#include "BuildPlanGenerator.h"
#include "BuildableCache.h"
#include "FactorySpawner.h"
#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildablePipeline.h"
#include "FGPipeConnectionComponent.h"
#include "Resources/FGItemDescriptor.h"

bool FBuildPlanGenerator::PlanGeneratorPipes(const FFactoryCommand& Command, FString& OutError)
{
    TArray<int32> Indices;
    bool bHasFuelGenerators = false;
    for (int32 Index = 0; Index < Command.Rows.Num(); ++Index)
    {
        const EBuildable Type = Command.Rows[Index].MachineType;
        bHasFuelGenerators |= Type == EBuildable::FuelGenerator;
        if (Type == EBuildable::FuelGenerator || Type == EBuildable::CoalGenerator || Type == EBuildable::NuclearReactor)
            Indices.Add(Index);
    }
    if (Command.GeneratorFuel && !bHasFuelGenerators)
    {
        OutError = TEXT("The fuel option requires a FuelGenerator row.");
        return false;
    }
    if (Indices.IsEmpty())
        return true;

    if (bHasFuelGenerators)
    {
        static const TMap<FString, FString> FuelNames = {
            {TEXT("fuel"), TEXT("Desc_LiquidFuel_C")}, {TEXT("turbofuel"), TEXT("Desc_LiquidTurboFuel_C")},
            {TEXT("liquidbiofuel"), TEXT("Desc_LiquidBiofuel_C")}, {TEXT("rocketfuel"), TEXT("Desc_RocketFuel_C")},
            {TEXT("ionizedfuel"), TEXT("Desc_IonizedFuel_C")}};
        const FString* AssetName = FuelNames.Find(Command.GeneratorFuel.Get(TEXT("Fuel")).ToLower());
        const auto Class = Cache->GetBuildableClass<AFGBuildableGeneratorFuel>(EBuildable::FuelGenerator);
        if (AssetName && Class)
        {
            const auto Fuels = Class.GetDefaultObject()->GetDefaultFuelClasses();
            if (const auto* Fuel = Fuels.FindByPredicate([&](const auto& Entry) { return Entry.GetAssetName() == *AssetName; }))
                GeneratorFuel = Fuel->LoadSynchronous();
        }
        if (!GeneratorFuel || UFGItemDescriptor::GetEnergyValue(GeneratorFuel) <= 0.0)
        {
            OutError = TEXT("Unsupported fuel. Use Fuel, Turbofuel, LiquidBiofuel, RocketFuel or IonizedFuel.");
            return false;
        }
    }

    const auto Pipe = Cache->GetBuildableClass<AFGBuildablePipeline>(EBuildable::Pipeline);
    if (!Pipe || Pipe.GetDefaultObject()->GetFlowLimit() <= 0.0)
    {
        OutError = TEXT("The selected pipeline has no valid flow limit.");
        return false;
    }
    const double Capacity = 60.0 * Pipe.GetDefaultObject()->GetFlowLimit();
    Indices.StableSort([&](int32 A, int32 B) { return Command.Rows[A].Floor < Command.Rows[B].Floor; });
    GeneratorRowLanes.SetNum(Command.Rows.Num());
    TMap<UClass*, int32> LastLaneByFluid;
    for (const int32 Index : Indices)
    {
        const auto& Row = Command.Rows[Index];
        const auto Class = Cache->GetBuildableClass<AFGBuildableGeneratorFuel>(Row.MachineType);
        if (!Class)
        {
            OutError = TEXT("Could not load a generator to calculate its full-load flow.");
            return false;
        }
        const auto* Defaults = Class.GetDefaultObject();
        const bool bFuel = Row.MachineType == EBuildable::FuelGenerator;
        TSubclassOf<UFGItemDescriptor> Item = bFuel ? GeneratorFuel : Defaults->GetSupplementalResourceClass();
        const double Rate = bFuel ? 60.0 * Defaults->GetDefaultPowerProductionCapacity() /
                              (1000.0 * UFGItemDescriptor::GetEnergyValue(GeneratorFuel))
                                  : 60.0 * Defaults->GetSupplementalConsumptionRateMaximum();
        if (!Item || !FMath::IsFinite(Rate) || Rate <= 0.0 || Rate > Capacity + KINDA_SMALL_NUMBER)
        {
            OutError = TEXT("A generator's full-load fluid demand is invalid or exceeds one pipeline.");
            return false;
        }

        int32* LastLane = LastLaneByFluid.Find(Item.Get());
        int32 Lane = LastLane ? *LastLane : INDEX_NONE;
        for (int32 Machine = 0; Machine < Row.Count; ++Machine)
        {
            if (Lane == INDEX_NONE || GeneratorPipeLanes[Lane].Rate + Rate > Capacity + KINDA_SMALL_NUMBER)
                Lane = GeneratorPipeLanes.Add({Item});
            GeneratorPipeLanes[Lane].Rate += Rate;
            GeneratorRowLanes[Index].Add(Lane);
        }
        LastLaneByFluid.Add(Item.Get(), Lane);
    }
    return true;
}

bool FBuildPlanGenerator::BuildGeneratorPipes(FString& OutError)
{
    if (GeneratorPipeLanes.IsEmpty())
        return true;

    double GroundZ = TNumericLimits<double>::Max();
    for (const auto& Floor : Floors)
        GroundZ = FMath::Min(GroundZ, Floor.Value.DeckZ);

    const FVector PowerLocation(PlatformBounds.Min.X - 450.0, PlatformBounds.Min.Y - 700.0, GroundZ);
    auto* PowerEntry = SpawnPowerPole(PowerLocation);
    WirePowerRoute(PowerEntry, FirstMachinePole);
    CachedPowerConnections.FirstPole = PowerEntry;
    if (bBuildFailed || !AddPortLabel(PowerLocation + FVector(0.0, 0.0, 450.0), TEXT("POWER / PUMP STARTUP")))
    {
        OutError = TEXT("Could not build the generator power connection.");
        return false;
    }

    const auto TakePort = [](TArray<UFGPipeConnectionComponent*>& Ports, const FVector& Direction)
    {
        Ports.Sort([&](const UFGPipeConnectionComponent& A, const UFGPipeConnectionComponent& B)
        {
            return FVector::DotProduct(A.GetConnectorNormal(), Direction) < FVector::DotProduct(B.GetConnectorNormal(), Direction);
        });
        return Ports.Pop();
    };
    for (int32 Index = 0; Index < GeneratorPipeLanes.Num(); ++Index)
    {
        auto& Lane = GeneratorPipeLanes[Index];
        if (Lane.Sections.IsEmpty())
        {
            OutError = TEXT("A generator inlet has no pipe sections.");
            return false;
        }
        Lane.Sections.StableSort([](const UFGPipeConnectionComponent& A, const UFGPipeConnectionComponent& B)
        {
            return A.GetComponentLocation().Z < B.GetComponentLocation().Z;
        });
        const double ShaftX = PlatformBounds.Min.X - 1600.0 - 650.0 * Index;
        const FVector Entry(ShaftX, PlatformBounds.Min.Y - 1150.0, GroundZ + 175.0);
        const FString Label = FString::Printf(TEXT("IN %d | %s %.2f m3/min"), Index + 1,
            *UFGItemDescriptor::GetItemName(Lane.Item).ToString(), Lane.Rate);
        auto Ports = SpawnPipeCross(Entry);
        if (Ports.Num() != 4 || !AddPortLabel(Entry + FVector(0.0, -200.0, 300.0), Label))
        {
            OutError = TEXT("Could not build an external generator fluid inlet.");
            return false;
        }
        auto* Previous = TakePort(Ports, FVector::RightVector);
        // External feed must reach the inlet and provide enough residual head for the first pump.
        double HeadCeiling = Previous->GetComponentLocation().Z + 650.0;
        for (int32 SectionIndex = 0; SectionIndex < Lane.Sections.Num(); ++SectionIndex)
        {
            auto* Section = Lane.Sections[SectionIndex];
            const FVector Target = Section->GetComponentLocation();
            const FVector Junction(ShaftX, Target.Y - 350.0 - 270.0 * Index, Target.Z);
            const FVector Approach = Previous->GetComponentLocation() - Junction;
            const bool bVertical = FMath::Abs(Approach.Z) > FMath::Abs(Approach.Y) ||
                (Lane.Sections.IsValidIndex(SectionIndex + 1) &&
                 Lane.Sections[SectionIndex + 1]->GetComponentLocation().Z > Target.Z + 500.0);
            const FRotator Rotation = bVertical
                ? FRotationMatrix::MakeFromXY(FVector::UpVector, FVector::ForwardVector).Rotator() : FRotator::ZeroRotator;
            Ports = SpawnPipeCross(Junction, Rotation);
            if (Ports.Num() != 4)
            {
                OutError = TEXT("Could not build a generator fluid junction.");
                return false;
            }
            auto* Branch = TakePort(Ports, FVector::ForwardVector);
            auto* In = TakePort(Ports, Approach);
            const bool bTrunkConnected = ConnectPipeRoute(Previous, In, Lane.Item, 0.0, HeadCeiling);
            double BranchHead = HeadCeiling;
            if (!bTrunkConnected || !ConnectPipeRoute(Branch, Section, Lane.Item, 0.0, BranchHead))
            {
                OutError = FString::Printf(TEXT("Could not route generator fluid inlet %d, section %d."), Index + 1, SectionIndex + 1);
                return false;
            }
            if (Lane.Sections.IsValidIndex(SectionIndex + 1))
            {
                const FVector Next = Lane.Sections[SectionIndex + 1]->GetComponentLocation();
                Previous = TakePort(Ports, FVector(0.0, Next.Y - Target.Y, Next.Z - Target.Z));
            }
        }
        UE_LOG(LogFactorySpawner, Display, TEXT("Generator fluid inlet %d: %s, %.2f m3/min, %d sections"),
            Index + 1, *Lane.Item->GetName(), Lane.Rate, Lane.Sections.Num());
    }
    return true;
}
