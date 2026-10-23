// Mouse and keys: click picks a province, drag turns the globe, keys drive the clock.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "WarRoomPlayerController.generated.h"

class AGlobeActor;
class AOrbitCameraPawn;
class SWarRoomHUD;

UCLASS()
class WARROOM_API AWarRoomPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AWarRoomPlayerController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	void SetGlobe(AGlobeActor* InGlobe) { Globe = InGlobe; }
	void SetHUD(TSharedPtr<SWarRoomHUD> InHUD) { HUD = InHUD; }

private:
	UPROPERTY() AGlobeActor* Globe = nullptr;
	TSharedPtr<SWarRoomHUD> HUD;
	FVector2D PressPos = FVector2D::ZeroVector;
	bool bPressed = false, bDrag = false;
	double PressTime = 0;

	void OnSelectPressed();
	void OnSelectReleased();
	void OnDragPressed();
	void OnDragReleased();
	void OnPauseKey();
	void OnSpeedKey(int32 Speed);
	void OnSpeed1() { OnSpeedKey(1); }
	void OnSpeed2() { OnSpeedKey(2); }
	void OnSpeed3() { OnSpeedKey(3); }
	void OnSpeed4() { OnSpeedKey(4); }
	void OnSpeed5() { OnSpeedKey(5); }
	void FindGlobe();
	void OnFaster();
	void OnSlower();
	void OnClosePanel();
	AOrbitCameraPawn* Cam() const;
};
