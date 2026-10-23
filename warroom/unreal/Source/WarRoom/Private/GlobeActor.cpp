#include "GlobeActor.h"

#include <map>

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "WarRoomModule.h"
#include "WarRoomSubsystem.h"

static const int32 kLut = 128;

AGlobeActor::AGlobeActor()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Globe"));
	RootComponent = Mesh;
	Mesh->bUseAsyncCooking = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Counters = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Counters"));
	Counters->SetupAttachment(Mesh);
	Counters->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Counters->NumCustomDataFloats = 3;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlobeMat(TEXT("/Game/WarRoom/M_Globe.M_Globe"));
	if (GlobeMat.Succeeded()) GlobeBaseMaterial = GlobeMat.Object;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CounterMesh(TEXT("/Game/WarRoom/Models/SM_division.SM_division"));
	if (CounterMesh.Succeeded()) Counters->SetStaticMesh(CounterMesh.Object);
}

UWarRoomSubsystem* AGlobeActor::Sim() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UWarRoomSubsystem>() : nullptr;
}

void AGlobeActor::BeginPlay()
{
	Super::BeginPlay();
	BuildSphere();
	if (!LoadIdMapForPicking()) UE_LOG(LogWarRoom, Error, TEXT("provinces.png could not be read; picking will not work"));

	ColorLUT = MakeLut(); InfoLUT = MakeLut(); FlagLUT = MakeLut();
	ColorBytes.SetNumZeroed(kLut * kLut * 4); InfoBytes.SetNumZeroed(kLut * kLut * 4); FlagBytes.SetNumZeroed(kLut * kLut * 4);

	if (GlobeBaseMaterial)
	{
		Material = UMaterialInstanceDynamic::Create(GlobeBaseMaterial, this);
		Material->SetTextureParameterValue(TEXT("ColorLUT"), ColorLUT);
		Material->SetTextureParameterValue(TEXT("InfoLUT"), InfoLUT);
		Material->SetTextureParameterValue(TEXT("FlagLUT"), FlagLUT);
		Material->SetScalarParameterValue(TEXT("BorderLevel"), 2.f);
		Material->SetScalarParameterValue(TEXT("Night"), 0.55f);
		Material->SetScalarParameterValue(TEXT("ReliefStrength"), 1.1f);
		Mesh->SetMaterial(0, Material);
	}
	else
	{
		UE_LOG(LogWarRoom, Error, TEXT("M_Globe is missing: run Scripts/setup_content.py in the editor (Tools > Execute Python Script)"));
	}

	if (UWarRoomSubsystem* S = Sim())
	{
		S->OnStateReplaced.AddUObject(this, &AGlobeActor::RefreshCounters);
		S->OnStateReplaced.AddLambda([this]() { PaintInfo(); Repaint(); });
		PaintInfo(); Repaint(); RefreshCounters();
	}
}

void AGlobeActor::BuildSphere()
{
	const int32 Segs = 192, Rings = 96;
	TArray<FVector> Verts; TArray<int32> Tris; TArray<FVector> Normals; TArray<FVector2D> UVs; TArray<FProcMeshTangent> Tangents; TArray<FLinearColor> Colors;
	for (int32 r = 0; r <= Rings; r++)
	{
		const double v = (double)r / Rings;               // 0 at the north pole
		const double Lat = 90.0 - v * 180.0;
		for (int32 s = 0; s <= Segs; s++)
		{
			const double u = (double)s / Segs;            // 0 at lon -180
			const double Lon = u * 360.0 - 180.0;
			const FVector N = LatLonToWorld(Lat, Lon, 0.0) / Radius;
			Verts.Add(N * Radius); Normals.Add(N); UVs.Add(FVector2D(u, v));
			const FVector East = FVector(-FMath::Sin(FMath::DegreesToRadians(Lon)), FMath::Cos(FMath::DegreesToRadians(Lon)), 0.0);
			Tangents.Add(FProcMeshTangent(East, false));
		}
	}
	for (int32 r = 0; r < Rings; r++)
	{
		for (int32 s = 0; s < Segs; s++)
		{
			const int32 a = r * (Segs + 1) + s, b = a + Segs + 1;
			Tris.Append({a, a + 1, b}); Tris.Append({a + 1, b + 1, b});
		}
	}
	Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
}

FVector AGlobeActor::LatLonToWorld(double Lat, double Lon, double Altitude) const
{
	// Unreal: X forward, Y right, Z up. Lon 0 points down +X, lon 90 down +Y (east), north is +Z.
	const double La = FMath::DegreesToRadians(Lat), Lo = FMath::DegreesToRadians(Lon);
	const FVector Dir(FMath::Cos(La) * FMath::Cos(Lo), FMath::Cos(La) * FMath::Sin(Lo), FMath::Sin(La));
	return GetActorLocation() + Dir * (Radius + Altitude);
}

void AGlobeActor::WorldDirToLatLon(const FVector& D, double& OutLat, double& OutLon)
{
	const FVector N = D.GetSafeNormal();
	OutLat = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(N.Z, -1.0, 1.0)));
	OutLon = FMath::RadiansToDegrees(FMath::Atan2(N.Y, N.X));
}

bool AGlobeActor::LoadIdMapForPicking()
{
	const FString Path = FPaths::Combine(UWarRoomSubsystem::FindDataDir(), TEXT("map"), TEXT("provinces.png"));
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return false;
	IImageWrapperModule& Module = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::PNG);
	if (!Wrapper.IsValid() || !Wrapper->SetCompressed(Bytes.GetData(), Bytes.Num())) return false;
	TArray<uint8> Raw;
	if (!Wrapper->GetRaw(ERGBFormat::RGBA, 8, Raw)) return false;
	IdMapW = Wrapper->GetWidth(); IdMapH = Wrapper->GetHeight();
	IdMap.SetNumUninitialized(IdMapW * IdMapH);
	for (int32 i = 0, j = 0; i < IdMap.Num(); i++, j += 4) IdMap[i] = (uint16)(Raw[j] | (Raw[j + 1] << 8));
	return true;
}

int32 AGlobeActor::PickAtWorld(const FVector& WorldPoint) const
{
	if (IdMap.Num() == 0) return 0;
	double Lat, Lon; WorldDirToLatLon(WorldPoint - GetActorLocation(), Lat, Lon);
	const int32 X = FMath::Clamp((int32)((Lon + 180.0) / 360.0 * IdMapW), 0, IdMapW - 1);
	const int32 Y = FMath::Clamp((int32)((90.0 - Lat) / 180.0 * IdMapH), 0, IdMapH - 1);
	const int32 Id = IdMap[Y * IdMapW + X];
	const UWarRoomSubsystem* S = Sim();
	return (S && S->IsReady() && Id <= S->GetData().provinceCount) ? Id : 0;
}

int32 AGlobeActor::PickAtScreen(APlayerController* PC, const FVector2D& ScreenPos) const
{
	FVector Origin, Dir;
	if (!PC || !PC->DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, Origin, Dir)) return 0;
	// ray/sphere
	const FVector C = GetActorLocation();
	const FVector OC = Origin - C;
	const double B = FVector::DotProduct(OC, Dir), Cc = FVector::DotProduct(OC, OC) - (double)Radius * Radius;
	const double Disc = B * B - Cc;
	if (Disc < 0) return 0;
	const double T = -B - FMath::Sqrt(Disc);
	if (T < 0) return 0;
	return PickAtWorld(Origin + Dir * T);
}

UTexture2D* AGlobeActor::MakeLut()
{
	UTexture2D* T = UTexture2D::CreateTransient(kLut, kLut, PF_B8G8R8A8);
	T->Filter = TF_Nearest; T->SRGB = false; T->NeverStream = true;
	T->UpdateResource();
	return T;
}

void AGlobeActor::UploadLut(UTexture2D* Tex, const TArray<uint8>& Bytes)
{
	if (!Tex || !Tex->GetPlatformData()) return;
	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void* Dst = Mip.BulkData.Lock(LOCK_READ_WRITE);
	// our bytes are RGBA; the texture is BGRA
	uint8* D = (uint8*)Dst;
	for (int32 i = 0; i < Bytes.Num(); i += 4) { D[i] = Bytes[i + 2]; D[i + 1] = Bytes[i + 1]; D[i + 2] = Bytes[i]; D[i + 3] = Bytes[i + 3]; }
	Mip.BulkData.Unlock();
	Tex->UpdateResource();
}

static void HexToRgb(const std::string& Hex, uint8& R, uint8& G, uint8& B)
{
	auto H = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : (c >= 'a' && c <= 'f' ? c - 'a' + 10 : (c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0)); };
	if (Hex.size() < 7) { R = G = B = 128; return; }
	R = (uint8)(H(Hex[1]) * 16 + H(Hex[2])); G = (uint8)(H(Hex[3]) * 16 + H(Hex[4])); B = (uint8)(H(Hex[5]) * 16 + H(Hex[6]));
}

static FColor HashColor(int32 N, float S = 0.55f, float L = 0.55f)
{
	const float Hh = FMath::Fmod((float)N * 0.618033988749895f, 1.f) * 360.f;
	return FLinearColor::MakeFromHSV8((uint8)(Hh / 360.f * 255.f), (uint8)(S * 255.f), (uint8)(L * 255.f)).ToFColor(false);
}

void AGlobeActor::Repaint()
{
	UWarRoomSubsystem* S = Sim();
	if (!S || !S->IsReady()) return;
	const warroom::Data& D = S->GetData();
	const warroom::GameState& St = S->GetState();
	static const TMap<FString, FColor> TerrainColors = {
		{TEXT("plains"), FColor(168, 196, 118)}, {TEXT("forest"), FColor(66, 122, 66)}, {TEXT("hills"), FColor(168, 140, 92)}, {TEXT("mountain"), FColor(128, 118, 118)},
		{TEXT("urban"), FColor(196, 88, 88)}, {TEXT("jungle"), FColor(34, 112, 54)}, {TEXT("marsh"), FColor(102, 150, 140)}, {TEXT("desert"), FColor(228, 208, 150)},
		{TEXT("wasteland"), FColor(238, 240, 248)}, {TEXT("ocean"), FColor(38, 66, 112)}, {TEXT("lake"), FColor(78, 128, 196)} };
	static const TMap<FString, FColor> WeatherColors = {
		{TEXT("clear"), FColor(230, 220, 150)}, {TEXT("rain"), FColor(90, 130, 200)}, {TEXT("storm"), FColor(60, 70, 140)}, {TEXT("snow"), FColor(240, 244, 255)},
		{TEXT("blizzard"), FColor(200, 210, 240)}, {TEXT("sandstorm"), FColor(220, 170, 90)}, {TEXT("mud"), FColor(130, 100, 60)}, {TEXT("frost"), FColor(170, 210, 240)} };
	const FColor Sea(38, 66, 112), Lake(78, 128, 196), Waste(228, 230, 238);
	std::map<std::string, FColor> Owner;
	for (const auto& [Tag, C] : St.countries) { uint8 r, g, b; HexToRgb(C.color, r, g, b); Owner[Tag] = FColor(r, g, b); }
	auto Mix = [](FColor A, FColor B, float K) { return FColor((uint8)(A.R + (B.R - A.R) * K), (uint8)(A.G + (B.G - A.G) * K), (uint8)(A.B + (B.B - A.B) * K)); };
	for (int32 Id = 1; Id <= D.provinceCount; Id++)
	{
		const warroom::Province& P = D.provinces[Id];
		FColor C;
		const FString Weather = UTF8_TO_TCHAR(St.regions.count(P.region) ? St.regions.at(P.region).weather.c_str() : "clear");
		if (P.type == "sea") C = MapMode == EWarRoomMapMode::Regions ? Mix(Sea, HashColor(P.region, 0.4f, 0.4f), 0.6f) : MapMode == EWarRoomMapMode::Weather ? Mix(Sea, WeatherColors.FindRef(Weather), 0.5f) : Sea;
		else if (P.type == "lake") C = Lake;
		else if (P.terrain == "wasteland" && MapMode != EWarRoomMapMode::Terrain) C = Waste;
		else
		{
			const warroom::StateState& SS = St.states.at(P.state);
			switch (MapMode)
			{
			case EWarRoomMapMode::Terrain: C = TerrainColors.FindRef(FString(UTF8_TO_TCHAR(P.terrain.c_str()))); break;
			case EWarRoomMapMode::States: C = HashColor(P.state); break;
			case EWarRoomMapMode::Regions: C = HashColor(P.region * 7 + 3, 0.5f, 0.5f); break;
			case EWarRoomMapMode::Weather: C = WeatherColors.FindRef(Weather); break;
			case EWarRoomMapMode::VictoryPoints: C = P.vp >= 15 ? FColor(220, 60, 60) : P.vp >= 5 ? FColor(230, 150, 70) : P.vp >= 1 ? FColor(230, 210, 120) : FColor(120, 130, 120); break;
			default:
				C = Owner.count(SS.controller) ? Owner[SS.controller] : FColor(128, 128, 128);
				if (SS.controller != SS.owner && Owner.count(SS.owner)) C = Mix(C, Owner[SS.owner], 0.35f);
			}
		}
		const int32 O = Id * 4;
		ColorBytes[O] = C.R; ColorBytes[O + 1] = C.G; ColorBytes[O + 2] = C.B; ColorBytes[O + 3] = 255;
	}
	UploadLut(ColorLUT, ColorBytes);
}

void AGlobeActor::PaintInfo()
{
	UWarRoomSubsystem* S = Sim();
	if (!S || !S->IsReady()) return;
	const warroom::Data& D = S->GetData();
	const warroom::GameState& St = S->GetState();
	std::map<std::string, int32> OwnerIndex; int32 N = 1;
	for (const auto& [Tag, C] : St.countries) OwnerIndex[Tag] = N++;
	for (int32 Id = 1; Id <= D.provinceCount; Id++)
	{
		const warroom::Province& P = D.provinces[Id];
		int32 Sid = 0, Own = 0;
		if (P.type == "land") { Sid = P.state; const std::string& Ctrl = St.states.at(P.state).controller; Own = OwnerIndex.count(Ctrl) ? OwnerIndex[Ctrl] : 0; }
		else if (P.type == "sea") { Sid = 0xffff; Own = 0xfffe; }
		else if (P.type == "lake") { Sid = 0xfffe; Own = 0xfffd; }
		const int32 O = Id * 4;
		InfoBytes[O] = Sid & 255; InfoBytes[O + 1] = (Sid >> 8) & 255; InfoBytes[O + 2] = Own & 255; InfoBytes[O + 3] = (Own >> 8) & 255;
	}
	UploadLut(InfoLUT, InfoBytes);
}

void AGlobeActor::Highlight(const TArray<int32>& Ids, bool bDimOthers)
{
	FMemory::Memzero(FlagBytes.GetData(), FlagBytes.Num());
	if (bDimOthers) for (int32 i = 1; i < kLut * kLut; i++) FlagBytes[i * 4 + 1] = 255;
	for (int32 Id : Ids) { FlagBytes[Id * 4] = 255; FlagBytes[Id * 4 + 1] = 0; }
	UploadLut(FlagLUT, FlagBytes);
}

void AGlobeActor::SetMapMode(EWarRoomMapMode Mode) { MapMode = Mode; Repaint(); }
void AGlobeActor::SetHover(int32 Id) { if (Material) Material->SetScalarParameterValue(TEXT("HoverId"), (float)Id); }
void AGlobeActor::SetSelected(int32 Id) { if (Material) Material->SetScalarParameterValue(TEXT("SelectedId"), (float)Id); }
void AGlobeActor::SetBorderLevel(float Level) { if (Material) Material->SetScalarParameterValue(TEXT("BorderLevel"), Level); }

void AGlobeActor::RefreshCounters()
{
	UWarRoomSubsystem* S = Sim();
	if (!S || !S->IsReady()) return;
	for (UTextRenderComponent* L : CounterLabels) if (L) L->DestroyComponent();
	CounterLabels.Reset();
	Counters->ClearInstances();
	const warroom::Data& D = S->GetData();
	const warroom::GameState& St = S->GetState();
	const double Scale = Radius * 0.00035;   // counter ~1/3000th of the radius, scaled up by the camera later
	for (const auto& [Tag, List] : St.armies)
	{
		uint8 r, g, b; HexToRgb(St.countries.at(Tag).color, r, g, b);
		for (const warroom::Army& A : List)
		{
			const warroom::StateDef* Sd = D.state(A.state);
			if (!Sd) continue;
			const warroom::Province& P = D.provinces[Sd->capital_province];
			const FVector Pos = LatLonToWorld(P.lat, P.lon, 0.0);
			const FVector Up = (Pos - GetActorLocation()).GetSafeNormal();
			const FTransform T(FRotationMatrix::MakeFromZ(Up).ToQuat(), Pos, FVector(Scale));
			const int32 Idx = Counters->AddInstance(T, true);
			Counters->SetCustomDataValue(Idx, 0, r / 255.f); Counters->SetCustomDataValue(Idx, 1, g / 255.f); Counters->SetCustomDataValue(Idx, 2, b / 255.f);
			UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this);
			Label->RegisterComponent();
			Label->AttachToComponent(Mesh, FAttachmentTransformRules::KeepWorldTransform);
			Label->SetWorldLocation(Pos + Up * Scale * 1.2);
			Label->SetWorldRotation(FRotationMatrix::MakeFromZX(Up, -Up).Rotator());
			Label->SetText(FText::AsNumber(A.divisions));
			Label->SetHorizontalAlignment(EHTA_Center); Label->SetVerticalAlignment(EVRTA_TextCenter);
			Label->SetWorldSize(Scale * 0.9);
			Label->SetTextRenderColor(FColor::White);
			CounterLabels.Add(Label);
		}
	}
}

void AGlobeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWarRoomSubsystem* S = Sim();
	if (!S || !S->IsReady() || !Material) return;
	// sun from the clock
	const warroom::Date Dt = warroom::HourToDate(S->GetState().hour);
	const double Decl = -23.44 * FMath::Cos((2.0 * PI / 365.0) * (Dt.dayOfYear + 10));
	const double SunLon = 180.0 - Dt.hour * 15.0;
	const FVector Sun = (LatLonToWorld(Decl, SunLon, 0.0) - GetActorLocation()).GetSafeNormal();
	Material->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(Sun.X, Sun.Y, Sun.Z, 0));
	if (S->bRegionsDirty && MapMode == EWarRoomMapMode::Weather) { Repaint(); S->bRegionsDirty = false; }
	// counters face the camera at a readable size
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FVector CamLoc; FRotator CamRot; PC->GetPlayerViewPoint(CamLoc, CamRot);
		const double Dist = (CamLoc - GetActorLocation()).Size() - Radius;
		const double K = FMath::Clamp(Dist / (Radius * 2.0), 0.08, 1.0);
		const bool bVisible = Dist < Radius * 2.2;
		Counters->SetVisibility(bVisible);
		for (int32 i = 0; i < Counters->GetInstanceCount(); i++)
		{
			FTransform T; Counters->GetInstanceTransform(i, T, true);
			T.SetScale3D(FVector(Radius * 0.035 * K));
			Counters->UpdateInstanceTransform(i, T, true, i == Counters->GetInstanceCount() - 1, false);
			if (CounterLabels.IsValidIndex(i) && CounterLabels[i])
			{
				const FVector Up = (T.GetLocation() - GetActorLocation()).GetSafeNormal();
				CounterLabels[i]->SetVisibility(bVisible);
				CounterLabels[i]->SetWorldLocation(T.GetLocation() + Up * Radius * 0.035 * K * 1.3);
				CounterLabels[i]->SetWorldSize(Radius * 0.035 * K * 0.9);
				CounterLabels[i]->SetWorldRotation(FRotationMatrix::MakeFromXZ(CamLoc - T.GetLocation(), Up).Rotator());
			}
		}
	}
}
