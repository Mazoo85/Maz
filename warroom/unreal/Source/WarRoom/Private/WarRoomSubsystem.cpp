#include "WarRoomSubsystem.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WarRoomModule.h"

static std::string ToStd(const FString& S) { return std::string(TCHAR_TO_UTF8(*S)); }
static FString ToF(const std::string& S) { return FString(UTF8_TO_TCHAR(S.c_str())); }

FString UWarRoomSubsystem::FindDataDir()
{
	const FString Candidates[] = {
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("..") , TEXT("data")),
	};
	for (const FString& C : Candidates)
	{
		if (FPaths::FileExists(FPaths::Combine(C, TEXT("map"), TEXT("provinces.json")))) return FPaths::ConvertRelativePathToFull(C);
	}
	return FPaths::ConvertRelativePathToFull(Candidates[1]);
}

FString UWarRoomSubsystem::FindModelsDir()
{
	const FString Candidates[] = {
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Models")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), TEXT("models")),
	};
	for (const FString& C : Candidates)
	{
		if (FPaths::FileExists(FPaths::Combine(C, TEXT("manifest.json")))) return FPaths::ConvertRelativePathToFull(C);
	}
	return FPaths::ConvertRelativePathToFull(Candidates[1]);
}

void UWarRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Data = MakeUnique<warroom::Data>();
	const FString Dir = FindDataDir();
	const double T0 = FPlatformTime::Seconds();
	const std::string Err = warroom::LoadData(ToStd(Dir), *Data);
	if (!Err.empty())
	{
		LoadError = FString::Printf(TEXT("Could not read the world data in %s: %s"), *Dir, *ToF(Err));
		UE_LOG(LogWarRoom, Error, TEXT("%s"), *LoadError);
		return;
	}
	UE_LOG(LogWarRoom, Log, TEXT("World data loaded from %s in %.1fs: %d provinces, %d states, %d countries"), *Dir,
		FPlatformTime::Seconds() - T0, Data->provinceCount, (int)Data->states.size(), (int)Data->countries.size());
	NewGame(FString::Printf(TEXT("%lld"), (long long)FDateTime::UtcNow().GetTicks()));
	FString LoadErr;
	if (ListSaves().Contains(TEXT("autosave")) && !LoadGame(TEXT("autosave"), LoadErr))
	{
		UE_LOG(LogWarRoom, Warning, TEXT("autosave not loaded: %s"), *LoadErr);
	}
	bReady = true;
}

void UWarRoomSubsystem::Deinitialize()
{
	if (bReady) { FString E; SaveGame(TEXT("autosave"), E); }
	Super::Deinitialize();
}

void UWarRoomSubsystem::NewGame(const FString& Seed)
{
	const std::string Err = warroom::NewGame(*Data, ToStd(Seed), "1936", State);
	if (!Err.empty()) { LoadError = ToF(Err); UE_LOG(LogWarRoom, Error, TEXT("%s"), *LoadError); return; }
	Accumulator = 0;
	OnStateReplaced.Broadcast();
}

void UWarRoomSubsystem::Tick(float DeltaTime)
{
	if (!bReady) return;
	const double PerSec = warroom::kSpeedHoursPerSecond[FMath::Clamp(State.speed, 0, 5)];
	if (PerSec <= 0) { Accumulator = 0; return; }
	Accumulator += FMath::Min((double)DeltaTime, 0.25) * PerSec;
	const double Budget = FPlatformTime::Seconds() + 0.012;
	while (Accumulator >= 1.0 && FPlatformTime::Seconds() < Budget)
	{
		warroom::Tick(State, *Data);
		Accumulator -= 1.0;
		if (State.dirtyRegions) { bRegionsDirty = true; State.dirtyRegions = false; }
		if (warroom::IsNewMonth(State.hour) && State.hour != LastAutosaveMonthHour)
		{
			LastAutosaveMonthHour = State.hour;
			FString E; SaveGame(TEXT("autosave"), E);
		}
	}
	if (Accumulator > PerSec) Accumulator = PerSec;
}

FString UWarRoomSubsystem::Send(const warroom::Command& Cmd)
{
	return ToF(warroom::ApplyCommand(State, Cmd));
}

FString UWarRoomSubsystem::SetSpeed(int32 Speed)
{
	warroom::Command C; C.type = "set_speed"; C.speed = Speed;
	return Send(C);
}

FString UWarRoomSubsystem::SelectCountry(const FString& Tag)
{
	warroom::Command C; C.type = "select_country"; C.hasTag = true; C.tag = ToStd(Tag);
	return Send(C);
}

FString UWarRoomSubsystem::SavePath(const FString& Name) const
{
	FString Safe = Name;
	for (TCHAR& Ch : Safe) { if (!FChar::IsAlnum(Ch) && Ch != '-' && Ch != '_' && Ch != ' ') Ch = '_'; }
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), Safe + TEXT(".warroom.json"));
}

TArray<FString> UWarRoomSubsystem::ListSaves() const
{
	TArray<FString> Files, Names;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("*.warroom.json")), true, false);
	for (FString& F : Files) { F.RemoveFromEnd(TEXT(".warroom.json")); Names.Add(F); }
	Names.Sort();
	return Names;
}

bool UWarRoomSubsystem::SaveGame(const FString& Name, FString& OutError)
{
	const std::string Text = warroom::SaveToString(State);
	if (!FFileHelper::SaveStringToFile(ToF(Text), *SavePath(Name), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FString::Printf(TEXT("could not write %s"), *SavePath(Name));
		return false;
	}
	return true;
}

bool UWarRoomSubsystem::LoadGame(const FString& Name, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SavePath(Name))) { OutError = FString::Printf(TEXT("no save called \"%s\""), *Name); return false; }
	warroom::GameState Loaded;
	const std::string Err = warroom::LoadFromString(ToStd(Text), *Data, Loaded);
	if (!Err.empty()) { OutError = ToF(Err); return false; }
	State = std::move(Loaded);
	Accumulator = 0;
	OnStateReplaced.Broadcast();
	return true;
}

bool UWarRoomSubsystem::DeleteSave(const FString& Name)
{
	return IFileManager::Get().Delete(*SavePath(Name));
}
