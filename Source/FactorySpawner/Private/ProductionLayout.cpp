#include "BuildPlanGenerator.h"
#include "BuildableCache.h"
#include "Buildables/FGBuildable.h"
#include "Engine/World.h"
#include "ProductionPlan.h"
#include "Algo/Unique.h"

namespace
{
    constexpr double TallMachineHeight = 1200.0;
    constexpr double DeckHeightStep = 100.0;
    constexpr double OverheadClearance = 150.0;
    constexpr double RowSideMargin = 160.0;
    constexpr double AisleMargin = 250.0;
    constexpr double AisleInset = 100.0;

    double CompactLayoutCost(FVector2D Size, double Corridor, double Height, double RouteCost)
    {
        // Compare the ground footprint, allowing extra space for vertical access to higher decks.
        const double Allowance = Corridor + Height;
        return (Size.X + Allowance) * (Size.Y + Allowance) * (1.0 + RouteCost);
    }

    struct FRowFootprint
    {
        int32 Index;
        double Width;
        double Front;
        double Back;
    };

    struct FRowBand
    {
        TArray<const FRowFootprint*> Rows;
        double Width = 0;
        double Front = 0;
        double Back = 0;
    };
}

bool FBuildPlanGenerator::PlanProductionLayout(const FFactoryCommand& Command, TArray<FPreparedRow>& Rows,
                                               FString& OutError)
{
    TMap<EBuildable, FBox> MachineBounds;
    TArray<int32> EquipmentStages;
    TSet<int32> ExplicitFloors;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const auto Type = Command.Rows[Index].MachineType;
        if (!MachineBounds.Contains(Type))
        {
            const auto Class = Cache->GetBuildableClass<AFGBuildable>(Type);
            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Probe = Class ? World->SpawnActor<AFGBuildable>(Class, FTransform::Identity, Spawn) : nullptr;
            if (!IsValid(Probe))
            {
                OutError = FString::Printf(TEXT("Cannot measure machine size for row %d"), Index + 1);
                return false;
            }
            FBox Bounds = Probe->GetComponentsBoundingBox(true);
            if (bCompact)
            {
                TArray<FFGClearanceData> Clearances;
                IFGClearanceInterface::Execute_GetClearanceData(Probe, Clearances);
                for (const auto& Clearance : Clearances)
                    if (Clearance.IsValid() && Clearance.Type != EClearanceType::CT_Soft)
                        Bounds += Clearance.GetTransformedClearanceBox();
            }
            Probe->Destroy();
            if (!Bounds.IsValid || !FMath::IsFinite(Bounds.Max.Z) || Bounds.Max.Z <= 0)
            {
                OutError = FString::Printf(TEXT("No valid machine height for row %d"), Index + 1);
                return false;
            }
            MachineBounds.Add(Type, Bounds);
        }
        if (Production->Rows[Index].bFixedFloor)
            ExplicitFloors.Add(Rows[Index].Floor);
        else if (MachineBounds.FindChecked(Type).Max.Z > TallMachineHeight)
            EquipmentStages.Add(Rows[Index].Floor);
    }

    int32 EquipmentFloor = INDEX_NONE;
    if (!EquipmentStages.IsEmpty())
    {
        EquipmentStages.Sort();
        EquipmentFloor = EquipmentStages[EquipmentStages.Num() / 2];
        while (ExplicitFloors.Contains(EquipmentFloor))
            ++EquipmentFloor;
    }

    const double Thickness = Command.bNoFoundations ? 0.0 : FoundationThickness;
    const double SideMargin = bCompact ? 90.0 : RowSideMargin;
    const double HeightAllowance = bCompact ? 75.0 : OverheadClearance;
    const double Grid = bCompact && Command.bNoFoundations ? 100.0 : FoundationSize;
    TArray<FRowFootprint> Geometry;
    TArray<double> Heights;
    TArray<int32> BaseFloors;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        auto& Row = Rows[Index];
        const FBox& Bounds = MachineBounds.FindChecked(Command.Rows[Index].MachineType);
        const double Height = Bounds.Max.Z;
        if (EquipmentFloor != INDEX_NONE && !Production->Rows[Index].bFixedFloor)
        {
            if (Height > TallMachineHeight)
                Row.Floor = EquipmentFloor;
            else if (Row.Floor >= EquipmentFloor)
                ++Row.Floor;
        }
        BaseFloors.Add(Row.Floor);
        const int32 Count = Command.Rows[Index].Count;
        const bool bDirect = bCompact && Count == 1;
        const double HalfLength = bCompact
            ? FMath::Max3(Row.Machine->Length * 50.0, FMath::Abs(Bounds.Min.Y), FMath::Abs(Bounds.Max.Y))
            : Row.Machine->Length * 50.0;
        double Top = Height;
        double PortWidth = 0;
        for (const FMachineConnections* Ports : {Row.Input, Row.Output})
        {
            for (const FConnector& Connector : Ports->Belt)
            {
                Top = FMath::Max(Top, 100.0 + Connector.LocationY * 100.0);
                PortWidth = FMath::Max(PortWidth, 2 * FMath::Abs(Connector.LocationX) * 100.0 + 140.0);
            }
            for (const FConnector& Connector : Ports->Pipe)
            {
                Top = FMath::Max(Top, 175.0 + Connector.LocationY * 100.0);
                PortWidth = FMath::Max(PortWidth, 2 * FMath::Abs(Connector.LocationX) * 100.0 + 140.0);
            }
        }
        const double Width = 2 * FMath::Max(FMath::Abs(Bounds.Min.X), FMath::Abs(Bounds.Max.X));
        Row.MachinePitch = bCompact ? FMath::Max(Width + 90.0, PortWidth) : Row.Machine->Width * 100.0;
        const double Clearance = bCompact ? (Row.Input->Pipe.Num() || Row.Output->Pipe.Num() ? 150.0 : 120.0)
                                          : AisleMargin;
        Geometry.Add({Index, Row.MachinePitch * Count + 2 * SideMargin,
            FMath::Max(HalfLength, bDirect ? 0.0 : Row.Input->Length * 100.0) + Clearance,
            FMath::Max(HalfLength, bDirect ? 0.0 : Row.Output->Length * 100.0) + Clearance});
        Heights.Add(Top);
    }

    TArray<TPair<int32, int32>> MaterialLinks;
    if (bCompact)
    {
        TMap<FString, int32> LastProducer;
        for (int32 Index = 0; Index < Production->Rows.Num(); ++Index)
        {
            const auto& Row = Production->Rows[Index];
            for (const auto& Input : Row.Inputs)
                if (const int32* Source = LastProducer.Find(Input.Item))
                    MaterialLinks.Emplace(*Source, Index);
            for (const auto& Output : Row.Outputs)
                LastProducer.Add(Output.Item, Index);
        }
    }

    const auto Pack = [&](const TArray<int32>& Assignment, bool bAlternate, bool bKeepOrder,
                          TMap<int32, FFloorLayout>& Layout, TArray<FPreparedRow>& Placement)
    {
        TMap<int32, TArray<FRowFootprint>> FloorRows;
        for (int32 Index = 0; Index < Placement.Num(); ++Index)
        {
            Placement[Index].Floor = Assignment[Index];
            FloorRows.FindOrAdd(Assignment[Index]).Add(Geometry[Index]);
            auto& Floor = Layout.FindOrAdd(Assignment[Index]);
            Floor.bEquipment |= MachineBounds.FindChecked(Command.Rows[Index].MachineType).Max.Z > TallMachineHeight;
            const double Rise = FMath::CeilToDouble((Heights[Index] + HeightAllowance + Thickness) / DeckHeightStep)
                              * DeckHeightStep;
            Floor.RequiredRise = FMath::Max3(Floor.RequiredRise, Rise, FloorHeight);
        }

        TArray<int32> FloorNumbers;
        Layout.GetKeys(FloorNumbers);
        FloorNumbers.Sort();
        FBox2D Envelope(ForceInit);
        double DeckZ = 0;
        double TopZ = 0;
        for (const int32 Number : FloorNumbers)
        {
            auto& Floor = Layout.FindChecked(Number);
            Floor.DeckZ = DeckZ;
            DeckZ += Floor.RequiredRise;
            auto& Footprints = FloorRows.FindChecked(Number);
            if (!bKeepOrder)
                Footprints.StableSort([](const FRowFootprint& A, const FRowFootprint& B)
                {
                    return A.Front + A.Back > B.Front + B.Back;
                });

            double TotalArea = 0;
            double Widest = 0;
            double SumWidths = 0;
            for (const auto& Row : Footprints)
            {
                TotalArea += Row.Width * (Row.Front + Row.Back);
                Widest = FMath::Max(Widest, Row.Width);
                SumWidths += Row.Width;
                TopZ = FMath::Max(TopZ, Floor.DeckZ + Heights[Row.Index]);
            }
            const int32 MinWidth = FMath::CeilToInt(Widest / Grid);
            const int32 MaxWidth = FMath::Max(MinWidth, FMath::CeilToInt(
                FMath::Min(SumWidths, 2.0 * FMath::Sqrt(TotalArea)) / Grid));
            double BestCost = TNumericLimits<double>::Max();
            TArray<FRowBand> BestBands;
            // Sample widths on large floors to bound the search without spawning trial layouts.
            const int32 Step = bCompact ? FMath::Max(1, FMath::DivideAndRoundUp(MaxWidth - MinWidth, 22)) : 1;
            for (int32 Width = MinWidth; ; Width = FMath::Min(Width + Step, MaxWidth))
            {
                TArray<FRowBand> Bands;
                for (const auto& Row : Footprints)
                {
                    int32 BandIndex = Bands.IndexOfByPredicate([&](const FRowBand& Candidate)
                    {
                        return Candidate.Width + Row.Width <= Width * Grid;
                    });
                    if (BandIndex == INDEX_NONE)
                        BandIndex = Bands.AddDefaulted();
                    auto& Band = Bands[BandIndex];
                    const bool bTurned = bAlternate && BandIndex % 2 == 1;
                    Band.Rows.Add(&Row);
                    Band.Width += Row.Width;
                    Band.Front = FMath::Max(Band.Front, bTurned ? Row.Back : Row.Front);
                    Band.Back = FMath::Max(Band.Back, bTurned ? Row.Front : Row.Back);
                }

                double UsedWidth = 0;
                double UsedLength = 0;
                for (const auto& Band : Bands)
                {
                    UsedWidth = FMath::Max(UsedWidth, Band.Width);
                    UsedLength += Band.Front + Band.Back;
                }
                UsedWidth = FMath::CeilToDouble(UsedWidth / Grid);
                UsedLength = FMath::CeilToDouble(UsedLength / Grid);
                const double Cost = UsedWidth * UsedLength +
                    (bCompact ? 0.15 : 1.0) * FMath::Square(UsedWidth - UsedLength);
                if (Cost < BestCost)
                {
                    BestCost = Cost;
                    BestBands = MoveTemp(Bands);
                }
                if (Width == MaxWidth)
                    break;
            }

            double Y = 0;
            for (int32 BandIndex = 0; BandIndex < BestBands.Num(); ++BandIndex)
            {
                const auto& Band = BestBands[BandIndex];
                const bool bTurned = bAlternate && BandIndex % 2 == 1;
                double X = 0;
                for (const FRowFootprint* Footprint : Band.Rows)
                {
                    auto& Row = Placement[Footprint->Index];
                    Row.bTurned = bTurned;
                    const double FirstX = SideMargin + Row.MachinePitch / 2.0;
                    Row.Position = FVector2D(X + (bTurned ? Footprint->Width - FirstX : FirstX), Y + Band.Front);
                    const double FrontAisle = Y + (bCompact ? 45.0 : AisleInset);
                    const double BackAisle = Y + Band.Front + Band.Back - (bCompact ? 45.0 : AisleInset);
                    Row.InputAisleY = bTurned ? BackAisle : FrontAisle;
                    Row.OutputAisleY = bTurned ? FrontAisle : BackAisle;
                    X += Footprint->Width;
                }
                Floor.Footprint += FVector2D(0, Y);
                Y += Band.Front + Band.Back;
                Floor.Footprint += FVector2D(X, Y);
            }
            Envelope += Floor.Footprint;
        }
        const FVector2D Size = Envelope.GetSize();
        // Include space at the perimeter for terminals and corridors, and account for layouts with more lifts.
        const double Corridor = 1000.0 + 350.0 * FloorNumbers.Num();
        double MaterialDistance = 0.0;
        for (const auto& Link : MaterialLinks)
        {
            const auto& Source = Placement[Link.Key];
            const auto& Sink = Placement[Link.Value];
            MaterialDistance += FMath::Abs(Source.Position.X - Sink.Position.X)
                + FMath::Abs(Source.OutputAisleY - Sink.InputAisleY)
                + 2.0 * FMath::Abs(Layout.FindChecked(Source.Floor).DeckZ - Layout.FindChecked(Sink.Floor).DeckZ);
        }
        const double RouteCost = MaterialLinks.IsEmpty() ? 0.0
            : 0.12 * MaterialDistance / (MaterialLinks.Num() * (Size.X + Size.Y + Corridor));
        const double Height = TopZ + Thickness;
        return bCompact ? CompactLayoutCost(Size, Corridor, Height, RouteCost)
                        : (Size.X + Corridor) * (Size.Y + Corridor) * Height;
    };

    TArray<FPreparedRow> BestRows = Rows;
    double BestScore = Pack(BaseFloors, false, bCompact, Floors, BestRows);
    if (bCompact)
    {
        const auto Consider = [&](const TArray<int32>& Assignment, bool bTurned, bool bKeepOrder)
        {
            TMap<int32, FFloorLayout> Layout;
            TArray<FPreparedRow> Placement = Rows;
            const double Score = Pack(Assignment, bTurned, bKeepOrder, Layout, Placement);
            if (Score < BestScore)
            {
                BestScore = Score;
                Floors = MoveTemp(Layout);
                BestRows = MoveTemp(Placement);
            }
        };
        Consider(BaseFloors, true, false);
        if (ExplicitFloors.IsEmpty())
        {
            TArray<int32> Stages = BaseFloors;
            Stages.Sort();
            Stages.SetNum(Algo::Unique(Stages));
            for (const int32 GroupSize : {2, FMath::Max(2, Stages.Num())})
            {
                TArray<int32> Grouped;
                for (const int32 Floor : BaseFloors)
                {
                    const int32 Stage = Stages.IndexOfByKey(Floor);
                    const int32 EquipmentStage = Stages.IndexOfByKey(EquipmentFloor);
                    if (EquipmentStage == INDEX_NONE || Stage < EquipmentStage)
                        Grouped.Add(Stage / GroupSize);
                    else if (Stage == EquipmentStage)
                        Grouped.Add(Stages.Num());
                    else
                        Grouped.Add(Stages.Num() + 1 + (Stage - EquipmentStage - 1) / GroupSize);
                }
                Consider(Grouped, true, true);
            }
        }
    }
    Rows = MoveTemp(BestRows);
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactorySpawnerCompactLayoutCost, "FactorySpawner.Production.CompactLayoutCost",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFactorySpawnerCompactLayoutCost::RunTest(const FString&)
{
    const double LargeFlat = CompactLayoutCost({17600, 16800}, 1350, 1070, 0.04);
    const double LargeStacked = CompactLayoutCost({14000, 13000}, 1700, 2280, 0.07);
    TestTrue(TEXT("A smaller ground footprint can justify stacking a large factory"), LargeStacked < LargeFlat);

    const double SmallFlat = CompactLayoutCost({4400, 4400}, 1350, 1070, 0.04);
    const double SmallStacked = CompactLayoutCost({3800, 3800}, 1700, 2280, 0.07);
    TestTrue(TEXT("A small footprint reduction does not justify another floor"), SmallFlat < SmallStacked);
    return true;
}

#endif
