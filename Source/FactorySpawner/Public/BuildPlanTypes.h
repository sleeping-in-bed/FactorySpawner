#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EBuildable : uint8
{
    // Machines
    Smelter,
    Constructor,
    Assembler,
    Foundry,
    Manufacturer,
    OilRefinery,
    Blender,
    Converter,
    ParticleAccelerator,
    QuantumEncoder,
    Packager,

    // Generators
    BiomassGenerator,
    CoalGenerator,
    FuelGenerator,
    NuclearReactor,

    // Utility
    Splitter,
    Merger,
    PowerPole,
    Foundation,
    Pipeline,
    Pipeline2,
    PipeCross,
    PipePump,
    PipePump2,
    PipeValve,
    SmartSplitter,
    PortSign,

    // Transport
    Belt,
    Lift,
    PowerLine,

    Invalid
};

inline double BeltItemsPerMinute(int32 Tier)
{
    constexpr double Rates[] = {60, 120, 270, 480, 780, 1200};
    return Rates[FMath::Clamp(Tier, 1, static_cast<int32>(UE_ARRAY_COUNT(Rates))) - 1];
}

struct FFactoryCommandToken
{
    int32 Count = 0;
    EBuildable MachineType;
    TOptional<FString> Recipe;
    TOptional<float> ClockPercent; // percent value (e.g. 75.5)
    int32 Floor = 1;
};

struct FFactoryCommand
{
    TArray<FFactoryCommandToken> Rows;
    FString BlueprintName;
    TOptional<int32> BeltTier; // optional belt tier override (1-6 for Mk1-Mk6)
    TOptional<FString> GeneratorFuel;
    int32 FloorHeightMeters = 32;
    bool bCompact = false;
    bool bNoFoundations = false;
};
