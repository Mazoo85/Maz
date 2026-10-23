#include "OrbitCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GlobeActor.h"
#include "EngineUtils.h"

static const double kMinDist = 1.025, kMaxDist = 4.5;

AOrbitCameraPawn::AOrbitCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->FieldOfView = 42.f;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AOrbitCameraPawn::SetGlobe(AGlobeActor* InGlobe) { Globe = InGlobe; Place(); }

double AOrbitCameraPawn::Altitude() const { return Dist - 1.0; }

FName AOrbitCameraPawn::ZoomLevel() const
{
	const double d = Altitude();
	if (d > 1.6) return TEXT("globe");
	if (d > 0.55) return TEXT("continent");
	if (d > 0.18) return TEXT("state");
	return TEXT("province");
}

void AOrbitCameraPawn::SetupPlayerInputComponent(UInputComponent* In)
{
	Super::SetupPlayerInputComponent(In);
	In->BindAxis(TEXT("Turn"), this, &AOrbitCameraPawn::Turn);
	In->BindAxis(TEXT("LookUp"), this, &AOrbitCameraPawn::LookUp);
	In->BindAxis(TEXT("Zoom"), this, &AOrbitCameraPawn::Zoom);
	In->BindAxis(TEXT("PanX"), this, &AOrbitCameraPawn::OnPanX);
	In->BindAxis(TEXT("PanY"), this, &AOrbitCameraPawn::OnPanY);
}

void AOrbitCameraPawn::FlyTo(double InLat, double InLon, double DistanceFactor)
{
	TargetLat = InLat;
	// go the short way round
	double L = InLon;
	while (L - Lon > 180) L -= 360; while (L - Lon < -180) L += 360;
	TargetLon = L;
	if (DistanceFactor > 0) TargetDist = FMath::Clamp(DistanceFactor, kMinDist, kMaxDist);
	bFlying = true;
}

void AOrbitCameraPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Globe) { for (TActorIterator<AGlobeActor> It(GetWorld()); It; ++It) { SetGlobe(*It); break; } }
	const double Alt = Altitude();
	const double TurnSpeed = FMath::Clamp(Alt * 0.5, 0.03, 1.0) * 0.6;   // degrees per mouse unit, slower when close
	if (bDragging && (TurnInput != 0 || LookInput != 0))
	{
		bFlying = false;
		TargetLon -= TurnInput * TurnSpeed * 60.0 * 0.08;
		TargetLat += LookInput * TurnSpeed * 60.0 * 0.08;
	}
	if (PanX != 0 || PanY != 0)
	{
		bFlying = false;
		TargetLon += PanX * DeltaTime * 40.0 * FMath::Clamp(Alt, 0.05, 1.5);
		TargetLat += PanY * DeltaTime * 40.0 * FMath::Clamp(Alt, 0.05, 1.5);
	}
	if (ZoomInput != 0)
	{
		bFlying = false;
		TargetDist = FMath::Clamp(TargetDist * FMath::Pow(0.85, (double)ZoomInput), kMinDist, kMaxDist);
		ZoomInput = 0;
	}
	TargetLat = FMath::Clamp(TargetLat, -89.0, 89.0);
	const double K = FMath::Clamp(DeltaTime * (bFlying ? 3.0 : 12.0), 0.0, 1.0);
	Lat += (TargetLat - Lat) * K; Lon += (TargetLon - Lon) * K; Dist += (TargetDist - Dist) * K;
	if (bFlying && FMath::Abs(TargetLat - Lat) < 0.05 && FMath::Abs(TargetLon - Lon) < 0.05 && FMath::Abs(TargetDist - Dist) < 0.002) bFlying = false;
	while (Lon > 180) { Lon -= 360; TargetLon -= 360; } while (Lon < -180) { Lon += 360; TargetLon += 360; }
	Place();
	TurnInput = LookInput = 0;
}

void AOrbitCameraPawn::Place()
{
	if (!Globe) return;
	const FVector Centre = Globe->GetActorLocation();
	const FVector Pos = Globe->LatLonToWorld(Lat, Lon, (Dist - 1.0) * Globe->Radius);
	SetActorLocation(Pos);
	const FVector Up = (Pos - Centre).GetSafeNormal();
	const FVector North = FVector::VectorPlaneProject(FVector::UpVector, Up).GetSafeNormal();
	SetActorRotation(FRotationMatrix::MakeFromXZ(-Up, North.IsNearlyZero() ? FVector::ForwardVector : North).Rotator());
}
