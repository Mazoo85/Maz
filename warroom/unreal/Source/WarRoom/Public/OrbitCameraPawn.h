// The camera: orbits the globe, zooms in and out, flies to places. Mouse drag turns,
// wheel zooms, WASD or arrows pan.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "OrbitCameraPawn.generated.h"

class UCameraComponent;
class AGlobeActor;

UCLASS()
class WARROOM_API AOrbitCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AOrbitCameraPawn();
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void SetGlobe(AGlobeActor* InGlobe);
	void FlyTo(double Lat, double Lon, double DistanceFactor = -1.0);
	bool IsFlying() const { return bFlying; }
	/** Distance from the surface as a multiple of the radius (0 = on the surface). */
	double Altitude() const;
	FName ZoomLevel() const;   // globe | continent | state | province
	void SetDragging(bool b) { bDragging = b; }

private:
	UPROPERTY() UCameraComponent* Camera = nullptr;
	UPROPERTY() AGlobeActor* Globe = nullptr;
	double Lat = 50.0, Lon = 10.0, Dist = 2.6;   // Dist in radii from the centre
	double TargetLat = 50.0, TargetLon = 10.0, TargetDist = 2.6;
	bool bFlying = false, bDragging = false;
	float TurnInput = 0, LookInput = 0, ZoomInput = 0, PanX = 0, PanY = 0;

	void Turn(float V) { TurnInput = V; }
	void LookUp(float V) { LookInput = V; }
	void Zoom(float V) { ZoomInput += V; }
	void OnPanX(float V) { PanX = V; }
	void OnPanY(float V) { PanY = V; }
	void Place();
};
