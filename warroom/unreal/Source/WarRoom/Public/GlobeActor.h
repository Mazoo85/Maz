// The globe: a procedural sphere whose material reads the province-id texture and the
// lookup textures this actor keeps up to date; picking by analytic lat/lon; the
// Blender counters for the armies.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GlobeActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UTexture2D;
class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UWarRoomSubsystem;

UENUM()
enum class EWarRoomMapMode : uint8 { Political, Terrain, States, Regions, Weather, VictoryPoints };

UCLASS()
class WARROOM_API AGlobeActor : public AActor
{
	GENERATED_BODY()

public:
	AGlobeActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Globe radius in world units (centimetres). */
	UPROPERTY(EditAnywhere, Category = "War Room") float Radius = 100000.f;

	// Province under a screen position, or 0.
	int32 PickAtScreen(APlayerController* PC, const FVector2D& ScreenPos) const;
	int32 PickAtWorld(const FVector& WorldPoint) const;
	FVector LatLonToWorld(double Lat, double Lon, double Altitude = 0.0) const;
	static void WorldDirToLatLon(const FVector& Dir, double& OutLat, double& OutLon);

	void SetMapMode(EWarRoomMapMode Mode);
	EWarRoomMapMode GetMapMode() const { return MapMode; }
	void SetHover(int32 ProvinceId);
	void SetSelected(int32 ProvinceId);
	void Highlight(const TArray<int32>& Ids, bool bDimOthers);
	void SetBorderLevel(float Level);
	void Repaint();          // recolours the lookup for the current map mode
	void RefreshCounters();  // re-places the army counters from the state

private:
	UPROPERTY() UProceduralMeshComponent* Mesh = nullptr;
	UPROPERTY() UMaterialInstanceDynamic* Material = nullptr;
	UPROPERTY() UTexture2D* ColorLUT = nullptr;
	UPROPERTY() UTexture2D* InfoLUT = nullptr;
	UPROPERTY() UTexture2D* FlagLUT = nullptr;
	UPROPERTY() UInstancedStaticMeshComponent* Counters = nullptr;
	UPROPERTY() TArray<UTextRenderComponent*> CounterLabels;
	UPROPERTY() UMaterialInterface* GlobeBaseMaterial = nullptr;

	TArray<uint16> IdMap;      // CPU copy of provinces.png for picking
	int32 IdMapW = 0, IdMapH = 0;
	EWarRoomMapMode MapMode = EWarRoomMapMode::Political;
	TArray<uint8> ColorBytes, InfoBytes, FlagBytes;

	void BuildSphere();
	bool LoadIdMapForPicking();
	UTexture2D* MakeLut();
	void UploadLut(UTexture2D* Tex, const TArray<uint8>& Bytes);
	void PaintInfo();
	UWarRoomSubsystem* Sim() const;
};
