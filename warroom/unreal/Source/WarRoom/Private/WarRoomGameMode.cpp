#include "WarRoomGameMode.h"

#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GlobeActor.h"
#include "Kismet/GameplayStatics.h"
#include "OrbitCameraPawn.h"
#include "SWarRoomHUD.h"
#include "WarRoomPlayerController.h"
#include "WarRoomSubsystem.h"

AWarRoomGameMode::AWarRoomGameMode()
{
	DefaultPawnClass = AOrbitCameraPawn::StaticClass();
	PlayerControllerClass = AWarRoomPlayerController::StaticClass();
}

void AWarRoomGameMode::BeginPlay()
{
	Super::BeginPlay();
	for (TActorIterator<AGlobeActor> It(GetWorld()); It; ++It) { Globe = *It; break; }
	if (!Globe) Globe = GetWorld()->SpawnActor<AGlobeActor>(AGlobeActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

	AWarRoomPlayerController* PC = Cast<AWarRoomPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	AOrbitCameraPawn* Cam = PC ? Cast<AOrbitCameraPawn>(PC->GetPawn()) : nullptr;
	if (Cam) Cam->SetGlobe(Globe);
	if (PC) PC->SetGlobe(Globe);

	if (GEngine && GEngine->GameViewport)
	{
		HUD = SNew(SWarRoomHUD).Globe(Globe).Camera(Cam).Subsystem(GetGameInstance()->GetSubsystem<UWarRoomSubsystem>());
		GEngine->GameViewport->AddViewportWidgetContent(HUD.ToSharedRef(), 10);
		if (PC) PC->SetHUD(HUD);
	}
}

void AWarRoomGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	if (HUD.IsValid() && GEngine && GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(HUD.ToSharedRef());
	HUD.Reset();
	Super::EndPlay(Reason);
}
