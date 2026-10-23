#include "WarRoomPlayerController.h"

#include "GlobeActor.h"
#include "OrbitCameraPawn.h"
#include "SWarRoomHUD.h"
#include "WarRoomSubsystem.h"
#include "EngineUtils.h"

AWarRoomPlayerController::AWarRoomPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AWarRoomPlayerController::BeginPlay()
{
	Super::BeginPlay();
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
}

AOrbitCameraPawn* AWarRoomPlayerController::Cam() const { return Cast<AOrbitCameraPawn>(GetPawn()); }

void AWarRoomPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction(TEXT("Select"), IE_Pressed, this, &AWarRoomPlayerController::OnSelectPressed);
	InputComponent->BindAction(TEXT("Select"), IE_Released, this, &AWarRoomPlayerController::OnSelectReleased);
	InputComponent->BindAction(TEXT("Drag"), IE_Pressed, this, &AWarRoomPlayerController::OnDragPressed);
	InputComponent->BindAction(TEXT("Drag"), IE_Released, this, &AWarRoomPlayerController::OnDragReleased);
	InputComponent->BindAction(TEXT("Pause"), IE_Pressed, this, &AWarRoomPlayerController::OnPauseKey);
	InputComponent->BindAction(TEXT("Speed1"), IE_Pressed, this, &AWarRoomPlayerController::OnSpeed1);
	InputComponent->BindAction(TEXT("Speed2"), IE_Pressed, this, &AWarRoomPlayerController::OnSpeed2);
	InputComponent->BindAction(TEXT("Speed3"), IE_Pressed, this, &AWarRoomPlayerController::OnSpeed3);
	InputComponent->BindAction(TEXT("Speed4"), IE_Pressed, this, &AWarRoomPlayerController::OnSpeed4);
	InputComponent->BindAction(TEXT("Speed5"), IE_Pressed, this, &AWarRoomPlayerController::OnSpeed5);
	InputComponent->BindAction(TEXT("Faster"), IE_Pressed, this, &AWarRoomPlayerController::OnFaster);
	InputComponent->BindAction(TEXT("Slower"), IE_Pressed, this, &AWarRoomPlayerController::OnSlower);
	InputComponent->BindAction(TEXT("ClosePanel"), IE_Pressed, this, &AWarRoomPlayerController::OnClosePanel);
}

void AWarRoomPlayerController::OnSelectPressed()
{
	float X, Y; GetMousePosition(X, Y);
	PressPos = FVector2D(X, Y); bPressed = true; bDrag = false; PressTime = FPlatformTime::Seconds();
	if (Cam()) Cam()->SetDragging(true);
}

void AWarRoomPlayerController::OnSelectReleased()
{
	if (Cam()) Cam()->SetDragging(false);
	if (!bPressed) return;
	bPressed = false;
	float X, Y; GetMousePosition(X, Y);
	const bool bTap = FVector2D::Distance(FVector2D(X, Y), PressPos) < 7.f && FPlatformTime::Seconds() - PressTime < 0.6;
	if (!bTap || !Globe || !HUD.IsValid()) return;
	const int32 Id = Globe->PickAtScreen(this, FVector2D(X, Y));
	if (Id) HUD->OpenProvince(Id, false); else HUD->ClosePanel();
}

void AWarRoomPlayerController::OnDragPressed() { if (Cam()) Cam()->SetDragging(true); }
void AWarRoomPlayerController::OnDragReleased() { if (Cam()) Cam()->SetDragging(false); }

void AWarRoomPlayerController::FindGlobe()
{
	for (TActorIterator<AGlobeActor> It(GetWorld()); It; ++It) { Globe = *It; return; }
}

void AWarRoomPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (!Globe) FindGlobe();
	if (!Globe) return;
	float X, Y;
	if (GetMousePosition(X, Y) && !bPressed) Globe->SetHover(Globe->PickAtScreen(this, FVector2D(X, Y)));
	if (Cam()) { const FName L = Cam()->ZoomLevel(); Globe->SetBorderLevel(L == TEXT("globe") ? 0.f : L == TEXT("continent") ? 1.f : 2.f); }
}

void AWarRoomPlayerController::OnPauseKey()
{
	if (UWarRoomSubsystem* S = GetGameInstance()->GetSubsystem<UWarRoomSubsystem>())
	{
		warroom::Command C; C.type = S->GetState().speed ? "pause" : "resume";
		S->Send(C);
	}
}
void AWarRoomPlayerController::OnSpeedKey(int32 Speed) { if (UWarRoomSubsystem* S = GetGameInstance()->GetSubsystem<UWarRoomSubsystem>()) S->SetSpeed(Speed); }
void AWarRoomPlayerController::OnFaster() { if (UWarRoomSubsystem* S = GetGameInstance()->GetSubsystem<UWarRoomSubsystem>()) S->SetSpeed(FMath::Min(5, S->GetState().speed + 1)); }
void AWarRoomPlayerController::OnSlower() { if (UWarRoomSubsystem* S = GetGameInstance()->GetSubsystem<UWarRoomSubsystem>()) S->SetSpeed(FMath::Max(0, S->GetState().speed - 1)); }
void AWarRoomPlayerController::OnClosePanel() { if (HUD.IsValid()) HUD->ClosePanel(); }
