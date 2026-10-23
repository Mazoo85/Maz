#include "SWarRoomHUD.h"

#include "GlobeActor.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "OrbitCameraPawn.h"
#include "Styling/CoreStyle.h"
#include "WarRoomSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WarRoom"

static const FLinearColor kPanel(0.08f, 0.11f, 0.15f, 0.92f);
static const FLinearColor kAccent(0.85f, 0.66f, 0.29f, 1.f);
static const FLinearColor kInkDim(0.6f, 0.65f, 0.7f, 1.f);

static FLinearColor HexColor(const FString& Hex)
{
	FColor C = FColor::FromHex(Hex);
	return FLinearColor(C);
}

FText SWarRoomHUD::Cap(const std::string& Str)
{
	FString Fs = UTF8_TO_TCHAR(Str.c_str());
	Fs = Fs.Replace(TEXT("_"), TEXT("-"));
	if (Fs.Len()) Fs[0] = FChar::ToUpper(Fs[0]);
	return FText::FromString(Fs);
}

void SWarRoomHUD::Construct(const FArguments& InArgs)
{
	Globe = InArgs._Globe; Camera = InArgs._Camera; Sim = InArgs._Subsystem;
	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Top) [ MakeTopBar() ]
		+ SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(10) [ MakeModeBar() ]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(10, 60, 10, 80)
		[
			SAssignNew(PanelBorder, SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(kPanel).Padding(12).Visibility(EVisibility::Collapsed)
			[
				SNew(SBox).WidthOverride(380).MaxDesiredHeight(640)
				[
					SNew(SScrollBox) + SScrollBox::Slot() [ SAssignNew(PanelBox, SVerticalBox) ]
				]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(10, 48, 10, 0)
		[
			SNew(SBox).WidthOverride(320)
			[
				SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(kPanel).Padding(4)
				.Visibility_Lambda([this]() { return SearchResults.IsValid() && SearchResults->NumSlots() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
				[ SAssignNew(SearchResults, SVerticalBox) ]
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0, 0, 0, 60)
		[
			SAssignNew(ToastBorder, SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(kPanel).Padding(FMargin(14, 6)).Visibility(EVisibility::Collapsed)
			[ SAssignNew(ToastText, STextBlock) ]
		]
	];
	if (Sim.IsValid() && !Sim->IsReady())
	{
		ShowPanel(LOCTEXT("CannotStart", "War Room could not start"), SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Sim->GetLoadError())));
	}
}

TSharedRef<SWidget> SWarRoomHUD::MakeTopBar()
{
	TSharedRef<SHorizontalBox> Speeds = SNew(SHorizontalBox);
	const TCHAR* Labels[6] = {TEXT("||"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5")};
	for (int32 i = 0; i < 6; i++)
	{
		TSharedPtr<SButton> B;
		Speeds->AddSlot().AutoWidth().Padding(1, 0)
		[
			SAssignNew(B, SButton).Text(FText::FromString(Labels[i])).OnClicked_Lambda([this, i]() { SetSpeed(i); return FReply::Handled(); })
		];
		SpeedButtons.Add(B);
	}
	return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(kPanel).Padding(FMargin(10, 6))
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 6, 0) [ SAssignNew(PlayerFlag, SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f)).Padding(0) [ SNew(SBox).WidthOverride(24).HeightOverride(16) ] ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").OnClicked_Lambda([this]() {
				if (Sim.IsValid() && !Sim->GetState().player.empty()) OpenCountry(F(Sim->GetState().player)); else Toast(LOCTEXT("PickCountry", "Pick a country: click one on the map, then \"Play as\""));
				return FReply::Handled(); })
			[ SAssignNew(PlayerText, STextBlock).Text(LOCTEXT("Observer", "Observer")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 12)) ]
		]
		+ SHorizontalBox::Slot().FillWidth(1) [ SNullWidget::NullWidget ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0) [ SAssignNew(DateText, STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ Speeds ]
		+ SHorizontalBox::Slot().FillWidth(1) [ SNullWidget::NullWidget ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0)
		[
			SNew(SBox).WidthOverride(180)
			[ SAssignNew(SearchBox, SEditableTextBox).HintText(LOCTEXT("Find", "Find a place")).OnTextChanged(this, &SWarRoomHUD::OnSearchChanged).OnTextCommitted(this, &SWarRoomHUD::OnSearchCommitted) ]
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(SButton).Text(LOCTEXT("Saves", "Saves")).OnClicked_Lambda([this]() { OpenSaves(); return FReply::Handled(); }) ]
	];
}

TSharedRef<SWidget> SWarRoomHUD::MakeModeBar()
{
	TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	const FText Names[6] = {LOCTEXT("Political", "Political"), LOCTEXT("Terrain", "Terrain"), LOCTEXT("States", "States"), LOCTEXT("Regions", "Regions"), LOCTEXT("Weather", "Weather"), LOCTEXT("VP", "Victory points")};
	for (int32 i = 0; i < 6; i++)
	{
		TSharedPtr<SButton> B;
		Box->AddSlot().AutoWidth().Padding(2, 0) [ SAssignNew(B, SButton).Text(Names[i]).OnClicked_Lambda([this, i]() { SetMode(i); return FReply::Handled(); }) ];
		ModeButtons.Add(B);
	}
	SetMode(0);
	return Box;
}

AOrbitCameraPawn* SWarRoomHUD::Cam() const
{
	if (Camera.IsValid()) return Camera.Get();
	if (Globe.IsValid()) return Cast<AOrbitCameraPawn>(UGameplayStatics::GetPlayerPawn(Globe.Get(), 0));
	return nullptr;
}

void SWarRoomHUD::SetSpeed(int32 Speed) { if (Sim.IsValid()) { const FString R = Sim->SetSpeed(Speed); if (!R.IsEmpty()) Toast(FText::FromString(R)); } }

void SWarRoomHUD::SetMode(int32 Mode)
{
	for (int32 i = 0; i < ModeButtons.Num(); i++) if (ModeButtons[i].IsValid()) ModeButtons[i]->SetBorderBackgroundColor(i == Mode ? kAccent : FLinearColor::White);
	if (Globe.IsValid()) Globe->SetMapMode((EWarRoomMapMode)Mode);
}

void SWarRoomHUD::Tick(const FGeometry& G, const double Now, const float Dt)
{
	SCompoundWidget::Tick(G, Now, Dt);
	if (!Sim.IsValid() || !Sim->IsReady()) return;
	const warroom::GameState& St = Sim->GetState();
	if (St.hour != LastHour) { DateText->SetText(S(warroom::FormatDate(St.hour))); LastHour = St.hour; }
	if (St.speed != LastSpeed)
	{
		for (int32 i = 0; i < SpeedButtons.Num(); i++) if (SpeedButtons[i].IsValid()) SpeedButtons[i]->SetBorderBackgroundColor(i == St.speed ? kAccent : FLinearColor::White);
		LastSpeed = St.speed;
	}
	if (F(St.player) != LastPlayer)
	{
		LastPlayer = F(St.player);
		if (St.player.empty()) { PlayerText->SetText(LOCTEXT("Observer", "Observer")); PlayerFlag->SetBorderBackgroundColor(FLinearColor(0.4f, 0.4f, 0.4f)); }
		else { const auto& C = St.countries.at(St.player); PlayerText->SetText(S(C.name)); PlayerFlag->SetBorderBackgroundColor(HexColor(F(C.color))); }
	}
	if (ToastBorder.IsValid() && ToastBorder->GetVisibility() == EVisibility::Visible && Now > ToastUntil) ToastBorder->SetVisibility(EVisibility::Collapsed);
}

void SWarRoomHUD::Toast(const FText& Message)
{
	if (!ToastText.IsValid()) return;
	ToastText->SetText(Message); ToastBorder->SetVisibility(EVisibility::Visible);
	ToastUntil = FSlateApplication::Get().GetCurrentTime() + 3.0;
}

// ---- panel pieces ---------------------------------------------------------------

TSharedRef<SWidget> SWarRoomHUD::Heading(const FText& Text)
{
	return SNew(STextBlock).Text(Text.ToUpper()).ColorAndOpacity(kInkDim).Font(FCoreStyle::GetDefaultFontStyle("Bold", 9)).Margin(FMargin(0, 10, 0, 2));
}

TSharedRef<SWidget> SWarRoomHUD::Row(const FText& Key, TSharedRef<SWidget> Value)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ SNew(SBox).WidthOverride(110) [ SNew(STextBlock).Text(Key).ColorAndOpacity(kInkDim) ] ]
		+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ Value ];
}

TSharedRef<SWidget> SWarRoomHUD::Row(const FText& Key, const FText& Value) { return Row(Key, SNew(STextBlock).Text(Value).AutoWrapText(true)); }

TSharedRef<SWidget> SWarRoomHUD::Link(const FText& Label, TFunction<void()> OnClick)
{
	return SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").ContentPadding(0).OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[ SNew(STextBlock).Text(Label).ColorAndOpacity(kAccent) ];
}

TSharedRef<SWidget> SWarRoomHUD::Chip(const FText& Label, TFunction<void()> OnClick)
{
	return SNew(SButton).ContentPadding(FMargin(6, 2)).OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); }) [ SNew(STextBlock).Text(Label).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)) ];
}

TSharedRef<SWidget> SWarRoomHUD::FlagBox(const FString& Hex)
{
	return SNew(SBox).WidthOverride(18).HeightOverride(12).Padding(FMargin(0, 0, 4, 0)) [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HexColor(Hex)) ];
}

FText SWarRoomHUD::CountryName(const std::string& Tag) const
{
	if (!Sim.IsValid()) return FText::GetEmpty();
	auto It = Sim->GetState().countries.find(Tag);
	return It == Sim->GetState().countries.end() ? S(Tag) : S(It->second.name);
}

void SWarRoomHUD::ShowPanel(const FText& Title, TSharedRef<SWidget> Body)
{
	PanelBox->ClearChildren();
	PanelBox->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1) [ SNew(STextBlock).Text(Title).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14)).AutoWrapText(true) ]
		+ SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").Text(FText::FromString(TEXT("X"))).OnClicked_Lambda([this]() { ClosePanel(); return FReply::Handled(); }) ]
	];
	PanelBox->AddSlot().AutoHeight().Padding(0, 6, 0, 0) [ Body ];
	PanelBorder->SetVisibility(EVisibility::Visible);
}

void SWarRoomHUD::ClosePanel()
{
	PanelBorder->SetVisibility(EVisibility::Collapsed);
	Selected = 0;
	if (Globe.IsValid()) { Globe->SetSelected(0); Globe->Highlight({}, false); }
}

// ---- views ----------------------------------------------------------------------

void SWarRoomHUD::OpenProvince(int32 Id, bool bFly)
{
	if (!Sim.IsValid() || !Sim->IsReady()) return;
	const warroom::Data& D = Sim->GetData();
	const warroom::GameState& St = Sim->GetState();
	const warroom::Province* P = D.province(Id);
	if (!P) return;
	Selected = Id;
	if (Globe.IsValid()) { Globe->SetSelected(Id); Globe->Highlight({}, false); }
	if (AOrbitCameraPawn* Cp = bFly ? Cam() : nullptr) Cp->FlyTo(P->lat, P->lon, FMath::Min(Cp->Altitude() + 1.0, 1.35));

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto Add = [&](TSharedRef<SWidget> W) { Body->AddSlot().AutoHeight().Padding(0, 2) [ W ]; };
	Add(Row(LOCTEXT("Type", "Type"), P->type == "land" ? Cap(P->terrain) : Cap(P->type)));
	if (const warroom::StateDef* Sd = D.state(P->state))
	{
		const int32 Sid = Sd->id;
		Add(Row(LOCTEXT("State", "State"), Link(S(Sd->name), [this, Sid]() { OpenState(Sid); })));
		const warroom::StateState& SS = St.states.at(Sid);
		const std::string Ctrl = SS.controller, Own = SS.owner;
		Add(Row(LOCTEXT("ControlledBy", "Controlled by"), SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ FlagBox(F(St.countries.at(Ctrl).color)) ] + SHorizontalBox::Slot().AutoWidth() [ Link(CountryName(Ctrl), [this, Ctrl]() { OpenCountry(F(Ctrl)); }) ]));
		if (Own != Ctrl) Add(Row(LOCTEXT("OwnedBy", "Owned by"), Link(CountryName(Own), [this, Own]() { OpenCountry(F(Own)); })));
	}
	if (P->vp) Add(Row(LOCTEXT("VictoryPoints", "Victory points"), FText::AsNumber(P->vp)));
	if (const warroom::Region* R = D.region(P->region)) Add(Row(LOCTEXT("Region", "Region"), S(R->name)));
	Add(Row(LOCTEXT("Weather", "Weather"), Cap(warroom::WeatherAt(St, D, Id))));
	if (P->coastal) Add(Row(LOCTEXT("Coast", "Coast"), LOCTEXT("Yes", "yes")));
	Add(Row(LOCTEXT("Position", "Position"), FText::FromString(FString::Printf(TEXT("%.2f°, %.2f°"), P->lat, P->lon))));
	Add(Heading(LOCTEXT("Neighbours", "Neighbours")));
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3, 3));
	for (int32 N : D.adjacency[Id])
	{
		const FString Name = F(D.provinces[N].name) + (D.isRiver(Id, N) ? TEXT(" ~") : TEXT(""));
		Wrap->AddSlot() [ Chip(FText::FromString(Name), [this, N]() { OpenProvince(N); }) ];
	}
	Add(Wrap);
	ShowPanel(S(P->name), Body);
}

void SWarRoomHUD::OpenState(int32 Id)
{
	if (!Sim.IsValid() || !Sim->IsReady()) return;
	const warroom::Data& D = Sim->GetData();
	const warroom::GameState& St = Sim->GetState();
	const warroom::StateDef* Sd = D.state(Id);
	if (!Sd) return;
	const warroom::StateState& SS = St.states.at(Id);
	Selected = 0;
	if (Globe.IsValid()) { Globe->SetSelected(0); TArray<int32> Ids; for (int p : Sd->provinces) Ids.Add(p); Globe->Highlight(Ids, false); }
	if (AOrbitCameraPawn* Cm = Cam()) { const auto& Cp = D.provinces[Sd->capital_province]; Cm->FlyTo(Cp.lat, Cp.lon, FMath::Min(Cm->Altitude() + 1.0, 1.6)); }

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto Add = [&](TSharedRef<SWidget> W) { Body->AddSlot().AutoHeight().Padding(0, 2) [ W ]; };
	const std::string Own = SS.owner, Ctrl = SS.controller;
	Add(Row(LOCTEXT("Owner", "Owner"), Link(CountryName(Own), [this, Own]() { OpenCountry(F(Own)); })));
	if (Ctrl != Own) Add(Row(LOCTEXT("Controller", "Controller"), Link(CountryName(Ctrl), [this, Ctrl]() { OpenCountry(F(Ctrl)); })));
	FString Cores; for (const auto& T : SS.cores) { if (!Cores.IsEmpty()) Cores += TEXT(", "); Cores += CountryName(T).ToString(); }
	Add(Row(LOCTEXT("Cores", "Cores"), FText::FromString(Cores.IsEmpty() ? TEXT("none") : Cores)));
	const int32 CapId = Sd->capital_province;
	Add(Row(LOCTEXT("Capital", "Capital"), Link(S(D.provinces[CapId].name), [this, CapId]() { OpenProvince(CapId); })));
	Add(Row(LOCTEXT("Manpower", "Manpower"), FText::AsNumber((int64)Sd->manpower)));
	Add(Row(LOCTEXT("Infrastructure", "Infrastructure"), FText::FromString(FString::Printf(TEXT("%d / 5"), SS.infrastructure))));
	Add(Row(LOCTEXT("Provinces", "Provinces"), FText::AsNumber((int32)Sd->provinces.size())));
	Add(Heading(LOCTEXT("Provinces", "Provinces")));
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3, 3));
	for (int32 Pid : Sd->provinces) Wrap->AddSlot() [ Chip(FText::FromString(F(D.provinces[Pid].name) + (D.provinces[Pid].vp ? TEXT(" *") : TEXT(""))), [this, Pid]() { OpenProvince(Pid); }) ];
	Add(Wrap);
	ShowPanel(S(Sd->name), Body);
}

void SWarRoomHUD::OpenCountry(const FString& Tag)
{
	if (!Sim.IsValid() || !Sim->IsReady()) return;
	const warroom::Data& D = Sim->GetData();
	const warroom::GameState& St = Sim->GetState();
	const std::string T = TCHAR_TO_UTF8(*Tag);
	auto It = St.countries.find(T);
	if (It == St.countries.end()) return;
	const warroom::CountryState& C = It->second;
	Selected = 0;
	if (Globe.IsValid())
	{
		TArray<int32> Ids;
		for (int Sid : C.states) if (const warroom::StateDef* Sd = D.state(Sid)) for (int p : Sd->provinces) Ids.Add(p);
		Globe->SetSelected(0); Globe->Highlight(Ids, true);
	}
	if (AOrbitCameraPawn* Cm = Cam()) { const auto& Cp = D.provinces[D.country(T)->capital_province]; Cm->FlyTo(Cp.lat, Cp.lon, FMath::Max(Cm->Altitude() + 1.0, 2.2)); }

	int64 Manpower = 0, Provinces = 0, Cores = 0, Divisions = 0;
	for (int Sid : C.states) { const warroom::StateDef* Sd = D.state(Sid); if (!Sd) continue; Manpower += Sd->manpower; Provinces += Sd->provinces.size(); for (const auto& Cc : St.states.at(Sid).cores) if (Cc == T) Cores++; }
	if (auto A = St.armies.find(T); A != St.armies.end()) for (const auto& Ar : A->second) Divisions += Ar.divisions;

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto Add = [&](TSharedRef<SWidget> W) { Body->AddSlot().AutoHeight().Padding(0, 2) [ W ]; };
	Add(Row(LOCTEXT("Leader", "Leader"), S(C.leader)));
	Add(Row(LOCTEXT("Ideology", "Ideology"), Cap(C.ideology)));
	const int32 CapState = C.capital;
	if (const warroom::StateDef* Cs = D.state(CapState)) Add(Row(LOCTEXT("Capital", "Capital"), Link(S(Cs->name), [this, CapState]() { OpenState(CapState); })));
	Add(Row(LOCTEXT("States", "States"), FText::FromString(FString::Printf(TEXT("%d (%lld core)"), (int32)C.states.size(), Cores))));
	Add(Row(LOCTEXT("Provinces", "Provinces"), FText::AsNumber(Provinces)));
	Add(Row(LOCTEXT("Manpower", "Manpower"), FText::AsNumber(Manpower)));
	Add(Row(LOCTEXT("Divisions", "Divisions"), FText::AsNumber(Divisions)));
	const bool bPlaying = St.player == T;
	Add(SNew(SButton).IsEnabled(!bPlaying).ButtonColorAndOpacity(kAccent)
		.Text(bPlaying ? LOCTEXT("YouPlay", "You play this country") : FText::Format(LOCTEXT("PlayAs", "Play as {0}"), S(C.name)))
		.OnClicked_Lambda([this, Tag]() { if (Sim.IsValid()) { const FString R = Sim->SelectCountry(Tag); if (!R.IsEmpty()) Toast(FText::FromString(R)); } OpenCountry(Tag); return FReply::Handled(); }));
	Add(Heading(LOCTEXT("States", "States")));
	TSharedRef<SWrapBox> Wrap = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(3, 3));
	TArray<const warroom::StateDef*> Sorted;
	for (int Sid : C.states) if (const warroom::StateDef* Sd = D.state(Sid)) Sorted.Add(Sd);
	Sorted.Sort([](const warroom::StateDef& A, const warroom::StateDef& B) { return A.name < B.name; });
	for (const warroom::StateDef* Sd : Sorted)
	{
		const int32 Sid = Sd->id;
		bool bCore = false; for (const auto& Cc : St.states.at(Sid).cores) if (Cc == T) bCore = true;
		Wrap->AddSlot() [ Chip(FText::FromString(F(Sd->name) + (bCore ? TEXT("") : TEXT(" (non-core)"))), [this, Sid]() { OpenState(Sid); }) ];
	}
	Add(Wrap);
	ShowPanel(S(C.name), Body);
}

void SWarRoomHUD::OpenSaves()
{
	if (!Sim.IsValid()) return;
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);
	auto Add = [&](TSharedRef<SWidget> W) { Body->AddSlot().AutoHeight().Padding(0, 3) [ W ]; };
	const FString Current = Sim->GetState().player.empty() ? TEXT("My game") : F(Sim->GetState().countries.at(Sim->GetState().player).name);
	Add(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1) [ SAssignNew(SaveNameBox, SEditableTextBox).Text(FText::FromString(Current)).HintText(LOCTEXT("NameSave", "Name this save")) ]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0) [ SNew(SButton).ButtonColorAndOpacity(kAccent).Text(LOCTEXT("Save", "Save")).OnClicked_Lambda([this]() {
			FString Err; const FString Name = SaveNameBox->GetText().ToString().TrimStartAndEnd();
			if (Sim->SaveGame(Name.IsEmpty() ? TEXT("My game") : Name, Err)) Toast(FText::Format(LOCTEXT("Saved", "Saved \"{0}\""), FText::FromString(Name))); else Toast(FText::FromString(Err));
			OpenSaves(); return FReply::Handled(); }) ]);
	for (const FString& Name : Sim->ListSaves())
	{
		Add(SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center) [ SNew(STextBlock).Text(FText::FromString(Name)) ]
			+ SHorizontalBox::Slot().AutoWidth() [ SNew(SButton).Text(LOCTEXT("Load", "Load")).OnClicked_Lambda([this, Name]() { FString Err; if (Sim->LoadGame(Name, Err)) { Toast(FText::Format(LOCTEXT("Loaded", "Loaded \"{0}\""), FText::FromString(Name))); ClosePanel(); } else Toast(FText::FromString(Err)); return FReply::Handled(); }) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2, 0, 0, 0) [ SNew(SButton).Text(LOCTEXT("Delete", "Delete")).OnClicked_Lambda([this, Name]() { Sim->DeleteSave(Name); OpenSaves(); return FReply::Handled(); }) ]);
	}
	Add(SNew(STextBlock).AutoWrapText(true).ColorAndOpacity(kInkDim).Text(LOCTEXT("SavesWhere", "Saves are files in the project's Saved/SaveGames folder; copy one to another computer to move a game.")));
	Add(SNew(SButton).Text(LOCTEXT("NewGame", "New game (1936)")).OnClicked_Lambda([this]() { Sim->NewGame(FString::Printf(TEXT("%lld"), (long long)FDateTime::UtcNow().GetTicks())); ClosePanel(); Toast(LOCTEXT("NewGameStarted", "A new game has begun: 1 January 1936")); return FReply::Handled(); }));
	ShowPanel(LOCTEXT("SavesTitle", "Saves"), Body);
}

// ---- search ---------------------------------------------------------------------

void SWarRoomHUD::OnSearchChanged(const FText& Text)
{
	SearchResults->ClearChildren();
	if (!Sim.IsValid() || !Sim->IsReady()) return;
	const FString Q = Text.ToString().TrimStartAndEnd().ToLower();
	if (Q.Len() < 2) return;
	const warroom::Data& D = Sim->GetData();
	struct FHit { FString Name, Kind; int32 Id; FString Tag; int32 Vp; bool bStarts; };
	TArray<FHit> Hits;
	auto Consider = [&](const FString& Name, const FString& Kind, int32 Id, const FString& Tag, int32 Vp) {
		const FString L = Name.ToLower();
		if (L.StartsWith(Q)) Hits.Add({Name, Kind, Id, Tag, Vp, true}); else if (L.Contains(Q)) Hits.Add({Name, Kind, Id, Tag, Vp, false});
	};
	for (const auto& C : D.countries) Consider(F(C.name), TEXT("country"), 0, F(C.tag), 100);
	for (const auto& Sd : D.states) Consider(F(Sd.name), TEXT("state"), Sd.id, TEXT(""), 50);
	for (int32 i = 1; i <= D.provinceCount; i++) Consider(F(D.provinces[i].name), D.provinces[i].type == "land" ? TEXT("province") : F(D.provinces[i].type), i, TEXT(""), D.provinces[i].vp);
	Hits.Sort([](const FHit& A, const FHit& B) { if (A.bStarts != B.bStarts) return A.bStarts; return A.Vp > B.Vp; });
	for (int32 i = 0; i < FMath::Min(10, Hits.Num()); i++)
	{
		const FHit H = Hits[i];
		SearchResults->AddSlot().AutoHeight()
		[
			SNew(SButton).ButtonStyle(FCoreStyle::Get(), "NoBorder").HAlign(HAlign_Left).OnClicked_Lambda([this, H]() {
				SearchResults->ClearChildren(); SearchBox->SetText(FText::GetEmpty());
				if (H.Kind == TEXT("country")) OpenCountry(H.Tag); else if (H.Kind == TEXT("state")) OpenState(H.Id); else OpenProvince(H.Id);
				return FReply::Handled(); })
			[ SNew(STextBlock).Text(FText::FromString(H.Name + TEXT("  ") + H.Kind)) ]
		];
	}
}

void SWarRoomHUD::OnSearchCommitted(const FText& Text, ETextCommit::Type Commit)
{
	if (Commit != ETextCommit::OnEnter || !SearchResults.IsValid() || SearchResults->NumSlots() == 0) return;
	// press the first result
	if (TSharedPtr<SWidget> First = SearchResults->GetChildren()->GetChildAt(0))
	{
		TSharedPtr<SButton> B = StaticCastSharedPtr<SButton>(First);
		if (B.IsValid()) B->SimulateClick();
	}
}

#undef LOCTEXT_NAMESPACE
