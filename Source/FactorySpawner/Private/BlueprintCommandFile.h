#pragma once

#include "CoreMinimal.h"

constexpr int32 MaxBlueprintCommandBytes = 64 * 1024;

bool LoadBlueprintCommandFile(const FString& FileName, FString& Command, FString& Error);
