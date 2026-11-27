#include "BuildPlanGenerator.h"
#include "BuildableCache.h"

#include "Buildables/FGBuildableGeneratorFuel.h"
#include "Buildables/FGBuildableGeneratorNuclear.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableManufacturerVariablePower.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "FGFactoryConnectionComponent.h"
#include "FGRecipe.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "ProductionPlan.h"
#include "Resources/FGItemDescriptorNuclearFuel.h"

namespace
{
    using FMaterialRates = TMap<TSubclassOf<UFGItemDescriptor>, double>;

    bool IsFluidMaterial(TSubclassOf<UFGItemDescriptor> Item)
    {
        const EResourceForm Form = UFGItemDescriptor::GetForm(Item);
        return Form == EResourceForm::RF_LIQUID || Form == EResourceForm::RF_GAS;
    }

    FString FormatQuantity(double Rate)
    {
        FNumberFormattingOptions Options;
        Options.SetUseGrouping(false).SetMaximumFractionalDigits(3);
        return FText::AsNumber(Rate, &Options).ToString();
    }

    FString FormatMaterials(const FMaterialRates& Materials, bool bChinese)
    {
        TArray<FString> Entries;
        for (const auto& Material : Materials)
        {
            if (!Material.Key || Material.Value <= 0)
                continue;
            const TCHAR* Unit = IsFluidMaterial(Material.Key) ? TEXT("m³") : (bChinese ? TEXT("个") : TEXT("items"));
            Entries.Add(FString::Printf(TEXT("%s %s %s/%s"), *UFGItemDescriptor::GetItemName(Material.Key).ToString(),
                                       *FormatQuantity(Material.Value), Unit, bChinese ? TEXT("分钟") : TEXT("min")));
        }
        Entries.Sort();
        return Entries.IsEmpty() ? (bChinese ? TEXT("无") : TEXT("None"))
                                 : FString::Join(Entries, bChinese ? TEXT("、") : TEXT(", "));
    }

    const TCHAR* ReferenceFuelName(const AFGBuildableGeneratorFuel& Generator)
    {
        if (Generator.IsA<AFGBuildableGeneratorNuclear>())
            return TEXT("Desc_NuclearFuelRod_C");
        if (Generator.GetClass()->GetName().Contains(TEXT("GeneratorBiomass")))
            return TEXT("Desc_Biofuel_C");
        if (Generator.GetRequiresSupplementalResource())
            return TEXT("Desc_Coal_C");
        return TEXT("Desc_LiquidFuel_C");
    }

    TSubclassOf<UFGItemDescriptor> FindReferenceFuel(const AFGBuildableGeneratorFuel& Generator)
    {
        const FString ReferenceName(ReferenceFuelName(Generator));
        const auto Fuels = Generator.GetDefaultFuelClasses();
        const auto* Reference = Fuels.FindByPredicate([&](const auto& Fuel) { return Fuel.GetAssetName() == ReferenceName; });
        if (Reference)
            return Reference->LoadSynchronous();

        for (const auto& Fuel : Fuels)
        {
            const TSubclassOf<UFGItemDescriptor> Descriptor = Fuel.LoadSynchronous();
            if (Descriptor && UFGItemDescriptor::GetEnergyValue(Descriptor) > 0.0)
                return Descriptor;
        }
        return {};
    }
}

FString FBuildPlanGenerator::DescribeBlueprint(const FFactoryCommand& Command, const TArray<FPreparedRow>& Rows,
                                               const FString& PreviousDescription) const
{
    const bool bChinese = FInternationalization::Get().GetCurrentLanguage()->GetName().StartsWith(TEXT("zh"));
    FMaterialRates Inputs;
    FMaterialRates Outputs;
    FMaterialRates Byproducts;
    double EstimatedPower = Production ? Production->PeakPowerMW : 0.0;
    double RatedGeneration = 0.0;
    int32 UnspecifiedMachines = 0;
    TSet<FString> ReferenceFuels;
    TSet<FString> AlternateRecipes;

    if (Production)
    {
        for (const auto& Material : Production->Imports)
            Inputs.Add(Production->Items.FindChecked(Material.Key), Material.Value);
        for (const auto& Material : Production->Exports)
        {
            const bool bPrimary = Production->RequestedProducts.Contains(Material.Key);
            (bPrimary ? Outputs : Byproducts).Add(Production->Items.FindChecked(Material.Key), Material.Value);
        }
    }
    else
    {
        for (int32 Index = 0; Index < Rows.Num(); ++Index)
        {
            const auto* Recipe = Rows[Index].Recipe.GetDefaultObject();
            if (!Recipe || Recipe->GetManufacturingDuration() <= 0)
                continue;

            const auto& Row = Command.Rows[Index];
            const double Cycles = 60.0 * Row.Count * Row.ClockPercent.Get(100.0f) / 100.0 /
                                  Recipe->GetManufacturingDuration();
            const auto& Ports = RowConveyors[Index];
            const auto AddExternal = [Cycles](const TArray<FItemAmount>& Items, const TArray<FItemPort>& Belts,
                                              FMaterialRates& Destination)
            {
                for (const auto& Item : Items)
                {
                    const auto* Port = Belts.FindByPredicate([&](const FItemPort& Belt) { return Belt.Item == Item.ItemClass; });
                    if (Port && Port->Connection->GetConnection())
                        continue;
                    const double Units = IsFluidMaterial(Item.ItemClass) ? 1000.0 : 1.0;
                    Destination.FindOrAdd(Item.ItemClass) += Cycles * Item.Amount / Units;
                }
            };
            AddExternal(Recipe->GetIngredients(), Ports.Inputs, Inputs);
            const auto& Products = Recipe->GetProducts();
            if (!Products.IsEmpty())
            {
                AddExternal({Products[0]}, Ports.Outputs, Outputs);
                for (int32 Product = 1; Product < Products.Num(); ++Product)
                    AddExternal({Products[Product]}, Ports.Outputs, Byproducts);
            }
        }
    }

    TMap<UClass*, TSubclassOf<UFGItemDescriptor>> FuelByGenerator;
    for (const auto* Buildable : BuildablesForBlueprint)
    {
        if (const auto* Pump = Cast<AFGBuildablePipelinePump>(Buildable); Pump && Pump->RunsOnPower())
        {
            EstimatedPower += Pump->GetDefaultProducingPowerConsumption();
        }
        else if (const auto* Generator = Cast<AFGBuildableGeneratorFuel>(Buildable))
        {
            const double Capacity = Generator->GetDefaultPowerProductionCapacity();
            RatedGeneration += Capacity;
            if (Generator->GetRequiresSupplementalResource() && Generator->GetSupplementalResourceClass())
                Inputs.FindOrAdd(Generator->GetSupplementalResourceClass()) += 60.0 * Generator->GetSupplementalConsumptionRateMaximum();

            auto* Fuel = FuelByGenerator.Find(Generator->GetClass());
            if (!Fuel)
            {
                const bool bSelectedFuel = GeneratorFuel && Generator->GetClass() ==
                    Cache->GetBuildableClass<AFGBuildableGeneratorFuel>(EBuildable::FuelGenerator).Get();
                Fuel = &FuelByGenerator.Add(Generator->GetClass(), bSelectedFuel ? GeneratorFuel : FindReferenceFuel(*Generator));
            }
            const double Energy = *Fuel ? UFGItemDescriptor::GetEnergyValue(*Fuel) : 0.0;
            if (Energy <= 0.0)
            {
                ++UnspecifiedMachines;
                continue;
            }
            const double PerMinute = 60.0 * Capacity / Energy;
            Inputs.FindOrAdd(*Fuel) += PerMinute / (IsFluidMaterial(*Fuel) ? 1000.0 : 1.0);
            ReferenceFuels.Add(UFGItemDescriptor::GetItemName(*Fuel).ToString());
            if (Generator->IsA<AFGBuildableGeneratorNuclear>() && (*Fuel)->IsChildOf(UFGItemDescriptorNuclearFuel::StaticClass()))
            {
                const TSubclassOf<UFGItemDescriptorNuclearFuel> NuclearFuel(Fuel->Get());
                const auto Waste = UFGItemDescriptorNuclearFuel::GetSpentFuelClass(NuclearFuel);
                if (Waste)
                    Byproducts.FindOrAdd(Waste) += PerMinute * UFGItemDescriptorNuclearFuel::GetAmountWasteCreated(NuclearFuel);
            }
        }
        else if (const auto* Manufacturer = Cast<AFGBuildableManufacturer>(Buildable))
        {
            const auto Recipe = Manufacturer->GetCurrentRecipe();
            if (Recipe && (Recipe->GetName().StartsWith(TEXT("Recipe_Alternate_")) ||
                           Recipe->GetPathName().Contains(TEXT("/AlternateRecipes/"))))
            {
                FString Name = Recipe.GetDefaultObject()->GetDisplayName().ToString();
                Name.RemoveFromStart(TEXT("替代："));
                Name.RemoveFromStart(TEXT("Alternate:"));
                AlternateRecipes.Add(Name.TrimStartAndEnd());
            }
            if (Production)
                continue;
            if (!Recipe)
                ++UnspecifiedMachines;
            const auto* Variable = Cast<AFGBuildableManufacturerVariablePower>(Manufacturer);
            if (Variable && Recipe)
            {
                const double BasePeak = FMath::Max(Variable->mEstimatedMaximumPowerConsumption,
                    Recipe.GetDefaultObject()->GetPowerConsumptionConstant() + Recipe.GetDefaultObject()->GetPowerConsumptionFactor());
                EstimatedPower += BasePeak * FMath::Pow(Manufacturer->GetPendingPotential(), 1.32192809489);
            }
            else
                EstimatedPower += Manufacturer->CalcProducingPowerConsumptionForPotential(Manufacturer->GetPendingPotential());
        }
    }

    const FString Marker = TEXT("[FactorySpawner]");
    FString Description = PreviousDescription;
    const int32 PreviousBlock = Description.StartsWith(Marker + TEXT("\n")) ? 0
        : Description.Find(TEXT("\n\n") + Marker + TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (PreviousBlock != INDEX_NONE)
        Description.LeftInline(PreviousBlock);
    else if (Description == TEXT("Auto-generated blueprint"))
        Description.Reset();
    Description.TrimEndInline();
    if (!Description.IsEmpty())
        Description += TEXT("\n\n");
    Description += Marker;
    Description += (bChinese ? TEXT("\n外部输入 ") : TEXT("\nExternal inputs ")) + FormatMaterials(Inputs, bChinese);
    Description += (bChinese ? TEXT("\n对外输出 ") : TEXT("\nExternal outputs ")) + FormatMaterials(Outputs, bChinese);
    if (!Byproducts.IsEmpty())
        Description += (bChinese ? TEXT("\n副产物 ") : TEXT("\nByproducts ")) + FormatMaterials(Byproducts, bChinese);
    if (!AlternateRecipes.IsEmpty())
    {
        auto Names = AlternateRecipes.Array();
        Names.Sort();
        Description += (bChinese ? TEXT("\n替代配方：") : TEXT("\nAlternate recipes: ")) +
                       FString::Join(Names, bChinese ? TEXT("、") : TEXT(", "));
    }
    if (EstimatedPower > 0.0 || RatedGeneration <= 0.0)
        Description += (bChinese ? TEXT("\n预计峰值用电 ") : TEXT("\nEstimated peak consumption ")) + FormatQuantity(EstimatedPower) + TEXT(" MW");
    if (RatedGeneration > 0.0)
        Description += (bChinese ? TEXT("\n额定发电 ") : TEXT("\nRated generation ")) + FormatQuantity(RatedGeneration) + TEXT(" MW");
    if (!ReferenceFuels.IsEmpty())
    {
        auto Names = ReferenceFuels.Array();
        Names.Sort();
        Description += (bChinese ? TEXT("\n发电物料按满载计算，燃料选用 ") : TEXT("\nFull-load generator rates use ")) +
                       FString::Join(Names, bChinese ? TEXT("、") : TEXT(", "));
    }
    bool bHasLiquidInlet = false;
    for (int32 Index = 0; Index < GeneratorPipeLanes.Num(); ++Index)
    {
        const auto& Lane = GeneratorPipeLanes[Index];
        bHasLiquidInlet |= UFGItemDescriptor::GetForm(Lane.Item) == EResourceForm::RF_LIQUID;
        const FString ItemName = UFGItemDescriptor::GetItemName(Lane.Item).ToString();
        const FString Rate = FormatQuantity(Lane.Rate);
        Description += bChinese
            ? FString::Printf(TEXT("\n管道入口 %d：%s %s m³/分钟"), Index + 1, *ItemName, *Rate)
            : FString::Printf(TEXT("\nPipe inlet %d: %s %s m³/min"), Index + 1, *ItemName, *Rate);
    }
    if (bHasLiquidInlet)
        Description += bChinese
            ? TEXT("\n外部液源需在入口保留约 7 米扬程；如果蓝图带泵，启动时可能需要外接电源。")
            : TEXT("\nAllow about 7 m of residual liquid head at the inlet. Any pumps may need external power to start.");
    if (UnspecifiedMachines > 0)
        Description += bChinese
            ? FString::Printf(TEXT("\n%d 台机器的配方或燃料未确定，物料清单未包含这些机器"), UnspecifiedMachines)
            : FString::Printf(TEXT("\nMaterials exclude %d machines whose recipe or fuel is unknown"), UnspecifiedMachines);
    return Description;
}
