#include "BlueprintCommandFile.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"

bool LoadBlueprintCommandFile(const FString& FileName, FString& Command, FString& Error)
{
    FText InvalidPathReason;
    if (!FileName.EndsWith(TEXT(".txt")) || FileName.Contains(TEXT("/")) || FileName.Contains(TEXT("\\")) ||
        FileName.Contains(TEXT(":")) || !FPaths::ValidatePath(FileName, &InvalidPathReason))
    {
        Error = TEXT("Specify a .txt filename in the FactorySpawner/Commands folder.");
        return false;
    }
    const FString Path = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FactorySpawner"), TEXT("Commands"), FileName));
    TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*Path, FILEREAD_Silent));
    if (!Reader)
    {
        Error = FString::Printf(TEXT("Could not open command file: %s"), *Path);
        return false;
    }

    const int64 Size = Reader->TotalSize();
    if (Size <= 0 || Size > MaxBlueprintCommandBytes)
    {
        Error = TEXT("Command file must contain 1 to 65536 bytes.");
        return false;
    }
    if (!FFileHelper::LoadFileToString(Command, *Reader) || Command.TrimStartAndEnd().IsEmpty())
    {
        Error = TEXT("Could not read a command from the file.");
        return false;
    }
    return true;
}
