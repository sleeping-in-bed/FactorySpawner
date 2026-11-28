#pragma once

#include "CoreMinimal.h"

bool ExtractBlueprintName(FString& Command, FString& OutName, FString& OutError);
FString MakeAutomaticBlueprintName(const FString& Label, const FString& Configuration);
