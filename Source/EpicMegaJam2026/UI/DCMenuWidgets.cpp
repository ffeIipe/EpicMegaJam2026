#include "UI/DCMenuWidgets.h"
#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Core/DCGameInstance.h"
#include "Core/DCGameMode.h"
#include "Core/DCPlayerController.h"
#include "Core/DCMainMenuMode.h"
#include "Core/DCSettings.h"
#include "Data/PDA_GameConfig.h"
#include "Engine/Texture2D.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Input/CommonUIInputTypes.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/Spacer.h"
#include "Containers/Ticker.h"
#include "FileMediaSource.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Materials/MaterialInterface.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DirectorsCut"

namespace
{
	const FName AllGenreTab = "All";
	const FAnchors Center(0.5f, 0.5f);
	const FVector2D Middle(0.5f, 0.5f);

	// Stacked menu buttons, stretched to the panel width
	UDCButton* MenuButton(UWidgetTree* Tree, UVerticalBox* Parent, const FText& Label, TSubclassOf<UCommonButtonStyle> Style = UDCButton_Primary::StaticClass())
	{
		UDCButton* Btn = DCUI::Button(Tree, Parent, Label, NAME_None, Style);
		Cast<UVerticalBoxSlot>(Btn->Slot)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		return Btn;
	}
	UDCButton* MenuButton(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label, TSubclassOf<UCommonButtonStyle> Style = UDCButton_Primary::StaticClass())
	{
		return MenuButton(Tree, Parent, FText::FromString(Label), Style);
	}
}

// ---------------------------------------------------------------- Day summary

UDCDaySummaryWidget::UDCDaySummaryWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	bIsBackHandler = false; // only "Open the store" closes it
}

void UDCDaySummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	TitleText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Title::StaticClass());
	TitleText->SetMargin(FMargin(0.f, 0.f, 0.f, 12.f));
	BodyText = DCUI::Text(WidgetTree, V, TEXT(""));
	MenuButton(WidgetTree, V, LOCTEXT("Summary_OpenStore", "Open the store"))->OnPayloadClicked.AddDynamic(this, &UDCDaySummaryWidget::HandleOpen);
}

void UDCDaySummaryWidget::Show(const FDaySummary& S)
{
	auto Num = [](int32 V) { return FText::FromString(FString::FromInt(V)); };
	TitleText->SetText(FText::Format(LOCTEXT("Summary_DayComplete", "DAY {0} COMPLETE"), Num(S.DayIndex)));
	BodyText->SetText(FText::Format(
		LOCTEXT("Summary_Body", "Clients served: {0}\nClients lost: {1}\nLoved: {2}   Liked: {3}   Neutral: {4}   Hated: {5}\nMoney earned: ${6}\nRent paid: ${7}\nMoney: ${8} / goal ${9}"),
		Num(S.ClientsServed), Num(S.ClientsLost), Num(S.Loved), Num(S.Liked), Num(S.Neutral), Num(S.Hated),
		Num(S.MoneyEarned), Num(S.RentPaid), Num(S.MoneyNow), Num(S.Goal)));
}

// Card of the computer / supplier lists (defined with the computer): panel frame, colored tile, fill-width info column
static UVerticalBox* AddCard(UWidgetTree* Tree, UVerticalBox* List, const FLinearColor& TileColor, UHorizontalBox** OutRow = nullptr, FVector2D TileDims = FVector2D(72.f, 72.f), TSoftObjectPtr<UTexture2D> Cover = nullptr);

// Genre color from DT_Genres, a neutral blue when the genre has no row
static FLinearColor GenreColor(const UPDA_GameConfig* Config, EGenre Genre)
{
	if (Config && Config->GenreTable)
	{
		for (const TPair<FName, uint8*>& G : Config->GenreTable->GetRowMap())
		{
			if (reinterpret_cast<const FGenreData*>(G.Value)->Genre == Genre)
			{
				return reinterpret_cast<const FGenreData*>(G.Value)->Color;
			}
		}
	}
	return DCPalette::Blue;
}

/// ---------------------------------------------------------------- Supplier

void UDCSupplierWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	bPausesGame = true; // the cash register is a menu: the day stops while shopping
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Inventory = GM->Inventory;
	Unlocks = GM->Unlocks;
	Config = GM->Config;
	GM->Economy->OnMoneyChanged.AddDynamic(this, &UDCSupplierWidget::OnMoneyChanged);
	Unlocks->OnUnlockPurchased.AddDynamic(this, &UDCSupplierWidget::OnUnlockPurchased);
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("PrevTab"), false, FSimpleDelegate::CreateUObject(this, &UDCSupplierWidget::CycleTab, -1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("NextTab"), false, FSimpleDelegate::CreateUObject(this, &UDCSupplierWidget::CycleTab, 1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("Warehouse"), false, FSimpleDelegate::CreateLambda([this]() { if (ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer())) { PC->GoToBackRoomFromScreen(); } })));

	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, LOCTEXT("Supplier_Title", "SUPPLIER"), LOCTEXT("Supplier_Subtitle", "Tapes for sale, one copy at a time"));
	Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	StarFilter = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(StarFilter)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UCommonBorder* Panel = DCUI::Border(WidgetTree, V, UDCBorder_Inset::StaticClass(), FMargin(12.f));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(720.f);
	Size->SetHeightOverride(400.f);
	Panel->SetContent(Size);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Size->AddChild(Scroll);
	List = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(List);
}

void UDCSupplierWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	ADCAudioManager::PlayCueStatic(this, TEXT("OpenSupplier"));
	SelectedGenre = AllGenreTab;
	Refresh(); // what is on sale changes with the unlocks
}

void UDCSupplierWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	ADCAudioManager::PlayCueStatic(this, TEXT("CloseSupplier"));
}

void UDCSupplierWidget::CycleTab(int32 Dir)
{
	const int32 Num = Tabs ? Tabs->GetChildrenCount() : 0;
	for (int32 i = 0; i < Num; ++i)
	{
		if (Cast<UDCButton>(Tabs->GetChildAt(i))->Payload == SelectedGenre)
		{
			SelectedGenre = Cast<UDCButton>(Tabs->GetChildAt((i + Dir + Num) % Num))->Payload;
			ADCAudioManager::PlayCueStatic(this, TEXT("TabSwitch"));
			Refresh();
			return;
		}
	}
}

void UDCSupplierWidget::Refresh()
{
	if (!Inventory || !Unlocks || !Config || !Config->VHSTable || !Config->GenreTable)
	{
		return;
	}
	auto Select = [](UDCButton* Btn, bool bSelected) { bSelected ? Btn->SetIsSelected(true, false) : Btn->ClearSelection(); };

	// Genre tabs: ALL + the unlocked ones, in DT_Genres order
	Tabs->ClearChildren();
	TArray<TPair<FName, FText>> Genres;
	Genres.Emplace(AllGenreTab, LOCTEXT("Supplier_TabAll", "ALL"));
	for (const TPair<FName, uint8*>& P : Config->GenreTable->GetRowMap())
	{
		const FGenreData* Row = reinterpret_cast<const FGenreData*>(P.Value);
		if (Row->Genre != EGenre::Paranormal && Unlocks->IsGenreUnlocked(Row->Genre))
		{
			Genres.Emplace(FName(*StaticEnum<EGenre>()->GetNameStringByValue((int64)Row->Genre)), Row->DisplayName);
		}
	}
	if (!Genres.ContainsByPredicate([this](const TPair<FName, FText>& Entry) { return Entry.Key == SelectedGenre; }))
	{
		SelectedGenre = AllGenreTab;
	}
	for (const TPair<FName, FText>& Genre : Genres)
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, Tabs, Genre.Value.ToString().ToUpper(), Genre.Key, UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::OnTabClicked);
		Cast<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		Select(Tab, Genre.Key == SelectedGenre);
	}

	// Star filter: up to the licensed level
	StarFilter->ClearChildren();
	if (SelectedStars > Unlocks->UnlockedStarTier)
	{
		SelectedStars = 0;
	}
	for (int32 Stars = 0; Stars <= Unlocks->UnlockedStarTier; ++Stars)
	{
		const FText Label = Stars == 0 ? LOCTEXT("Supplier_AllStars", "ALL STARS")
			: (Stars == 1 ? LOCTEXT("Supplier_OneStar", "1 STAR") : FText::Format(LOCTEXT("Supplier_ManyStars", "{0} STARS"), FText::FromString(FString::FromInt(Stars))));
		UDCButton* Filter = DCUI::Button(WidgetTree, StarFilter, Label, FName(*FString::FromInt(Stars)), UDCButton_Tab::StaticClass());
		Filter->SetIsSelectable(true);
		Filter->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::OnStarClicked);
		Cast<UHorizontalBoxSlot>(Filter->Slot)->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		Select(Filter, Stars == SelectedStars);
	}

	// Rows: fewest stars first
	List->ClearChildren();
	BuyButtons.Reset();
	TArray<FName> Rows;
	for (const TPair<FName, uint8*>& P : Config->VHSTable->GetRowMap())
	{
		const FVHSData* Tape = reinterpret_cast<const FVHSData*>(P.Value);
		if (Inventory->IsTapeForSale(P.Key)
			&& (SelectedGenre == AllGenreTab || StaticEnum<EGenre>()->GetNameStringByValue((int64)Tape->PrimaryGenre) == SelectedGenre.ToString())
			&& (SelectedStars == 0 || Tape->Rating == SelectedStars))
		{
			Rows.Add(P.Key);
		}
	}
	Rows.Sort([this](const FName& A, const FName& B)
	{
		const int32 StarsA = Config->VHSTable->FindRow<FVHSData>(A, TEXT("Sort"))->Rating;
		const int32 StarsB = Config->VHSTable->FindRow<FVHSData>(B, TEXT("Sort"))->Rating;
		return StarsA != StarsB ? StarsA < StarsB : A.LexicalLess(B);
	});
	if (Rows.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, LOCTEXT("Supplier_Empty", "Nothing on sale here yet."));
	}
	for (const FName& Row : Rows)
	{
		const FVHSData* Tape = Config->VHSTable->FindRow<FVHSData>(Row, TEXT("Row"));
		// Same card as the computer: cover art (falling back to genre color if unset), title with stars and skills, price over the buy buttons
		UHorizontalBox* H = nullptr;
		UVerticalBox* Info = AddCard(WidgetTree, List, GenreColor(Config, Tape->PrimaryGenre), &H, FVector2D(54.f, 96.f), Tape->Cover);
		DCUI::Text(WidgetTree, Info, DCUI::CleanTitle(Tape->Title).ToUpper(), UDCText_Heading::StaticClass())->SetAutoWrapText(true);
		DCUI::Text(WidgetTree, Info, DCUI::Stars(Tape->Rating), UDCText_Stars::StaticClass());
		DCUI::InlineSkills(WidgetTree, Info, Tape->Skills);
		H->SetVisibility(ESlateVisibility::Visible); // hit-testable so the tooltip shows
		DCUI::SkillTooltip(WidgetTree, H, Tape->Skills);
		UVerticalBox* Buy = WidgetTree->ConstructWidget<UVerticalBox>();
		H->AddChildToHorizontalBox(Buy)->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Price = DCUI::Text(WidgetTree, Buy, FString::Printf(TEXT("$%d"), Inventory->GetTapePrice(Row)), UDCText_Caution::StaticClass());
		Cast<UVerticalBoxSlot>(Price->Slot)->SetHorizontalAlignment(HAlign_Center);
		UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
		Buy->AddChildToVerticalBox(Buttons);
		for (const int32 Count : { 1, 5 })
		{
			UDCButton* Btn = DCUI::Button(WidgetTree, Buttons, FString::Printf(TEXT("x%d"), Count), Row);
			if (Count == 1)
			{
				Btn->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::HandleBuyOne);
			}
			else
			{
				Btn->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::HandleBuyFive);
			}
			Cast<UHorizontalBoxSlot>(Btn->Slot)->SetPadding(FMargin(Count == 1 ? 0.f : 4.f, 0.f, 0.f, 0.f));
			BuyButtons.Add({ Btn, Row, Count });
		}
	}
	UpdateAffordability();
}

void UDCSupplierWidget::UpdateAffordability()
{
	for (const FBuyButton& B : BuyButtons)
	{
		if (B.Button.IsValid())
		{
			B.Button->SetIsEnabled(Inventory->CanBuyTape(B.Row, B.Count));
		}
	}
}

void UDCSupplierWidget::HandleBuyOne(FName Row)
{
	Inventory->BuyTape(Row, 1);
}

void UDCSupplierWidget::HandleBuyFive(FName Row)
{
	Inventory->BuyTape(Row, 5);
}

// ---------------------------------------------------------------- Computer

void UDCComputerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	bPausesGame = true; // the day stops while upgrading
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Unlocks = GM->Unlocks;
	Config = GM->Config;
	GM->Economy->OnMoneyChanged.AddDynamic(this, &UDCComputerWidget::OnMoneyChanged);
	GM->ClientDirector->OnReviewReceived.AddDynamic(this, &UDCComputerWidget::OnReviewReceived);
	Unlocks->OnUnlockPurchased.AddDynamic(this, &UDCComputerWidget::OnUnlockPurchased);
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("PrevTab"), false, FSimpleDelegate::CreateUObject(this, &UDCComputerWidget::CycleTab, -1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("NextTab"), false, FSimpleDelegate::CreateUObject(this, &UDCComputerWidget::CycleTab, 1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("Warehouse"), false, FSimpleDelegate::CreateLambda([this]() { if (ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer())) { PC->GoToBackRoomFromScreen(); } })));

	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, LOCTEXT("Computer_Title", "COMPUTER"), LOCTEXT("Computer_Subtitle", "Upgrade the store, check the rent and read your reviews"));
	MoneyText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Heading::StaticClass());
	MoneyText->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	const TPair<const TCHAR*, FText> TabDefs[] = {
		{ TEXT("Genres"), LOCTEXT("Computer_TabGenres", "GENRES") },
		{ TEXT("Machine"), LOCTEXT("Computer_TabMachine", "COMBINE MACHINE") },
		{ TEXT("Stars"), LOCTEXT("Computer_TabStars", "STAR LICENSES") },
		{ TEXT("Rent"), LOCTEXT("Computer_TabRent", "RENT") },
		{ TEXT("Reviews"), LOCTEXT("Computer_TabReviews", "REVIEWS") }
	};
	for (const TPair<const TCHAR*, FText>& Def : TabDefs)
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, Tabs, Def.Value, FName(Def.Key), UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCComputerWidget::OnTabClicked);
		Cast<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
	}
	SelectedTab = "Genres";
	UCommonBorder* Panel = DCUI::Border(WidgetTree, V, UDCBorder_Inset::StaticClass(), FMargin(12.f));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(720.f);
	Size->SetHeightOverride(460.f);
	Panel->SetContent(Size);
	Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	Size->AddChild(Scroll);
	List = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(List);
}

void UDCComputerWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	ADCAudioManager::PlayCueStatic(this, TEXT("TurnOnComputer"));
	Refresh();
}

void UDCComputerWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	ADCAudioManager::PlayCueStatic(this, TEXT("TurnOffComputer"));
}

void UDCComputerWidget::Refresh()
{
	if (!Unlocks || !Config || !Config->UnlockTable)
	{
		return;
	}
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	MoneyText->SetText(FText::Format(LOCTEXT("Computer_Money", "Money: ${0}"), FText::FromString(FString::FromInt(GM->Economy->Money))));
	for (int32 i = 0; i < Tabs->GetChildrenCount(); ++i)
	{
		UDCButton* Tab = Cast<UDCButton>(Tabs->GetChildAt(i));
		Tab->Payload == SelectedTab ? Tab->SetIsSelected(true, false) : Tab->ClearSelection();
	}
	const float Offset = Scroll->GetScrollOffset(); // a purchase rebuilds the list: keep the place
	List->ClearChildren();
	Buttons.Reset();
	if (SelectedTab == "Rent")
	{
		BuildRent();
	}
	else if (SelectedTab == "Reviews")
	{
		BuildReviews();
	}
	else
	{
		BuildUnlocks(SelectedTab == "Genres" ? EUnlockType::Genre : SelectedTab == "Machine" ? EUnlockType::Slot : EUnlockType::StarTier);
	}
	UpdateAffordability();
	Scroll->SetScrollOffset(Offset);
}

void UDCComputerWidget::OnTabClicked(FName Tab)
{
	SelectedTab = Tab;
	Refresh();
	Scroll->SetScrollOffset(0.f); // a new tab starts at the top
}

void UDCComputerWidget::CycleTab(int32 Dir)
{
	const int32 Num = Tabs->GetChildrenCount();
	for (int32 i = 0; i < Num; ++i)
	{
		if (Cast<UDCButton>(Tabs->GetChildAt(i))->Payload == SelectedTab)
		{
			ADCAudioManager::PlayCueStatic(this, TEXT("TabSwitch"));
			OnTabClicked(Cast<UDCButton>(Tabs->GetChildAt((i + Dir + Num) % Num))->Payload);
			return;
		}
	}
}

void UDCComputerWidget::OnMoneyChanged(int32 NewMoney, int32 Delta)
{
	if (SelectedTab == "Rent")
	{
		Refresh(); // the covered / short verdict depends on the money
	}
	else
	{
		MoneyText->SetText(FText::Format(LOCTEXT("Computer_Money", "Money: ${0}"), FText::FromString(FString::FromInt(NewMoney))));
		UpdateAffordability();
	}
}

// A card of the computer lists: panel frame, colored tile (or cover art), and a fill-width info column. Returns the info box; OutRow is the row to append things on the right.
static UVerticalBox* AddCard(UWidgetTree* Tree, UVerticalBox* List, const FLinearColor& TileColor, UHorizontalBox** OutRow, FVector2D TileDims, TSoftObjectPtr<UTexture2D> Cover)
{
	UCommonBorder* Card = DCUI::Border(Tree, List, UDCBorder_Panel::StaticClass(), FMargin(14.f, 12.f));
	Cast<UVerticalBoxSlot>(Card->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	UHorizontalBox* H = Tree->ConstructWidget<UHorizontalBox>();
	Card->SetContent(H);
	USizeBox* TileSize = Tree->ConstructWidget<USizeBox>();
	TileSize->SetWidthOverride(TileDims.X);
	TileSize->SetHeightOverride(TileDims.Y);
	UImage* Tile = Tree->ConstructWidget<UImage>();
	if (!DCUI::SetCoverTexture(Tile, Cover))
	{
		Tile->SetBrush(FSlateBrush());
		Tile->SetColorAndOpacity(TileColor);
	}
	TileSize->AddChild(Tile);
	UHorizontalBoxSlot* TileSlot = H->AddChildToHorizontalBox(TileSize);
	TileSlot->SetVerticalAlignment(VAlign_Center);
	TileSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
	UVerticalBox* Info = Tree->ConstructWidget<UVerticalBox>();
	UHorizontalBoxSlot* InfoSlot = H->AddChildToHorizontalBox(Info);
	InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	InfoSlot->SetVerticalAlignment(VAlign_Center);
	InfoSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
	if (OutRow)
	{
		*OutRow = H;
	}
	return Info;
}

void UDCComputerWidget::BuildRent()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	const int32 Day = GM->DayNightCycle->DayIndex;
	const int32 Rent = GM->Economy->GetRentForDay(Day);
	const int32 Money = GM->Economy->Money;
	const int32 Goal = GM->Config->MovieBudgetGoal;

	// Store status: day, phase, clock and movie budget goal
	UVerticalBox* Status = AddCard(WidgetTree, List, DCPalette::Blue);
	const FText PhaseLabel = GM->DayNightCycle->CurrentPhase == EPhase::Day
		? LOCTEXT("Phase_Day", "Day")
		: LOCTEXT("Phase_Night", "Night");
	const FText ClockLabel = GM->DayNightCycle->bTutorialPaused
		? FText::Format(LOCTEXT("Stats_TutorialClock", "{0} (Tutorial)"), GM->DayNightCycle->GetClockText())
		: GM->DayNightCycle->GetClockText();
	DCUI::Text(WidgetTree, Status, FText::Format(LOCTEXT("Rent_DayClock", "DAY {0} - {1}   {2}"),
		FText::FromString(FString::FromInt(Day)), PhaseLabel, ClockLabel), UDCText_Heading::StaticClass());
	UHorizontalBox* GoalRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Status->AddChildToVerticalBox(GoalRow)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	UProgressBar* GoalBar = DCUI::Bar(WidgetTree, GoalRow, 220.f);
	Cast<UHorizontalBoxSlot>(GoalBar->GetParent()->Slot)->SetVerticalAlignment(VAlign_Center);
	GoalBar->SetPercent(Goal > 0 ? FMath::Clamp((float)Money / Goal, 0.f, 1.f) : 0.f);
	UCommonTextBlock* GoalText = DCUI::Text(WidgetTree, GoalRow,
		FText::Format(LOCTEXT("Rent_GoalProgress", "Goal: ${0} / ${1}"),
			FText::FromString(FString::FromInt(Money)),
			FText::FromString(FString::FromInt(Goal))),
		UDCText_Accent::StaticClass());
	Cast<UHorizontalBoxSlot>(GoalText->Slot)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	Cast<UHorizontalBoxSlot>(GoalText->Slot)->SetVerticalAlignment(VAlign_Center);

	// Tonight's rent
	const int32 DueMinutes = FMath::RoundToInt(GM->Config->ClockNightEndHour * 60.f) % (24 * 60);
	const FString DueTimeStr = FString::Printf(TEXT("%02d:%02d"), DueMinutes / 60, DueMinutes % 60);
	UVerticalBox* Now = AddCard(WidgetTree, List, Money >= Rent ? DCPalette::Lime : DCPalette::Red);
	DCUI::Text(WidgetTree, Now, FText::Format(LOCTEXT("Rent_ForDayDue", "RENT FOR DAY {0}: ${1} (DUE {2})"),
		FText::FromString(FString::FromInt(Day)),
		FText::FromString(FString::FromInt(Rent)),
		FText::FromString(DueTimeStr)), UDCText_Heading::StaticClass());
	DCUI::Text(WidgetTree, Now, LOCTEXT("Rent_Explanation", "It is collected automatically when the night ends. If you can't cover it, you are evicted."))->SetWrapTextAt(520.f);
	DCUI::Text(WidgetTree, Now, Money >= Rent
		? FText::Format(LOCTEXT("Rent_Covered", "Covered: you have ${0}, ${1} to spare."), FText::FromString(FString::FromInt(Money)), FText::FromString(FString::FromInt(Money - Rent)))
		: FText::Format(LOCTEXT("Rent_Short", "Short by ${0}: you have ${1}."), FText::FromString(FString::FromInt(Rent - Money)), FText::FromString(FString::FromInt(Money))),
		Money >= Rent ? UDCText_Accent::StaticClass() : UDCText_Warn::StaticClass());
	DCUI::Text(WidgetTree, List, LOCTEXT("Rent_ComingUp", "COMING UP"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 6.f, 0.f, 8.f));
	for (int32 Next = Day + 1; Next <= Day + 3; ++Next)
	{
		UHorizontalBox* Row = nullptr;
		UVerticalBox* Info = AddCard(WidgetTree, List, DCPalette::Blue, &Row);
		DCUI::Text(WidgetTree, Info, FText::Format(LOCTEXT("Rent_DayN", "DAY {0}"), FText::FromString(FString::FromInt(Next))), UDCText_Heading::StaticClass());
		Row->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("$%d"), GM->Economy->GetRentForDay(Next)), UDCText_Caution::StaticClass()))->SetVerticalAlignment(VAlign_Center);
	}
}

void UDCComputerWidget::BuildReviews()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	const TArray<FReview>& History = GM->ClientDirector->ReviewHistory;
	if (History.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, LOCTEXT("Reviews_Empty", "No reviews yet. Deliver a film and ReelTalk.net will have an opinion."));
		return;
	}
	for (const FReview& R : History)
	{
		const int32* Stars = Config->ReviewStars.Find(R.Reaction);
		const FLinearColor Tile = R.Reaction == EReaction::Loved ? DCPalette::Lime : R.Reaction == EReaction::Liked ? DCPalette::Green
			: R.Reaction == EReaction::Neutral ? DCPalette::Yellow : DCPalette::Red;
		UHorizontalBox* H = nullptr;
		UVerticalBox* Info = AddCard(WidgetTree, List, Tile, &H);
		DCUI::Text(WidgetTree, Info, R.FilmTitle, UDCText_Heading::StaticClass());
		DCUI::Text(WidgetTree, Info, R.Line)->SetWrapTextAt(440.f);
		if (!R.FeedbackLine.IsEmpty())
		{
			DCUI::Text(WidgetTree, Info, R.FeedbackLine, UDCText_Accent::StaticClass())->SetWrapTextAt(440.f);
		}
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, DCUI::Stars(Stars ? *Stars : 0), UDCText_Stars::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Delta = DCUI::Text(WidgetTree, nullptr, R.MoneyDelta == 0 ? FString() : FString::Printf(TEXT("%+d"), R.MoneyDelta), UDCText_Heading::StaticClass());
		Delta->SetColorAndOpacity(FSlateColor(R.MoneyDelta < 0 ? DCPalette::Red : DCPalette::Lime));
		UHorizontalBoxSlot* DeltaSlot = H->AddChildToHorizontalBox(Delta);
		DeltaSlot->SetVerticalAlignment(VAlign_Center);
		DeltaSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	}
}

void UDCComputerWidget::BuildUnlocks(EUnlockType Type)
{
	TArray<FName> Ids;
	for (const TPair<FName, uint8*>& P : Config->UnlockTable->GetRowMap())
	{
		if (reinterpret_cast<const FUnlockData*>(P.Value)->Type == Type)
		{
			Ids.Add(P.Key);
		}
	}
	Ids.Sort([this](const FName& A, const FName& B)
	{
		const int32 PriceA = Config->UnlockTable->FindRow<FUnlockData>(A, TEXT("Sort"))->Price;
		const int32 PriceB = Config->UnlockTable->FindRow<FUnlockData>(B, TEXT("Sort"))->Price;
		return PriceA != PriceB ? PriceA < PriceB : A.LexicalLess(B);
	});
	for (const FName& Id : Ids)
	{
		const FUnlockData* Row = Config->UnlockTable->FindRow<FUnlockData>(Id, TEXT("Row"));
		const TPair<FName, const FUnlockData*> P(Id, Row);
		const bool bOwned = Unlocks->IsOwned(P.Key);
		// One card per item: colored tile (genre color, yellow for stars, blue for slots), type tag, name, description, price and button
		UCommonBorder* Card = DCUI::Border(WidgetTree, List, UDCBorder_Panel::StaticClass(), FMargin(14.f, 12.f));
		Cast<UVerticalBoxSlot>(Card->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		Card->SetRenderOpacity(bOwned ? 0.6f : 1.f);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Card->SetContent(H);
		FLinearColor TileColor = Type == EUnlockType::StarTier ? DCPalette::Yellow : DCPalette::Blue;
		if (Type == EUnlockType::Genre && Config->GenreTable)
		{
			for (const TPair<FName, uint8*>& G : Config->GenreTable->GetRowMap())
			{
				if (reinterpret_cast<const FGenreData*>(G.Value)->Genre == Row->Genre)
				{
					TileColor = reinterpret_cast<const FGenreData*>(G.Value)->Color;
				}
			}
		}
		USizeBox* TileSize = WidgetTree->ConstructWidget<USizeBox>();
		TileSize->SetWidthOverride(72.f);
		TileSize->SetHeightOverride(72.f);
		UImage* Tile = WidgetTree->ConstructWidget<UImage>();
		Tile->SetBrush(FSlateBrush());
		Tile->SetColorAndOpacity(TileColor);
		TileSize->AddChild(Tile);
		UHorizontalBoxSlot* TileSlot = H->AddChildToHorizontalBox(TileSize);
		TileSlot->SetVerticalAlignment(VAlign_Center);
		TileSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* InfoSlot = H->AddChildToHorizontalBox(Info);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetVerticalAlignment(VAlign_Center);
		InfoSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		DCUI::Text(WidgetTree, Info, Row->DisplayName.ToString().ToUpper(), UDCText_Heading::StaticClass());
		DCUI::Text(WidgetTree, Info, Row->Description)->SetWrapTextAt(440.f);
		TArray<FString> Needs;
		for (const FName& Required : Row->RequiresUnlocks)
		{
			const FUnlockData* Other = Config->UnlockTable->FindRow<FUnlockData>(Required, TEXT("Requires"), false);
			Needs.Add(Other ? Other->DisplayName.ToString() : Required.ToString());
		}
		if (Row->MinGenresOwned > 0)
		{
			Needs.Add(FText::Format(LOCTEXT("Unlock_MinGenres", "{0} genres unlocked (you have {1})"),
				FText::FromString(FString::FromInt(Row->MinGenresOwned)),
				FText::FromString(FString::FromInt(Unlocks->GetGenreCount()))).ToString());
		}
		if (!bOwned && Needs.Num() > 0)
		{
			DCUI::Text(WidgetTree, Info, FText::Format(LOCTEXT("Unlock_Requires", "Requires: {0}"), FText::FromString(FString::Join(Needs, TEXT(", ")))), UDCText_Warn::StaticClass())->SetWrapTextAt(440.f);
		}
		UVerticalBox* Buy = WidgetTree->ConstructWidget<UVerticalBox>();
		H->AddChildToHorizontalBox(Buy)->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Price = DCUI::Text(WidgetTree, Buy, bOwned ? FString() : FString::Printf(TEXT("$%d"), Row->Price), UDCText_Caution::StaticClass());
		Price->SetJustification(ETextJustify::Center);
		Cast<UVerticalBoxSlot>(Price->Slot)->SetHorizontalAlignment(HAlign_Center);
		UDCButton* Btn = DCUI::Button(WidgetTree, Buy, bOwned ? LOCTEXT("Unlock_Owned", "OWNED") : LOCTEXT("Unlock_Buy", "BUY"), P.Key);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCComputerWidget::HandleBuy);
		Buttons.Add({ Btn, P.Key });
	}
	if (Ids.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, LOCTEXT("Unlock_Empty", "Nothing to buy here."));
	}
}

void UDCComputerWidget::UpdateAffordability()
{
	for (const FUnlockButton& B : Buttons)
	{
		if (B.Button.IsValid())
		{
			B.Button->SetIsEnabled(Unlocks->CanBuyUnlock(B.Id)); // false when owned, short of money or a requirement is missing
		}
	}
}

void UDCComputerWidget::HandleBuy(FName UnlockId)
{
	Unlocks->BuyUnlock(UnlockId);
}

// ---------------------------------------------------------------- Shelf popup

void UDCShelfWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, LOCTEXT("Shelf_Title", "VHS INVENTORY"));
	List = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	V->AddChild(List);
	List->Setup(GM->Inventory, GM->Config, false, nullptr); // read-only stock (section 9): clicking a row only shows its detail
	GM->Inventory->OnInventoryChanged.AddDynamic(List, &UDCInventoryWidget::Refresh);
}

void UDCShelfWidget::ShowGenre(EGenre Genre)
{
	List->SetGenre(Genre);
}

// ---------------------------------------------------------------- Tutorial sign

UDCTutorialWidget::UDCTutorialWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bAutoActivate = false;
	bIsBackHandler = true;
	bIsBackActionDisplayedInActionBar = false;
}

TOptional<FUIInputConfig> UDCTutorialWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

bool UDCTutorialWidget::NativeOnHandleBackAction()
{
	if (ADCPlayerController* PC = GetOwningPlayer<ADCPlayerController>())
	{
		PC->OpenPauseFromTutorial();
	}
	return true;
}

FReply UDCTutorialWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::F3 && SkipButton && SkipButton->GetVisibility() == ESlateVisibility::Visible)
	{
		HandleSkip(NAME_None);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDCTutorialWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	// Fullscreen semi-transparent black backdrop that blocks interaction with everything behind the tutorial
	Backdrop = DCUI::Border(WidgetTree, Canvas, UDCBorder_TutorialBackdrop::StaticClass(), FMargin(0.f));
	Backdrop->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* BackdropSlot = Cast<UCanvasPanelSlot>(Backdrop->Slot))
	{
		BackdropSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BackdropSlot->SetOffsets(FMargin(0.f));
	}

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	Stack->SetVisibility(ESlateVisibility::Collapsed);
	Panel = Stack;
	PanelSlot = Canvas->AddChildToCanvas(Stack);
	PanelSlot->SetAutoSize(true);
	SetPlacement(ETutorialPlacement::BottomCenter);

	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	TopRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Stack->AddChildToVerticalBox(TopRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	UCommonBorder* HeaderPlate = DCUI::Border(WidgetTree, TopRow, UDCBorder_Header::StaticClass(), FMargin(48.f, 8.f, 36.f, 8.f));
	HeaderPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	StepTitleText = DCUI::Text(WidgetTree, nullptr, LOCTEXT("Tutorial_DefaultHeader", "TUTORIAL"), UDCText_Heading::StaticClass());
	HeaderPlate->SetContent(StepTitleText);

	USpacer* FillGap = WidgetTree->ConstructWidget<USpacer>();
	FillGap->SetVisibility(ESlateVisibility::HitTestInvisible);
	TopRow->AddChildToHorizontalBox(FillGap)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	SkipButton = DCUI::Button(WidgetTree, TopRow, LOCTEXT("Tutorial_SkipBtn", "Skip (F3)"), NAME_None, UDCButton_Tab::StaticClass());
	SkipButton->SetIsFocusable(false);
	SkipButton->OnPayloadClicked.AddDynamic(this, &UDCTutorialWidget::HandleSkip);
	if (UHorizontalBoxSlot* SkipSlot = Cast<UHorizontalBoxSlot>(SkipButton->Slot))
	{
		SkipSlot->SetVerticalAlignment(VAlign_Bottom);
		SkipSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	}

	UCommonBorder* BodyBorder = DCUI::Border(WidgetTree, Stack, UDCBorder_Panel::StaticClass(), FMargin(18.f, 12.f));
	BodyBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* BodyV = WidgetTree->ConstructWidget<UVerticalBox>();
	BodyV->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BodyBorder->SetContent(BodyV);

	HintText = DCUI::Text(WidgetTree, BodyV, TEXT(""), UDCText_Body::StaticClass());
	HintText->SetWrapTextAt(780.f);
	ActionText = DCUI::Text(WidgetTree, BodyV, TEXT(""), UDCText_Accent::StaticClass());
	ActionText->SetWrapTextAt(780.f);
	ActionText->SetMargin(FMargin(0.f, 4.f, 0.f, 0.f));

	UHorizontalBox* AcceptRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	AcceptRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* AcceptRowSlot = BodyV->AddChildToVerticalBox(AcceptRow))
	{
		AcceptRowSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		AcceptRowSlot->SetHorizontalAlignment(HAlign_Right);
	}
	AcceptButton = DCUI::Button(WidgetTree, AcceptRow, LOCTEXT("Tutorial_AcceptBtn", "Accept"), NAME_None, UDCButton_Primary::StaticClass());
	AcceptButton->SetIsFocusable(false);
	AcceptButton->OnPayloadClicked.AddDynamic(this, &UDCTutorialWidget::HandleAccept);
}

void UDCTutorialWidget::SetPlacement(ETutorialPlacement Placement)
{
	if (!PanelSlot)
	{
		return;
	}
	const float WrapWidth = 780.f;
	if (HintText)
	{
		HintText->SetWrapTextAt(WrapWidth);
	}
	if (ActionText)
	{
		ActionText->SetWrapTextAt(WrapWidth);
	}
	// Always stay bottom-center
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
	PanelSlot->SetPosition(FVector2D(0.f, -18.f));
}

bool UDCTutorialWidget::IsModalOpen() const
{
	return Panel && Panel->GetVisibility() != ESlateVisibility::Collapsed;
}

void UDCTutorialWidget::ShowHint(const FText& Text)
{
	ShowStep(LOCTEXT("Tutorial_DefaultHeader", "TUTORIAL"), Text, FText::GetEmpty(), true);
}

void UDCTutorialWidget::ShowStep(const FText& StepHeader, const FText& Body, const FText& Action, bool bShowSkip)
{
	SetPlacement(ETutorialPlacement::BottomCenter);
	if (StepTitleText)
	{
		StepTitleText->SetText(StepHeader);
	}
	// Only the green line is shown: the explanatory body is dropped, and a step with no action line shows its body in green
	if (HintText)
	{
		HintText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ActionText)
	{
		const FText& Line = Action.IsEmpty() ? Body : Action;
		ActionText->SetText(Line);
		ActionText->SetMargin(FMargin(0.f));
		ActionText->SetVisibility(Line.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (SkipButton)
	{
		SkipButton->SetVisibility(bShowSkip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Backdrop)
	{
		Backdrop->SetVisibility(ESlateVisibility::Visible);
	}
	if (Panel)
	{
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	if (!IsActivated())
	{
		ActivateWidget();
	}
}

void UDCTutorialWidget::HideHint()
{
	if (Backdrop)
	{
		Backdrop->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Panel)
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (IsActivated())
	{
		DeactivateWidget();
	}
}

void UDCTutorialWidget::HandleSkip(FName Unused)
{
	HideHint();
	OnSkipped.Broadcast();
}

void UDCTutorialWidget::HandleAccept(FName Unused)
{
	HideHint();
	OnAccepted.Broadcast();
}

// ---------------------------------------------------------------- Main menu / pause

UDCMainMenuWidget::UDCMainMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = false; // keep MediaPlayer ticking smoothly in MAP_MainMenu
	bIsBackHandler = true;
}

void UDCMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	// Fullscreen looping video layer behind the left-side menu panel
	UScaleBox* VideoScale = WidgetTree->ConstructWidget<UScaleBox>();
	VideoScale->SetStretch(EStretch::ScaleToFill);
	UCanvasPanelSlot* VideoSlot = Root->AddChildToCanvas(VideoScale);
	VideoSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	VideoSlot->SetOffsets(FMargin(0.f));

	VideoImage = WidgetTree->ConstructWidget<UImage>();
	VideoScale->AddChild(VideoImage);

	// Left-aligned retro panel (header plate + bordered content), leaving the bottom-left action bar clear
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Stack);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.f, 0.5f));
	PanelSlot->SetPosition(FVector2D(64.f, -30.f));
	PanelSlot->SetAutoSize(true);

	UCommonBorder* HeaderPlate = DCUI::Border(WidgetTree, nullptr, UDCBorder_Header::StaticClass(), DCUI::HeaderPadding);
	HeaderPlate->SetContent(DCUI::Text(WidgetTree, nullptr, INVTEXT("RENT TONIGHT"), UDCText_Title::StaticClass()));
	UVerticalBoxSlot* PlateSlot = Stack->AddChildToVerticalBox(HeaderPlate);
	PlateSlot->SetHorizontalAlignment(HAlign_Left);
	PlateSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	PanelBorder = DCUI::Border(WidgetTree, Stack, UDCBorder_Transparent::StaticClass(), FMargin(20.f, 16.f));
	Cast<UVerticalBoxSlot>(PanelBorder->Slot)->SetHorizontalAlignment(HAlign_Left);
	USizeBox* MinWidth = WidgetTree->ConstructWidget<USizeBox>();
	MinWidth->SetMinDesiredWidth(280.f);
	PanelBorder->SetContent(MinWidth);
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	MinWidth->AddChild(V);

	// --- Main buttons (left side)
	MainButtons = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(MainButtons);
	PlayButton = MenuButton(WidgetTree, MainButtons, LOCTEXT("Menu_Play", "PLAY"), UDCButton_Menu::StaticClass());
	PlayButton->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandlePlay);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Menu_NewGame", "NEW GAME"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleNewGame);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Menu_Credits", "CREDITS"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleCredits);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Menu_Options", "OPTIONS"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleOptions);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Menu_Exit", "EXIT"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleQuit);

	// --- Save slots sub-page (Continue / New Game)
	SlotsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(SlotsPage);
	SlotsTitle = DCUI::Text(WidgetTree, SlotsPage, TEXT(""), UDCText_Heading::StaticClass());
	SlotsTitle->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	SlotsList = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPage->AddChild(SlotsList);

	OverwriteConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPage->AddChildToVerticalBox(OverwriteConfirmBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	OverwriteConfirmText = DCUI::Text(WidgetTree, OverwriteConfirmBox, LOCTEXT("Menu_OverwritePrompt", "OVERWRITE THIS SAVE SLOT?"), UDCText_Caution::StaticClass());
	MenuButton(WidgetTree, OverwriteConfirmBox, LOCTEXT("Menu_ConfirmOverwrite", "CONFIRM OVERWRITE"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleConfirmOverwrite);
	MenuButton(WidgetTree, OverwriteConfirmBox, LOCTEXT("Menu_Cancel", "CANCEL"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleCancelOverwrite);
	OverwriteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);

	DeleteConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPage->AddChildToVerticalBox(DeleteConfirmBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	DeleteConfirmText = DCUI::Text(WidgetTree, DeleteConfirmBox, LOCTEXT("Menu_DeletePrompt", "DELETE THIS SAVE SLOT?"), UDCText_Caution::StaticClass());
	MenuButton(WidgetTree, DeleteConfirmBox, LOCTEXT("Menu_ConfirmDelete", "CONFIRM DELETE"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleConfirmDelete);
	MenuButton(WidgetTree, DeleteConfirmBox, LOCTEXT("Menu_Cancel", "CANCEL"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleCancelDelete);
	DeleteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);

	MenuButton(WidgetTree, SlotsPage, LOCTEXT("Menu_Back", "BACK"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleBackToMain);

	// --- Credits sub-page
	CreditsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(CreditsPage);
	UCommonTextBlock* CreditsHeader = DCUI::Text(WidgetTree, CreditsPage, LOCTEXT("Menu_Credits", "CREDITS"), UDCText_Heading::StaticClass());
	CreditsHeader->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));

	auto AddCredit = [this](const TCHAR* Name, const FText& Role)
	{
		UCommonTextBlock* NameText = DCUI::Text(WidgetTree, CreditsPage, Name, UDCText_Heading::StaticClass());
		NameText->SetMargin(FMargin(0.f, 8.f, 0.f, 0.f));
		UCommonTextBlock* RoleText = DCUI::Text(WidgetTree, CreditsPage, Role, UDCText_Body::StaticClass());
		RoleText->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
	};
	AddCredit(TEXT("Couture Augusto"), LOCTEXT("Credits_Role1", "3D Props Artist & Technical Art"));
	AddCredit(TEXT("Londonio Gabriel"), LOCTEXT("Credits_Role2", "Art Direction, 3D Environment Art & Cinematics"));
	AddCredit(TEXT("Felipe Nuñez"), LOCTEXT("Credits_Role3", "Gameplay Systems Designer & Programmer"));
	AddCredit(TEXT("Patricio Mackinlay"), LOCTEXT("Credits_Role4", "Technical Sound Designer & Composer"));

	UCommonTextBlock* StudioFooter = DCUI::Text(WidgetTree, CreditsPage, INVTEXT("Ampersand Games"), UDCText_Accent::StaticClass());
	StudioFooter->SetMargin(FMargin(0.f, 12.f, 0.f, 4.f));
	MenuButton(WidgetTree, CreditsPage, LOCTEXT("Menu_Back", "BACK"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleBackToMain);

	// --- Options sub-page (reuses UDCSettingsWidget)
	OptionsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(OptionsPage);
	Settings = WidgetTree->ConstructWidget<UDCSettingsWidget>();
	OptionsPage->AddChild(Settings);
	MenuButton(WidgetTree, OptionsPage, LOCTEXT("Menu_Back", "BACK"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleBackToMain);

	// Studio badge outside the menu in the bottom-right corner (mirrors bottom-left action bar margin)
	UCommonBorder* StudioBadge = DCUI::Border(WidgetTree, nullptr, UDCBorder_Inset::StaticClass(), FMargin(14.f, 8.f));
	StudioBadge->SetContent(DCUI::Text(WidgetTree, nullptr, INVTEXT("AMPERSAND GAMES"), UDCText_Heading::StaticClass()));
	UCanvasPanelSlot* StudioSlot = Root->AddChildToCanvas(StudioBadge);
	StudioSlot->SetAnchors(FAnchors(1.f, 1.f));
	StudioSlot->SetAlignment(FVector2D(1.f, 1.f));
	StudioSlot->SetPosition(FVector2D(-48.f, -36.f));
	StudioSlot->SetAutoSize(true);

	SetupBackgroundVideo();
	RefreshMainButtons();
	ShowPage(EMenuPage::Main);
	LanguageChangedHandle = UDCSettings::OnLanguageChanged.AddUObject(this, &UDCMainMenuWidget::RefreshDynamicTexts);
}

void UDCMainMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshMainButtons();
	ShowPage(EMenuPage::Main);
}

void UDCMainMenuWidget::NativeDestruct()
{
	UDCSettings::OnLanguageChanged.Remove(LanguageChangedHandle);
	if (Settings)
	{
		Settings->CancelCapture();
	}
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}
	Super::NativeDestruct();
}

bool UDCMainMenuWidget::NativeOnHandleBackAction()
{
	if (CurrentPage != EMenuPage::Main)
	{
		Cue(TEXT("Click"));
		ShowPage(EMenuPage::Main);
		return true;
	}
	return true; // keep the main menu open when pressing Esc/Back on the root page
}

void UDCMainMenuWidget::SetupBackgroundVideo()
{
	if (!VideoImage)
	{
		return;
	}

	if (!VideoMaterial)
	{
		VideoMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DirectorsCut/UI/Media/M_UI_MainMenuVideo.M_UI_MainMenuVideo"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!MediaPlayer)
	{
		MediaPlayer = LoadObject<UMediaPlayer>(nullptr, TEXT("/Game/DirectorsCut/UI/Media/MP_MainMenu.MP_MainMenu"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!MediaPlayer)
	{
		MediaPlayer = NewObject<UMediaPlayer>(this);
	}
	if (!MediaSource)
	{
		MediaSource = LoadObject<UMediaSource>(nullptr, TEXT("/Game/DirectorsCut/UI/Media/FMS_MainMenu.FMS_MainMenu"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!MediaSource)
	{
		UFileMediaSource* FileSource = NewObject<UFileMediaSource>(this);
		FileSource->SetFilePath(FPaths::ProjectContentDir() / TEXT("Movies/Menu.mp4"));
		FileSource->PrecacheFile = true;
		MediaSource = FileSource;
	}

	if (!MediaTexture)
	{
		MediaTexture = LoadObject<UMediaTexture>(nullptr, TEXT("/Game/DirectorsCut/UI/Media/MT_MainMenu.MT_MainMenu"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!MediaTexture)
	{
		MediaTexture = NewObject<UMediaTexture>(this);
		MediaTexture->NewStyleOutput = true;
	}
	// Always bind MediaTexture -> MediaPlayer (PostLoad only restores the serialized MediaPlayer UPROPERTY)
	MediaTexture->SetMediaPlayer(MediaPlayer);
	MediaTexture->UpdateResource();

	if (VideoMaterial)
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(VideoMaterial);
		Brush.ImageSize = FVector2D(1920.f, 1080.f);
		VideoImage->SetBrush(Brush);
	}
	else
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(MediaTexture);
		Brush.ImageSize = FVector2D(1920.f, 1080.f);
		VideoImage->SetBrush(Brush);
	}

	MediaPlayer->PlayOnOpen = true;
	MediaPlayer->SetLooping(true);
	// Video audio stays muted so FMOD owns all menu audio
	MediaPlayer->SetNativeVolume(0.f);
	if (MediaSource)
	{
		MediaPlayer->OpenSource(MediaSource);
	}
}

void UDCMainMenuWidget::Cue(const TCHAR* Name) const
{
	if (const ADCMainMenuPlayerController* MenuPC = Cast<ADCMainMenuPlayerController>(GetOwningPlayer()))
	{
		MenuPC->PlayCue(Name);
	}
}

void UDCMainMenuWidget::RefreshMainButtons()
{
	if (PlayButton)
	{
		const UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr;
		PlayButton->SetIsEnabled(GI && GI->HasAnySaveSlot());
	}
}

void UDCMainMenuWidget::ShowPage(EMenuPage Page)
{
	CurrentPage = Page;
	PendingOverwriteSlot.Empty();
	PendingDeleteSlot.Empty();
	if (OverwriteConfirmBox)
	{
		OverwriteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (DeleteConfirmBox)
	{
		DeleteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (Page != EMenuPage::Options && Settings)
	{
		Settings->CancelCapture();
	}

	if (PanelBorder)
	{
		PanelBorder->SetStyle(Page == EMenuPage::Main ? UDCBorder_Transparent::StaticClass() : UDCBorder_Panel::StaticClass());
		PanelBorder->SetPadding(Page == EMenuPage::Main ? FMargin(0.f, 10.f, 0.f, 0.f) : FMargin(20.f, 16.f));
	}
	MainButtons->SetVisibility(Page == EMenuPage::Main ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SlotsPage->SetVisibility(Page == EMenuPage::Continue || Page == EMenuPage::NewGame ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	CreditsPage->SetVisibility(Page == EMenuPage::Credits ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OptionsPage->SetVisibility(Page == EMenuPage::Options ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (Page == EMenuPage::Continue || Page == EMenuPage::NewGame)
	{
		RefreshSlots(Page == EMenuPage::NewGame);
	}
}

void UDCMainMenuWidget::RefreshDynamicTexts()
{
	if (CurrentPage == EMenuPage::Continue || CurrentPage == EMenuPage::NewGame)
	{
		RefreshSlots(CurrentPage == EMenuPage::NewGame);
	}
	if (!PendingOverwriteSlot.IsEmpty() && OverwriteConfirmText)
	{
		OverwriteConfirmText->SetText(FText::Format(LOCTEXT("Menu_OverwriteSlot", "OVERWRITE {0}?"), FText::FromString(PendingOverwriteSlot.ToUpper())));
	}
	if (!PendingDeleteSlot.IsEmpty() && DeleteConfirmText)
	{
		DeleteConfirmText->SetText(FText::Format(LOCTEXT("Menu_DeleteSlot", "DELETE {0}?"), FText::FromString(PendingDeleteSlot.ToUpper())));
	}
}

void UDCMainMenuWidget::RefreshSlots(bool bForNewGame)
{
	SlotsTitle->SetText(bForNewGame
		? LOCTEXT("Menu_NewGameSlots", "NEW GAME - SELECT SLOT")
		: LOCTEXT("Menu_ContinueSlots", "CONTINUE - SELECT SLOT"));
	SlotsList->ClearChildren();

	const UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr;
	if (!GI)
	{
		return;
	}

	for (const FDCSaveSlotSummary& SlotSummary : GI->GetSaveSlots())
	{
		FText Label;
		if (SlotSummary.bExists)
		{
			const FDateTime Timestamp = SlotSummary.LastSavedAt.GetTicks() > 0 ? SlotSummary.LastSavedAt : SlotSummary.CreatedAt;
			const FString DateStr = Timestamp.ToString(TEXT("%Y-%m-%d %H:%M"));
			Label = FText::Format(LOCTEXT("Menu_SlotSummary", "SLOT {0}  |  DAY {1}  |  ${2}  |  {3}"),
				FText::FromString(FString::FromInt(SlotSummary.SlotIndex + 1)),
				FText::FromString(FString::FromInt(SlotSummary.DayIndex)),
				FText::FromString(FString::FromInt(SlotSummary.Money)),
				FText::FromString(DateStr));
		}
		else
		{
			Label = FText::Format(LOCTEXT("Menu_SlotEmpty", "SLOT {0}  |  EMPTY"), FText::FromString(FString::FromInt(SlotSummary.SlotIndex + 1)));
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		SlotsList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

		UDCButton* Btn = DCUI::Button(WidgetTree, Row, Label, FName(*SlotSummary.SlotName), UDCButton_Menu::StaticClass());
		Btn->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleSlotClicked);
		if (UHorizontalBoxSlot* BtnSlot = Cast<UHorizontalBoxSlot>(Btn->Slot))
		{
			BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BtnSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (!bForNewGame && !SlotSummary.bExists)
		{
			Btn->SetIsEnabled(false);
		}

		if (SlotSummary.bExists)
		{
			UDCButton* DelBtn = DCUI::Button(WidgetTree, Row, LOCTEXT("Menu_Delete", "DELETE"), FName(*SlotSummary.SlotName), UDCButton_Menu::StaticClass());
			DelBtn->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleDeleteSlotClicked);
			if (UHorizontalBoxSlot* DelSlot = Cast<UHorizontalBoxSlot>(DelBtn->Slot))
			{
				DelSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
				DelSlot->SetVerticalAlignment(VAlign_Center);
			}
		}
	}
}

void UDCMainMenuWidget::HandlePlay(FName Unused)
{
	ShowPage(EMenuPage::Continue);
}

void UDCMainMenuWidget::HandleNewGame(FName Unused)
{
	ShowPage(EMenuPage::NewGame);
}

void UDCMainMenuWidget::HandleCredits(FName Unused)
{
	ShowPage(EMenuPage::Credits);
}

void UDCMainMenuWidget::HandleOptions(FName Unused)
{
	ShowPage(EMenuPage::Options);
}

void UDCMainMenuWidget::HandleBackToMain(FName Unused)
{
	ShowPage(EMenuPage::Main);
}

void UDCMainMenuWidget::HandleSlotClicked(FName SlotName)
{
	PendingDeleteSlot.Empty();
	if (DeleteConfirmBox)
	{
		DeleteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	const FString SlotStr = SlotName.ToString();

	if (CurrentPage == EMenuPage::Continue)
	{
		StartGameOnSlot(SlotStr, false);
		return;
	}

	if (UGameplayStatics::DoesSaveGameExist(SlotStr, UDCGameInstance::SaveUserIndex))
	{
		PendingOverwriteSlot = SlotStr;
		OverwriteConfirmText->SetText(FText::Format(LOCTEXT("Menu_OverwriteSlot", "OVERWRITE {0}?"), FText::FromString(SlotStr.ToUpper())));
		OverwriteConfirmBox->SetVisibility(ESlateVisibility::Visible);
		return;
	}

	StartGameOnSlot(SlotStr, true);
}

void UDCMainMenuWidget::HandleConfirmOverwrite(FName Unused)
{
	if (!PendingOverwriteSlot.IsEmpty())
	{
		StartGameOnSlot(PendingOverwriteSlot, true);
	}
}

void UDCMainMenuWidget::HandleCancelOverwrite(FName Unused)
{
	PendingOverwriteSlot.Empty();
	OverwriteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCMainMenuWidget::HandleDeleteSlotClicked(FName SlotName)
{
	PendingOverwriteSlot.Empty();
	if (OverwriteConfirmBox)
	{
		OverwriteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	PendingDeleteSlot = SlotName.ToString();
	DeleteConfirmText->SetText(FText::Format(LOCTEXT("Menu_DeleteSlot", "DELETE {0}?"), FText::FromString(PendingDeleteSlot.ToUpper())));
	DeleteConfirmBox->SetVisibility(ESlateVisibility::Visible);
}

void UDCMainMenuWidget::HandleConfirmDelete(FName Unused)
{
	if (!PendingDeleteSlot.IsEmpty())
	{
		if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
		{
			GI->DeleteSaveSlot(PendingDeleteSlot);
		}
		PendingDeleteSlot.Empty();
	}
	DeleteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
	RefreshMainButtons();
	RefreshSlots(CurrentPage == EMenuPage::NewGame);
}

void UDCMainMenuWidget::HandleCancelDelete(FName Unused)
{
	PendingDeleteSlot.Empty();
	DeleteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCMainMenuWidget::StartGameOnSlot(const FString& SlotName, bool bNewGame)
{
	UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr;
	if (!GI)
	{
		return;
	}

	if (bNewGame)
	{
		int32 StartingMoney = 50;
		if (const ADCMainMenuGameMode* MenuGM = GetWorld()->GetAuthGameMode<ADCMainMenuGameMode>())
		{
			if (MenuGM->Config)
			{
				StartingMoney = MenuGM->Config->StartMoney;
			}
		}
		GI->CreateNewSaveSlot(SlotName, StartingMoney);
	}
	else if (!GI->SelectSaveSlot(SlotName))
	{
		return;
	}

	// FMOD bracket: menu start cue before travelling to MAP_Rewind
	Cue(TEXT("MainMenuStart"));
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}
	GI->TravelToGameplay();
}

void UDCMainMenuWidget::HandleQuit(FName Unused)
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

// ---------------------------------------------------------------- Settings piece

// Swallows the next key (or non-left mouse button) before CommonUI sees it, so Esc cancels instead of going Back.
// Unregistering from inside a handler isn't safe, so the owner removes it later (see StopCapture).
class FDCKeyCapture : public IInputProcessor
{
public:
	TFunction<void(const FKey&)> OnKey; // EKeys::Invalid = cancelled
	bool bActive = true;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
	{
		const FKey Key = Event.GetKey();
		return bActive && !Key.IsGamepadKey() && Finish(Key == EKeys::Escape ? EKeys::Invalid : Key);
	}
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& Event) override
	{
		const FKey Button = Event.GetEffectingButton();
		if (!bActive)
		{
			return false;
		}
		if (Button == EKeys::LeftMouseButton)
		{
			Finish(EKeys::Invalid); // a click elsewhere cancels, and still reaches the UI
			return false;
		}
		return Finish(Button);
	}
	virtual const TCHAR* GetDebugName() const override { return TEXT("DCKeyCapture"); }

private:
	bool Finish(const FKey& Key)
	{
		bActive = false;
		OnKey(Key);
		return true;
	}
};

namespace
{
	FText QualityLabel(int32 Quality)
	{
		switch (FMath::Clamp(Quality, 0, 3))
		{
		case 0: return LOCTEXT("Quality_Low", "LOW");
		case 1: return LOCTEXT("Quality_Medium", "MEDIUM");
		case 2: return LOCTEXT("Quality_High", "HIGH");
		default: return LOCTEXT("Quality_Ultra", "ULTRA");
		}
	}

	void StepValue(float& Value, float Delta, float Min, float Max)
	{
		Value = FMath::Clamp(FMath::RoundToFloat((Value + Delta) * 10.f) / 10.f, Min, Max);
	}
}

void UDCSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>();
	RootBox->SetMinDesiredWidth(640.f);
	WidgetTree->RootWidget = RootBox;
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	RootBox->AddChild(V);
	DCUI::Text(WidgetTree, V, LOCTEXT("Options_Title", "OPTIONS"), UDCText_Heading::StaticClass());
	UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(TabRow)->SetPadding(FMargin(0.f, 8.f, 0.f, 10.f));

	// Row with the label filling the left side, so whatever follows is pushed against the right edge
	auto AddRow = [this](UVerticalBox* Page, const FText& Label)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Page->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 4.f));
		UHorizontalBoxSlot* LabelSlot = H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, Label));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return H;
	};
	auto AddPage = [this, V, TabRow](const FText& Title)
	{
		UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>();
		V->AddChildToVerticalBox(Page);
		const int32 Index = Pages.Add(Page);
		UDCButton* Tab = DCUI::Button(WidgetTree, TabRow, Title, FName(*FString::FromInt(Index)), UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCSettingsWidget::HandleTab);
		Cast<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		TabButtons.Add(Tab);
		return Page;
	};
	// Value-plus-arrows control: [◀] value [▶] (Press Start 2P has the glyphs)
	auto AddStepper = [this, &AddRow](UVerticalBox* Page, const FText& Label, FName Down, FName Up)
	{
		UHorizontalBox* H = AddRow(Page, Label);
		UDCButton* Left = DCUI::Button(WidgetTree, H, TEXT("◀"), Down);
		UCommonTextBlock* Value = DCUI::Text(WidgetTree, nullptr, TEXT(""), UDCText_Heading::StaticClass());
		Value->SetJustification(ETextJustify::Center);
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
		ValueBox->SetMinDesiredWidth(160.f);
		ValueBox->AddChild(Value);
		UHorizontalBoxSlot* ValueSlot = H->AddChildToHorizontalBox(ValueBox);
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		ValueSlot->SetPadding(FMargin(8.f, 0.f));
		UDCButton* Right = DCUI::Button(WidgetTree, H, TEXT("▶"), Up);
		Left->OnPayloadClicked.AddDynamic(this, &UDCSettingsWidget::HandleStep);
		Right->OnPayloadClicked.AddDynamic(this, &UDCSettingsWidget::HandleStep);
		return Value;
	};
	// Slider plus a percent readout; the slider's size box is the row's slot
	auto AddSlider = [this, &AddRow](UVerticalBox* Page, const FText& Label, FName Id)
	{
		UHorizontalBox* H = AddRow(Page, Label);
		USlider* Slider = DCUI::Slider(WidgetTree, H, 240.f);
		Cast<UHorizontalBoxSlot>(Slider->GetParent()->Slot)->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Value = DCUI::Text(WidgetTree, nullptr, TEXT(""), UDCText_Heading::StaticClass());
		Value->SetJustification(ETextJustify::Right);
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
		ValueBox->SetMinDesiredWidth(110.f);
		ValueBox->AddChild(Value);
		UHorizontalBoxSlot* ValueSlot = H->AddChildToHorizontalBox(ValueBox);
		ValueSlot->SetVerticalAlignment(VAlign_Center);
		ValueSlot->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
		VolumeSliders.Add(Id, Slider);
		VolumeTexts.Add(Id, Value);
		return Slider;
	};

	UVerticalBox* Controls = AddPage(LOCTEXT("Options_TabControls", "CONTROLS"));
	for (int32 i = 0; i < (int32)EDCAction::Count; ++i)
	{
		UHorizontalBox* H = AddRow(Controls, UDCSettings::ActionDisplayLabel((EDCAction)i));
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, TEXT(""), FName(*FString::FromInt(i)));
		USizeBox* BtnBox = WidgetTree->ConstructWidget<USizeBox>();
		BtnBox->SetMinDesiredWidth(240.f);
		BtnBox->AddChild(Btn);
		H->AddChildToHorizontalBox(BtnBox);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCSettingsWidget::HandleRebind);
		KeyButtons.Add(Btn);
	}
	SensitivityText = AddStepper(Controls, LOCTEXT("Options_MouseSens", "Mouse sensitivity"), "SensDown", "SensUp");

	UVerticalBox* Audio = AddPage(LOCTEXT("Options_TabAudio", "AUDIO"));
	AddSlider(Audio, LOCTEXT("Options_Master", "Master"), "Master")->OnValueChanged.AddDynamic(this, &UDCSettingsWidget::HandleMasterVolume);
	AddSlider(Audio, LOCTEXT("Options_SFX", "SFX"), "Sfx")->OnValueChanged.AddDynamic(this, &UDCSettingsWidget::HandleSfxVolume);
	AddSlider(Audio, LOCTEXT("Options_Music", "Music"), "Music")->OnValueChanged.AddDynamic(this, &UDCSettingsWidget::HandleMusicVolume);

	UVerticalBox* Graphics = AddPage(LOCTEXT("Options_TabGraphics", "GRAPHICS"));
	QualityText = AddStepper(Graphics, LOCTEXT("Options_Quality", "Quality"), "QualDown", "QualUp");
	LumenText = AddStepper(Graphics, LOCTEXT("Options_Lumen", "Lumen (GI + reflections)"), "LumenDown", "LumenUp");
	FpsText = AddStepper(Graphics, LOCTEXT("Options_FpsCap", "Frame rate cap"), "FpsDown", "FpsUp");
	VSyncText = AddStepper(Graphics, LOCTEXT("Options_VSync", "VSync"), "VSyncDown", "VSyncUp");
	ResScaleText = AddStepper(Graphics, LOCTEXT("Options_ResScale", "Resolution scale"), "ResDown", "ResUp");
	FullscreenText = AddStepper(Graphics, LOCTEXT("Options_Fullscreen", "Fullscreen"), "FullDown", "FullUp");

	UVerticalBox* LanguagePage = AddPage(LOCTEXT("Options_TabLanguage", "LANGUAGE"));
	LanguageText = AddStepper(LanguagePage, LOCTEXT("Options_Language", "Language"), "LangDown", "LangUp");

	ShowTab(0);
	Refresh();
	LanguageChangedHandle = UDCSettings::OnLanguageChanged.AddUObject(this, &UDCSettingsWidget::HandleLanguageChanged);
}

void UDCSettingsWidget::NativeDestruct()
{
	UDCSettings::OnLanguageChanged.Remove(LanguageChangedHandle);
	Super::NativeDestruct();
}

void UDCSettingsWidget::HandleLanguageChanged()
{
	Refresh();
}

void UDCSettingsWidget::ShowTab(int32 Index)
{
	CancelCapture();
	for (int32 i = 0; i < Pages.Num(); ++i)
	{
		Pages[i]->SetVisibility(i == Index ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		i == Index ? TabButtons[i]->SetIsSelected(true, false) : TabButtons[i]->ClearSelection();
	}
}

void UDCSettingsWidget::HandleTab(FName Index)
{
	ShowTab(FCString::Atoi(*Index.ToString()));
}

void UDCSettingsWidget::SetVolume(float& Field, float Value)
{
	Field = FMath::Clamp(FMath::RoundToFloat(Value * 20.f) / 20.f, 0.f, 1.f);
	UDCSettings::Get()->Save();
	Refresh();
}

void UDCSettingsWidget::Refresh()
{
	UDCSettings* S = UDCSettings::Get();
	for (int32 i = 0; i < KeyButtons.Num(); ++i)
	{
		KeyButtons[i]->Label->SetText(i == CapturingAction ? LOCTEXT("Options_PressKey", "PRESS A KEY...") : S->Key((EDCAction)i).GetDisplayName());
	}
	const TPair<FName, float> Volumes[] = {{"Master", S->MasterVolume}, {"Sfx", S->SfxVolume}, {"Music", S->MusicVolume}};
	for (const TPair<FName, float>& Vol : Volumes)
	{
		VolumeSliders[Vol.Key]->SetValue(Vol.Value);
		VolumeTexts[Vol.Key]->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Vol.Value * 100.f))));
	}
	const FText OnText = LOCTEXT("Options_On", "ON");
	const FText OffText = LOCTEXT("Options_Off", "OFF");
	SensitivityText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), S->MouseSensitivityScale)));
	QualityText->SetText(QualityLabel(S->Quality));
	LumenText->SetText(S->bLumen ? OnText : OffText);
	FpsText->SetText(S->FpsCap > 0 ? FText::FromString(FString::FromInt(S->FpsCap)) : LOCTEXT("Options_None", "NONE"));
	VSyncText->SetText(S->bVSync ? OnText : OffText);
	ResScaleText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), S->ResolutionScale)));
	FullscreenText->SetText(S->bFullscreen ? OnText : OffText);
	if (LanguageText)
	{
		LanguageText->SetText(UDCSettings::LanguageDisplayName(S->GetEffectiveLanguage()));
	}
}

void UDCSettingsWidget::HandleStep(FName Id)
{
	UDCSettings* S = UDCSettings::Get();
	if (Id == "LangUp" || Id == "LangDown")
	{
		const FString Next = S->GetEffectiveLanguage() == TEXT("es") ? TEXT("en") : TEXT("es");
		S->SetLanguage(Next);
		return;
	}
	const bool bUp = Id.ToString().EndsWith(TEXT("Up"));
	const float Dir = bUp ? 0.1f : -0.1f;
	if (Id == "SensUp" || Id == "SensDown")
	{
		StepValue(S->MouseSensitivityScale, Dir, 0.1f, 3.f);
	}
	else if (Id == "QualUp" || Id == "QualDown")
	{
		S->Quality = FMath::Clamp(S->Quality + (bUp ? 1 : -1), 0, 3);
	}
	else if (Id == "LumenUp" || Id == "LumenDown")
	{
		S->bLumen = !S->bLumen;
	}
	else if (Id == "VSyncUp" || Id == "VSyncDown")
	{
		S->bVSync = !S->bVSync;
	}
	else if (Id == "FullUp" || Id == "FullDown")
	{
		S->bFullscreen = !S->bFullscreen;
	}
	else if (Id == "ResUp" || Id == "ResDown")
	{
		S->ResolutionScale = FMath::Clamp(S->ResolutionScale + (bUp ? 10 : -10), 50, 100);
	}
	else // frame cap: steps through the list, 0 (unlimited) last
	{
		const int32 Caps[] = {30, 60, 120, 0};
		int32 Idx = 1;
		for (int32 i = 0; i < UE_ARRAY_COUNT(Caps); ++i)
		{
			if (Caps[i] == S->FpsCap)
			{
				Idx = i;
			}
		}
		S->FpsCap = Caps[FMath::Clamp(Idx + (bUp ? 1 : -1), 0, UE_ARRAY_COUNT(Caps) - 1)];
	}
	S->Save();
	Refresh();
}

void UDCSettingsWidget::HandleRebind(FName ActionIndex)
{
	CancelCapture();
	CapturingAction = FCString::Atoi(*ActionIndex.ToString());
	Capture = MakeShared<FDCKeyCapture>();
	Capture->OnKey = [Weak = TWeakObjectPtr<UDCSettingsWidget>(this)](const FKey& Key)
	{
		if (Weak.IsValid())
		{
			Weak->FinishCapture(Key);
		}
	};
	FSlateApplication::Get().RegisterInputPreProcessor(Capture, 0); // index 0: ahead of CommonUI's own preprocessor
	Refresh();
}

void UDCSettingsWidget::FinishCapture(const FKey& Key)
{
	if (Key.IsValid())
	{
		UDCSettings* S = UDCSettings::Get();
		FKey& Mine = S->Key((EDCAction)CapturingAction);
		for (int32 i = 0; i < (int32)EDCAction::Count; ++i)
		{
			if (i != CapturingAction && S->Key((EDCAction)i) == Key)
			{
				S->Key((EDCAction)i) = Mine; // taken: swap, so no two actions share a key
			}
		}
		Mine = Key;
		S->Save();
	}
	CancelCapture();
}

void UDCSettingsWidget::CancelCapture()
{
	if (Capture.IsValid())
	{
		Capture->bActive = false;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Processor = Capture](float)
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
			}
			return false;
		}));
		Capture.Reset();
	}
	CapturingAction = INDEX_NONE;
	if (SensitivityText)
	{
		Refresh();
	}
}

// ---------------------------------------------------------------- Pause

UDCPauseWidget::UDCPauseWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
}

void UDCPauseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, LOCTEXT("Pause_Title", "PAUSED"));
	MainButtons = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(MainButtons);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Pause_Continue", "CONTINUE"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleContinue);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Pause_Options", "OPTIONS"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleOptions);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Pause_Restart", "RESTART"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleRestart);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Pause_MainMenu", "BACK TO MAIN MENU"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleMainMenu);
	MenuButton(WidgetTree, MainButtons, LOCTEXT("Pause_Quit", "QUIT"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleQuit);
	Settings = WidgetTree->ConstructWidget<UDCSettingsWidget>();
	V->AddChild(Settings);
	OptionsBackButton = MenuButton(WidgetTree, V, LOCTEXT("Menu_Back", "BACK"));
	OptionsBackButton->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleOptionsBack);
	ShowOptions(false);
}

void UDCPauseWidget::ShowOptions(bool bShow)
{
	if (!bShow)
	{
		Settings->CancelCapture();
	}
	MainButtons->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	Settings->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OptionsBackButton->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UDCPauseWidget::NativeOnDeactivated()
{
	ShowOptions(false); // the stack reuses this instance: next time it opens on the main buttons
	Super::NativeOnDeactivated();
}

bool UDCPauseWidget::NativeOnHandleBackAction()
{
	if (Settings->GetVisibility() == ESlateVisibility::Visible)
	{
		ShowOptions(false); // Esc/B inside the options goes back to the pause buttons, not out of the menu
		return true;
	}
	return Super::NativeOnHandleBackAction();
}

void UDCPauseWidget::HandleRestart(FName Unused)
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void UDCPauseWidget::HandleMainMenu(FName Unused)
{
	if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
	{
		GI->TravelToMainMenu();
		return;
	}
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/_MainGame/Level/MAP_MainMenu")));
}

void UDCPauseWidget::HandleQuit(FName Unused)
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE
