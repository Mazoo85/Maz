// The interface, in Slate: the top bar (country, date, speed), the map-mode bar, the
// side panel (province / state / country / saves), the search box.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class AGlobeActor;
class AOrbitCameraPawn;
class UWarRoomSubsystem;
class STextBlock;
class SVerticalBox;
class SBorder;
class SEditableTextBox;

class WARROOM_API SWarRoomHUD : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWarRoomHUD) {}
		SLATE_ARGUMENT(AGlobeActor*, Globe)
		SLATE_ARGUMENT(AOrbitCameraPawn*, Camera)
		SLATE_ARGUMENT(UWarRoomSubsystem*, Subsystem)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	void OpenProvince(int32 Id, bool bFly = true);
	void OpenState(int32 Id);
	void OpenCountry(const FString& Tag);
	void OpenSaves();
	void ClosePanel();
	void Toast(const FText& Message);

private:
	TWeakObjectPtr<AGlobeActor> Globe;
	TWeakObjectPtr<AOrbitCameraPawn> Camera;
	TWeakObjectPtr<UWarRoomSubsystem> Sim;

	TSharedPtr<STextBlock> DateText, PlayerText, ToastText;
	TSharedPtr<SBorder> PlayerFlag, PanelBorder, ToastBorder;
	TSharedPtr<SVerticalBox> PanelBox, SearchResults;
	TSharedPtr<SEditableTextBox> SearchBox, SaveNameBox;
	TArray<TSharedPtr<class SButton>> SpeedButtons, ModeButtons;
	int32 Selected = 0;
	double ToastUntil = 0;
	long long LastHour = -1; int32 LastSpeed = -1; FString LastPlayer = TEXT("?");

	AOrbitCameraPawn* Cam() const;
	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeModeBar();
	TSharedRef<SWidget> Row(const FText& Key, TSharedRef<SWidget> Value);
	TSharedRef<SWidget> Row(const FText& Key, const FText& Value);
	TSharedRef<SWidget> Link(const FText& Label, TFunction<void()> OnClick);
	TSharedRef<SWidget> Chip(const FText& Label, TFunction<void()> OnClick);
	TSharedRef<SWidget> FlagBox(const FString& Hex);
	TSharedRef<SWidget> Heading(const FText& Text);
	void ShowPanel(const FText& Title, TSharedRef<SWidget> Body);
	void OnSearchChanged(const FText& Text);
	void OnSearchCommitted(const FText& Text, ETextCommit::Type Commit);
	void SetSpeed(int32 Speed);
	void SetMode(int32 Mode);
	FText CountryName(const std::string& Tag) const;
	static FText S(const std::string& Str) { return FText::FromString(UTF8_TO_TCHAR(Str.c_str())); }
	static FString F(const std::string& Str) { return FString(UTF8_TO_TCHAR(Str.c_str())); }
	static FText Cap(const std::string& Str);
};
