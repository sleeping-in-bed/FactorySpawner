#include "BlueprintNaming.h"

#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

namespace
{
    constexpr int32 MaxBlueprintNameLength = 80;

    bool IsValidBlueprintName(const FString& Name)
    {
        FString Base;
        if (!Name.Split(TEXT("."), &Base, nullptr))
            Base = Name;
        Base.TrimEndInline();

        if (Name.IsEmpty() || Name.Len() > MaxBlueprintNameLength || Name.StartsWith(TEXT(".")) ||
            Name.EndsWith(TEXT(".")) || !Name.Equals(FPaths::MakeValidFileName(Name), ESearchCase::CaseSensitive) ||
            !FPaths::ValidatePath(Base))
            return false;

        for (const TCHAR Character : Name)
            if (Character < 32)
                return false;
        return true;
    }
}

bool ExtractBlueprintName(FString& Command, FString& OutName, FString& OutError)
{
    TArray<FString> Groups;
    Command.ParseIntoArray(Groups, TEXT(","), false);
    FString Name;
    for (int32 Index = Groups.Num() - 1; Index >= 0; --Index)
    {
        const FString Group = Groups[Index].TrimStartAndEnd();
        if (!Group.Equals(TEXT("name"), ESearchCase::IgnoreCase) &&
            !Group.StartsWith(TEXT("name "), ESearchCase::IgnoreCase))
            continue;

        if (!Name.IsEmpty())
        {
            OutError = TEXT("Set the blueprint name only once");
            return false;
        }
        Name = Group.Mid(4).TrimStartAndEnd();
        if (!IsValidBlueprintName(Name))
        {
            OutError = FString::Printf(TEXT("Blueprint name must be 1-%d characters and valid as a file name"),
                                      MaxBlueprintNameLength);
            return false;
        }
        Groups.RemoveAt(Index);
    }

    if (!Name.IsEmpty())
        Command = FString::Join(Groups, TEXT(",")).TrimStartAndEnd();
    OutName = MoveTemp(Name);
    return true;
}

FString MakeAutomaticBlueprintName(const FString& Label, const FString& Configuration)
{
    const FTCHARToUTF8 ConfigurationUtf8(*Configuration);
    const FString Hash = FMD5::HashBytes(reinterpret_cast<const uint8*>(ConfigurationUtf8.Get()),
                                         ConfigurationUtf8.Length()).Left(12);
    const FString Suffix = TEXT(" - ") + Hash;
    FString Title = FPaths::MakeValidFileName(Label, TEXT('-')).Left(MaxBlueprintNameLength - Suffix.Len());

    // Do not split a supplementary Unicode character when trimming the readable part.
    if (!Title.IsEmpty() && Title[Title.Len() - 1] >= 0xD800 && Title[Title.Len() - 1] <= 0xDBFF)
        Title.LeftChopInline(1);
    return Title.TrimStartAndEnd() + Suffix;
}
