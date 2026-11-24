#include "FactoryCommandParser.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "BlueprintNaming.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactorySpawnerFloorsTest, "FactorySpawner.Command.Floors",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFactorySpawnerFloorsTest::RunTest(const FString&)
{
    FFactoryCommand Command;
    FString Error;
    const bool bParsed = FFactoryCommandParser::ParseCommand(
        TEXT("2 Smelter IngotIron 75% floor=3, 3 Constructor IronPlate FLOOR=2, 1 Assembler, floorHeight 48, beltTier 3"),
        Command, Error);
    if (!TestTrue(*Error, bParsed) || !TestEqual(TEXT("Row count"), Command.Rows.Num(), 3))
        return false;

    TestEqual(TEXT("Source floor"), Command.Rows[0].Floor, 3);
    TestEqual(TEXT("Destination floor"), Command.Rows[1].Floor, 2);
    TestEqual(TEXT("Unspecified floor"), Command.Rows[2].Floor, 1);
    TestEqual(TEXT("Clock percent"), Command.Rows[0].ClockPercent.Get(0.0f), 75.0f);
    TestEqual(TEXT("Destination recipe"), Command.Rows[1].Recipe.Get(FString()), FString(TEXT("IronPlate")));
    TestEqual(TEXT("Floor height"), Command.FloorHeightMeters, 48);
    TestEqual(TEXT("Belt tier"), Command.BeltTier.Get(0), 3);

    if (!TestTrue(TEXT("Single-row command"),
                  FFactoryCommandParser::ParseCommand(TEXT("1 Smelter IngotIron"), Command, Error)))
        return false;
    TestEqual(TEXT("Default floor height"), Command.FloorHeightMeters, 32);
    TestFalse(TEXT("Automatic belt tier"), Command.BeltTier.IsSet());
    TestFalse(TEXT("Foundations enabled by default"), Command.bNoFoundations);
    if (!TestTrue(TEXT("Manual noFoundations option"),
                  FFactoryCommandParser::ParseCommand(TEXT("1 Smelter IngotIron floor=2, noFoundations"), Command, Error)))
        return false;
    TestTrue(TEXT("Foundations omitted"), Command.bNoFoundations);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactorySpawnerInvalidFloorsTest, "FactorySpawner.Command.InvalidFloors",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFactorySpawnerInvalidFloorsTest::RunTest(const FString&)
{
    const TCHAR* InvalidCommands[] = {
        TEXT("1 Smelter floor=0"),
        TEXT("1 Smelter floor=9"),
        TEXT("1 Smelter floor=top"),
        TEXT("1 Smelter floor=2 floor=3"),
        TEXT("1 Smelter floor=2 75"),
        TEXT("1 Smelter floor=2, floorHeight 8"),
        TEXT("1 Smelter floor=2, floorHeight 40, floorHeight 48"),
        TEXT("floorHeight 32"),
        TEXT("1 Smelter, noFoundations, noFoundations"),
    };
    for (const TCHAR* Input : InvalidCommands)
    {
        FFactoryCommand Command;
        FString Error;
        TestFalse(Input, FFactoryCommandParser::ParseCommand(Input, Command, Error));
        TestTrue(TEXT("Error message"), !Error.IsEmpty());
        TestTrue(TEXT("No partially parsed result"), Command.Rows.IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFactorySpawnerBlueprintNaming, "FactorySpawner.Command.BlueprintNaming",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFactorySpawnerBlueprintNaming::RunTest(const FString&)
{
    FString Input = TEXT("build Rotor=10, name Rotor factory, compact");
    FString Name;
    FString Error;
    TestTrue(TEXT("Production custom name"), ExtractBlueprintName(Input, Name, Error));
    TestEqual(TEXT("Name with spaces"), Name, FString(TEXT("Rotor factory")));
    TestEqual(TEXT("Remaining production arguments"), Input, FString(TEXT("build Rotor=10, compact")));

    Input = TEXT("2 CoalGenerator, name \u7164\u7535\u5382");
    TestTrue(TEXT("Manual custom name"), ExtractBlueprintName(Input, Name, Error));
    TestEqual(TEXT("Unicode name"), Name, FString(TEXT("\u7164\u7535\u5382")));
    TestEqual(TEXT("Remaining machine arguments"), Input, FString(TEXT("2 CoalGenerator")));

    const TCHAR* InvalidNames[] = {TEXT("name"), TEXT("name A, name B"), TEXT("name ../A"),
                                  TEXT("name AUX.data"), TEXT("name Power.")};
    for (const TCHAR* Invalid : InvalidNames)
    {
        Input = FString(TEXT("2 CoalGenerator, ")) + Invalid;
        Error.Reset();
        TestFalse(Invalid, ExtractBlueprintName(Input, Name, Error));
        TestTrue(TEXT("Invalid name has an error"), !Error.IsEmpty());
    }

    Input = TEXT("2 CoalGenerator");
    TestTrue(TEXT("Command without custom name"), ExtractBlueprintName(Input, Name, Error));
    TestTrue(TEXT("Name remains automatic"), Name.IsEmpty());
    TestEqual(TEXT("Unnamed command unchanged"), Input, FString(TEXT("2 CoalGenerator")));

    const FString Label = TEXT("Rotor 10 per min");
    const FString Normal = MakeAutomaticBlueprintName(Label, TEXT("Rotor=10"));
    TestTrue(TEXT("Automatic name keeps readable label"), Normal.StartsWith(Label));
    TestEqual(TEXT("Same configuration has a stable name"), MakeAutomaticBlueprintName(Label, TEXT("Rotor=10")), Normal);
    TestNotEqual(TEXT("Different configurations have different names"),
                 MakeAutomaticBlueprintName(Label, TEXT("Rotor=10, compact")), Normal);
    const FString LongLabel = FString::ChrN(150, TEXT('A'));
    TestEqual(TEXT("Long label is capped"), MakeAutomaticBlueprintName(LongLabel, Input).Len(), 80);
    return true;
}

#endif
