// Spawns the globe, the camera and the interface. Works in any level, even an empty one.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "WarRoomGameMode.generated.h"

class AGlobeActor;
class SWarRoomHUD;

UCLASS()
class WARROOM_API AWarRoomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AWarRoomGameMode();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY() AGlobeActor* Globe = nullptr;
	TSharedPtr<SWarRoomHUD> HUD;
};
