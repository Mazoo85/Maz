// Owns the data and the game state for the whole game instance, advances the clock,
// takes commands, saves and loads. The Unreal face of WarRoomSim.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"

#include "WarRoomSim/WarRoomSim.h"

#include "WarRoomSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnWarRoomStateReplaced);

UCLASS()
class WARROOM_API UWarRoomSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UWarRoomSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bReady; }

	bool IsReady() const { return bReady; }
	const FString& GetLoadError() const { return LoadError; }
	const warroom::Data& GetData() const { return *Data; }
	warroom::GameState& GetState() { return State; }
	const warroom::GameState& GetState() const { return State; }

	// Sends a command; returns an empty string or the reason it was refused.
	FString Send(const warroom::Command& Cmd);
	FString SetSpeed(int32 Speed);
	FString SelectCountry(const FString& Tag);

	// Saves live in <Project>/Saved/SaveGames/<name>.warroom.json
	TArray<FString> ListSaves() const;
	bool SaveGame(const FString& Name, FString& OutError);
	bool LoadGame(const FString& Name, FString& OutError);
	bool DeleteSave(const FString& Name);
	void NewGame(const FString& Seed);

	// Where the data and models live (../data next to the project, or Content/Data when packaged).
	static FString FindDataDir();
	static FString FindModelsDir();

	FOnWarRoomStateReplaced OnStateReplaced;
	bool bRegionsDirty = false;   // the view clears this after repainting the weather map mode

private:
	TUniquePtr<warroom::Data> Data;
	warroom::GameState State;
	bool bReady = false;
	FString LoadError;
	double Accumulator = 0.0;
	long long LastAutosaveMonthHour = -1;
	FString SavePath(const FString& Name) const;
};
