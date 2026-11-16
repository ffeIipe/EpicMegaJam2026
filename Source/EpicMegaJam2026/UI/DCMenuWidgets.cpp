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

namespace
{
	const FAnchors Center(0.5f, 0.5f);
	const FVector2D Middle(0.5f, 0.5f);

	// Stacked menu buttons, stretched to the panel width
	UDCButton* MenuButton(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label, TSubclassOf<UCommonButtonStyle> Style = UDCButton_Primary::StaticClass())
	{
		UDCButton* Btn = DCUI::Button(Tree, Parent, Label, NAME_None, Style);
		Cast<UVerticalBoxSlot>(Btn->Slot)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		return Btn;
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
	MenuButton(WidgetTree, V, TEXT("Open the store"))->OnPayloadClicked.AddDynamic(this, &UDCDaySummaryWidget::HandleOpen);
}

void UDCDaySummaryWidget::Show(const FDaySummary& S)
{
	TitleText->SetText(FText::FromString(FString::Printf(TEXT("DAY %d COMPLETE"), S.DayIndex)));
	BodyText->SetText(FText::FromString(FString::Printf(
		TEXT("Clients served: %d\nClients lost: %d\nLoved: %d   Liked: %d   Neutral: %d   Hated: %d\nMoney earned: $%d\nRent paid: $%d\nMoney: $%d / goal $%d"),
		S.ClientsServed, S.ClientsLost, S.Loved, S.Liked, S.Neutral, S.Hated, S.MoneyEarned, S.RentPaid, S.MoneyNow, S.Goal)));
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
	DCUI::Header(WidgetTree, V, TEXT("SUPPLIER"), TEXT("Tapes for sale, one copy at a time"));
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

	// Genre tabs: the unlocked ones, in DT_Genres order
	Tabs->ClearChildren();
	TArray<FName> Genres;
	for (const TPair<FName, uint8*>& P : Config->GenreTable->GetRowMap())
	{
		const FGenreData* Row = reinterpret_cast<const FGenreData*>(P.Value);
		if (Row->Genre != EGenre::Paranormal && Unlocks->IsGenreUnlocked(Row->Genre))
		{
			Genres.Add(FName(*StaticEnum<EGenre>()->GetNameStringByValue((int64)Row->Genre)));
		}
	}
	if (!Genres.Contains(SelectedGenre))
	{
		SelectedGenre = Genres.IsEmpty() ? NAME_None : Genres[0];
	}
	for (const FName& Genre : Genres)
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, Tabs, Genre.ToString().ToUpper(), Genre, UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::OnTabClicked);
		Cast<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
		Select(Tab, Genre == SelectedGenre);
	}

	// Star filter: up to the licensed level
	StarFilter->ClearChildren();
	if (SelectedStars > Unlocks->UnlockedStarTier)
	{
		SelectedStars = 0;
	}
	for (int32 Stars = 0; Stars <= Unlocks->UnlockedStarTier; ++Stars)
	{
		const FString Label = Stars == 0 ? TEXT("ALL STARS") : FString::Printf(TEXT("%d STAR%s"), Stars, Stars > 1 ? TEXT("S") : TEXT(""));
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
		if (Inventory->IsTapeForSale(P.Key) && StaticEnum<EGenre>()->GetNameStringByValue((int64)Tape->PrimaryGenre) == SelectedGenre.ToString()
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
		DCUI::Text(WidgetTree, List, TEXT("Nothing on sale here yet."));
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
	DCUI::Header(WidgetTree, V, TEXT("COMPUTER"), TEXT("Upgrade the store, check the rent and read your reviews"));
	MoneyText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Heading::StaticClass());
	MoneyText->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	const TPair<const TCHAR*, const TCHAR*> TabDefs[] = { { TEXT("Genres"), TEXT("GENRES") }, { TEXT("Machine"), TEXT("COMBINE MACHINE") }, { TEXT("Stars"), TEXT("STAR LICENSES") }, { TEXT("Rent"), TEXT("RENT") }, { TEXT("Reviews"), TEXT("REVIEWS") } };
	for (const TPair<const TCHAR*, const TCHAR*>& Def : TabDefs)
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
	MoneyText->SetText(FText::FromString(FString::Printf(TEXT("Money: $%d"), GM->Economy->Money)));
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
		MoneyText->SetText(FText::FromString(FString::Printf(TEXT("Money: $%d"), NewMoney)));
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
	UVerticalBox* Now = AddCard(WidgetTree, List, Money >= Rent ? DCPalette::Lime : DCPalette::Red);
	DCUI::Text(WidgetTree, Now, FString::Printf(TEXT("RENT FOR DAY %d: $%d"), Day, Rent), UDCText_Heading::StaticClass());
	DCUI::Text(WidgetTree, Now, TEXT("It is collected automatically when the night ends. If you can't cover it, you are evicted."))->SetWrapTextAt(520.f);
	DCUI::Text(WidgetTree, Now, Money >= Rent
		? FString::Printf(TEXT("Covered: you have $%d, $%d to spare."), Money, Money - Rent)
		: FString::Printf(TEXT("Short by $%d: you have $%d."), Rent - Money, Money),
		Money >= Rent ? UDCText_Accent::StaticClass() : UDCText_Warn::StaticClass());
	DCUI::Text(WidgetTree, List, TEXT("COMING UP"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 6.f, 0.f, 8.f));
	for (int32 Next = Day + 1; Next <= Day + 3; ++Next)
	{
		UHorizontalBox* Row = nullptr;
		UVerticalBox* Info = AddCard(WidgetTree, List, DCPalette::Blue, &Row);
		DCUI::Text(WidgetTree, Info, FString::Printf(TEXT("DAY %d"), Next), UDCText_Heading::StaticClass());
		Row->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("$%d"), GM->Economy->GetRentForDay(Next)), UDCText_Caution::StaticClass()))->SetVerticalAlignment(VAlign_Center);
	}
}

void UDCComputerWidget::BuildReviews()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	const TArray<FReview>& History = GM->ClientDirector->ReviewHistory;
	if (History.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, TEXT("No reviews yet. Deliver a film and ReelTalk.net will have an opinion."));
		return;
	}
	for (const FReview& R : History)
	{
		const int32* Stars = Config->ReviewStars.Find(R.Reaction);
		const FLinearColor Tile = R.Reaction == EReaction::Loved ? DCPalette::Lime : R.Reaction == EReaction::Liked ? DCPalette::Green
			: R.Reaction == EReaction::Neutral ? DCPalette::Yellow : DCPalette::Red;
		UHorizontalBox* H = nullptr;
		UVerticalBox* Info = AddCard(WidgetTree, List, Tile, &H);
		DCUI::Text(WidgetTree, Info, R.FilmTitle.ToString(), UDCText_Heading::StaticClass());
		DCUI::Text(WidgetTree, Info, R.Line.ToString())->SetWrapTextAt(440.f);
		if (!R.FeedbackLine.IsEmpty())
		{
			DCUI::Text(WidgetTree, Info, R.FeedbackLine.ToString(), UDCText_Accent::StaticClass())->SetWrapTextAt(440.f);
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
		DCUI::Text(WidgetTree, Info, Row->Description.ToString())->SetWrapTextAt(440.f);
		TArray<FString> Needs;
		for (const FName& Required : Row->RequiresUnlocks)
		{
			const FUnlockData* Other = Config->UnlockTable->FindRow<FUnlockData>(Required, TEXT("Requires"), false);
			Needs.Add(Other ? Other->DisplayName.ToString() : Required.ToString());
		}
		if (Row->MinGenresOwned > 0)
		{
			Needs.Add(FString::Printf(TEXT("%d genres unlocked (you have %d)"), Row->MinGenresOwned, Unlocks->GetGenreCount()));
		}
		if (!bOwned && Needs.Num() > 0)
		{
			DCUI::Text(WidgetTree, Info, TEXT("Requires: ") + FString::Join(Needs, TEXT(", ")), UDCText_Warn::StaticClass())->SetWrapTextAt(440.f);
		}
		UVerticalBox* Buy = WidgetTree->ConstructWidget<UVerticalBox>();
		H->AddChildToHorizontalBox(Buy)->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Price = DCUI::Text(WidgetTree, Buy, bOwned ? FString() : FString::Printf(TEXT("$%d"), Row->Price), UDCText_Caution::StaticClass());
		Price->SetJustification(ETextJustify::Center);
		Cast<UVerticalBoxSlot>(Price->Slot)->SetHorizontalAlignment(HAlign_Center);
		UDCButton* Btn = DCUI::Button(WidgetTree, Buy, bOwned ? TEXT("OWNED") : TEXT("BUY"), P.Key);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCComputerWidget::HandleBuy);
		Buttons.Add({ Btn, P.Key });
	}
	if (Ids.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, TEXT("Nothing to buy here."));
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
	DCUI::Header(WidgetTree, V, TEXT("VHS INVENTORY"));
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

void UDCTutorialWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>();
	Stack->SetVisibility(ESlateVisibility::Collapsed);
	Panel = Stack;
	PanelSlot = Canvas->AddChildToCanvas(Stack);
	PanelSlot->SetAutoSize(true);
	SetTopPlacement(false);

	UHorizontalBox* TopRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	TopRow->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Stack->AddChildToVerticalBox(TopRow)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	UCommonBorder* HeaderPlate = DCUI::Border(WidgetTree, TopRow, UDCBorder_Header::StaticClass(), FMargin(48.f, 8.f, 36.f, 8.f));
	HeaderPlate->SetVisibility(ESlateVisibility::HitTestInvisible);
	StepTitleText = DCUI::Text(WidgetTree, nullptr, TEXT("TUTORIAL"), UDCText_Heading::StaticClass());
	HeaderPlate->SetContent(StepTitleText);

	USpacer* FillGap = WidgetTree->ConstructWidget<USpacer>();
	FillGap->SetVisibility(ESlateVisibility::HitTestInvisible);
	TopRow->AddChildToHorizontalBox(FillGap)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	SkipButton = DCUI::Button(WidgetTree, TopRow, TEXT("Skip (F3)"), NAME_None, UDCButton_Tab::StaticClass());
	SkipButton->SetIsFocusable(false);
	SkipButton->OnPayloadClicked.AddDynamic(this, &UDCTutorialWidget::HandleSkip);
	if (UHorizontalBoxSlot* SkipSlot = Cast<UHorizontalBoxSlot>(SkipButton->Slot))
	{
		SkipSlot->SetVerticalAlignment(VAlign_Bottom);
		SkipSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	}

	UCommonBorder* BodyBorder = DCUI::Border(WidgetTree, Stack, UDCBorder_Panel::StaticClass(), FMargin(18.f, 10.f));
	BodyBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	UVerticalBox* BodyV = WidgetTree->ConstructWidget<UVerticalBox>();
	BodyV->SetVisibility(ESlateVisibility::HitTestInvisible);
	BodyBorder->SetContent(BodyV);

	HintText = DCUI::Text(WidgetTree, BodyV, TEXT(""), UDCText_Body::StaticClass());
	HintText->SetWrapTextAt(780.f);
	ActionText = DCUI::Text(WidgetTree, BodyV, TEXT(""), UDCText_Accent::StaticClass());
	ActionText->SetWrapTextAt(780.f);
	ActionText->SetMargin(FMargin(0.f, 4.f, 0.f, 0.f));
}

void UDCTutorialWidget::SetPlacement(ETutorialPlacement Placement)
{
	if (!PanelSlot)
	{
		return;
	}
	const float WrapWidth = Placement == ETutorialPlacement::TopRight ? 540.f : 780.f;
	if (HintText)
	{
		HintText->SetWrapTextAt(WrapWidth);
	}
	if (ActionText)
	{
		ActionText->SetWrapTextAt(WrapWidth);
	}
	switch (Placement)
	{
	case ETutorialPlacement::TopCenter:
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.f));
		PanelSlot->SetPosition(FVector2D(0.f, 20.f));
		break;
	case ETutorialPlacement::TopRight:
		PanelSlot->SetAnchors(FAnchors(1.f, 0.f));
		PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
		PanelSlot->SetPosition(FVector2D(-24.f, 14.f));
		break;
	default:
		PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
		PanelSlot->SetPosition(FVector2D(0.f, -18.f));
		break;
	}
}

void UDCTutorialWidget::ShowHint(const FText& Text)
{
	ShowStep(FText::FromString(TEXT("TUTORIAL")), Text, FText::GetEmpty(), true);
}

void UDCTutorialWidget::ShowStep(const FText& StepHeader, const FText& Body, const FText& Action, bool bShowSkip)
{
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
	if (Panel)
	{
		Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UDCTutorialWidget::HideHint()
{
	if (Panel)
	{
		Panel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDCTutorialWidget::HandleSkip(FName Unused)
{
	OnSkipped.Broadcast();
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
	HeaderPlate->SetContent(DCUI::Text(WidgetTree, nullptr, TEXT("RENT TONIGHT"), UDCText_Title::StaticClass()));
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
	PlayButton = MenuButton(WidgetTree, MainButtons, TEXT("PLAY"), UDCButton_Menu::StaticClass());
	PlayButton->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandlePlay);
	MenuButton(WidgetTree, MainButtons, TEXT("NEW GAME"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleNewGame);
	MenuButton(WidgetTree, MainButtons, TEXT("CREDITS"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleCredits);
	MenuButton(WidgetTree, MainButtons, TEXT("OPTIONS"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleOptions);
	MenuButton(WidgetTree, MainButtons, TEXT("EXIT"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleQuit);

	// --- Save slots sub-page (Continue / New Game)
	SlotsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(SlotsPage);
	SlotsTitle = DCUI::Text(WidgetTree, SlotsPage, TEXT(""), UDCText_Heading::StaticClass());
	SlotsTitle->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	SlotsList = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPage->AddChild(SlotsList);

	OverwriteConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPage->AddChildToVerticalBox(OverwriteConfirmBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	OverwriteConfirmText = DCUI::Text(WidgetTree, OverwriteConfirmBox, TEXT("OVERWRITE THIS SAVE SLOT?"), UDCText_Caution::StaticClass());
	MenuButton(WidgetTree, OverwriteConfirmBox, TEXT("CONFIRM OVERWRITE"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleConfirmOverwrite);
	MenuButton(WidgetTree, OverwriteConfirmBox, TEXT("CANCEL"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleCancelOverwrite);
	OverwriteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);

	DeleteConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPage->AddChildToVerticalBox(DeleteConfirmBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	DeleteConfirmText = DCUI::Text(WidgetTree, DeleteConfirmBox, TEXT("DELETE THIS SAVE SLOT?"), UDCText_Caution::StaticClass());
	MenuButton(WidgetTree, DeleteConfirmBox, TEXT("CONFIRM DELETE"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleConfirmDelete);
	MenuButton(WidgetTree, DeleteConfirmBox, TEXT("CANCEL"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleCancelDelete);
	DeleteConfirmBox->SetVisibility(ESlateVisibility::Collapsed);

	MenuButton(WidgetTree, SlotsPage, TEXT("BACK"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleBackToMain);

	// --- Credits sub-page
	CreditsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(CreditsPage);
	UCommonTextBlock* CreditsHeader = DCUI::Text(WidgetTree, CreditsPage, TEXT("CREDITS"), UDCText_Heading::StaticClass());
	CreditsHeader->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));

	auto AddCredit = [this](const TCHAR* Name, const TCHAR* Role)
	{
		UCommonTextBlock* NameText = DCUI::Text(WidgetTree, CreditsPage, Name, UDCText_Heading::StaticClass());
		NameText->SetMargin(FMargin(0.f, 8.f, 0.f, 0.f));
		UCommonTextBlock* RoleText = DCUI::Text(WidgetTree, CreditsPage, Role, UDCText_Body::StaticClass());
		RoleText->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
	};
	AddCredit(TEXT("Couture Augusto"), TEXT("3D Props Artist & Technical Art"));
	AddCredit(TEXT("Londonio Gabriel"), TEXT("Art Direction, 3D Environment Art & Cinematics"));
	AddCredit(TEXT("Felipe Nuñez"), TEXT("Gameplay Systems Designer & Programmer"));
	AddCredit(TEXT("Patricio Mackinlay"), TEXT("Technical Sound Designer & Composer"));

	UCommonTextBlock* StudioFooter = DCUI::Text(WidgetTree, CreditsPage, TEXT("Ampersand Games"), UDCText_Accent::StaticClass());
	StudioFooter->SetMargin(FMargin(0.f, 12.f, 0.f, 4.f));
	MenuButton(WidgetTree, CreditsPage, TEXT("BACK"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleBackToMain);

	// --- Options sub-page (reuses UDCSettingsWidget)
	OptionsPage = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(OptionsPage);
	Settings = WidgetTree->ConstructWidget<UDCSettingsWidget>();
	OptionsPage->AddChild(Settings);
	MenuButton(WidgetTree, OptionsPage, TEXT("BACK"), UDCButton_Menu::StaticClass())->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleBackToMain);

	// Studio badge outside the menu in the bottom-right corner (mirrors bottom-left action bar margin)
	UCommonBorder* StudioBadge = DCUI::Border(WidgetTree, nullptr, UDCBorder_Inset::StaticClass(), FMargin(14.f, 8.f));
	StudioBadge->SetContent(DCUI::Text(WidgetTree, nullptr, TEXT("AMPERSAND GAMES"), UDCText_Heading::StaticClass()));
	UCanvasPanelSlot* StudioSlot = Root->AddChildToCanvas(StudioBadge);
	StudioSlot->SetAnchors(FAnchors(1.f, 1.f));
	StudioSlot->SetAlignment(FVector2D(1.f, 1.f));
	StudioSlot->SetPosition(FVector2D(-48.f, -36.f));
	StudioSlot->SetAutoSize(true);

	SetupBackgroundVideo();
	RefreshMainButtons();
	ShowPage(EMenuPage::Main);
}

void UDCMainMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshMainButtons();
	ShowPage(EMenuPage::Main);
}

void UDCMainMenuWidget::NativeDestruct()
{
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

void UDCMainMenuWidget::RefreshSlots(bool bForNewGame)
{
	SlotsTitle->SetText(FText::FromString(bForNewGame ? TEXT("NEW GAME - SELECT SLOT") : TEXT("CONTINUE - SELECT SLOT")));
	SlotsList->ClearChildren();

	const UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr;
	if (!GI)
	{
		return;
	}

	for (const FDCSaveSlotSummary& SlotSummary : GI->GetSaveSlots())
	{
		FString Label;
		if (SlotSummary.bExists)
		{
			const FDateTime Timestamp = SlotSummary.LastSavedAt.GetTicks() > 0 ? SlotSummary.LastSavedAt : SlotSummary.CreatedAt;
			const FString DateStr = Timestamp.ToString(TEXT("%Y-%m-%d %H:%M"));
			Label = FString::Printf(TEXT("SLOT %d  |  DAY %d  |  $%d  |  %s"), SlotSummary.SlotIndex + 1, SlotSummary.DayIndex, SlotSummary.Money, *DateStr);
		}
		else
		{
			Label = FString::Printf(TEXT("SLOT %d  |  EMPTY"), SlotSummary.SlotIndex + 1);
		}

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		SlotsList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

		UDCButton* Btn = DCUI::Button(WidgetTree, Row, *Label, FName(*SlotSummary.SlotName), UDCButton_Menu::StaticClass());
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
			UDCButton* DelBtn = DCUI::Button(WidgetTree, Row, TEXT("DELETE"), FName(*SlotSummary.SlotName), UDCButton_Menu::StaticClass());
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
		OverwriteConfirmText->SetText(FText::FromString(FString::Printf(TEXT("OVERWRITE %s?"), *SlotStr.ToUpper())));
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
	DeleteConfirmText->SetText(FText::FromString(FString::Printf(TEXT("DELETE %s?"), *PendingDeleteSlot.ToUpper())));
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
	const TCHAR* const QualityNames[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("ULTRA")};

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
	DCUI::Text(WidgetTree, V, TEXT("OPTIONS"), UDCText_Heading::StaticClass());
	UHorizontalBox* TabRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(TabRow)->SetPadding(FMargin(0.f, 8.f, 0.f, 10.f));

	// Row with the label filling the left side, so whatever follows is pushed against the right edge
	auto AddRow = [this](UVerticalBox* Page, const FString& Label)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Page->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 4.f));
		UHorizontalBoxSlot* LabelSlot = H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, Label));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return H;
	};
	auto AddPage = [this, V, TabRow](const TCHAR* Title)
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
	auto AddStepper = [this, &AddRow](UVerticalBox* Page, const TCHAR* Label, FName Down, FName Up)
	{
		UHorizontalBox* H = AddRow(Page, Label);
		UDCButton* Left = DCUI::Button(WidgetTree, H, TEXT("◀"), Down);
		UCommonTextBlock* Value = DCUI::Text(WidgetTree, nullptr, TEXT(""), UDCText_Heading::StaticClass());
		Value->SetJustification(ETextJustify::Center);
		USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
		ValueBox->SetMinDesiredWidth(110.f);
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
	auto AddSlider = [this, &AddRow](UVerticalBox* Page, const TCHAR* Label, FName Id)
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

	UVerticalBox* Controls = AddPage(TEXT("CONTROLS"));
	for (int32 i = 0; i < (int32)EDCAction::Count; ++i)
	{
		UHorizontalBox* H = AddRow(Controls, UDCSettings::ActionLabel((EDCAction)i));
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, TEXT(""), FName(*FString::FromInt(i)));
		USizeBox* BtnBox = WidgetTree->ConstructWidget<USizeBox>();
		BtnBox->SetMinDesiredWidth(240.f);
		BtnBox->AddChild(Btn);
		H->AddChildToHorizontalBox(BtnBox);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCSettingsWidget::HandleRebind);
		KeyButtons.Add(Btn);
	}
	SensitivityText = AddStepper(Controls, TEXT("Mouse sensitivity"), "SensDown", "SensUp");

	UVerticalBox* Audio = AddPage(TEXT("AUDIO"));
	AddSlider(Audio, TEXT("Master"), "Master")->OnValueChanged.AddDynamic(this, &UDCSettingsWidget::HandleMasterVolume);
	AddSlider(Audio, TEXT("SFX"), "Sfx")->OnValueChanged.AddDynamic(this, &UDCSettingsWidget::HandleSfxVolume);
	AddSlider(Audio, TEXT("Music"), "Music")->OnValueChanged.AddDynamic(this, &UDCSettingsWidget::HandleMusicVolume);

	UVerticalBox* Graphics = AddPage(TEXT("GRAPHICS"));
	QualityText = AddStepper(Graphics, TEXT("Quality"), "QualDown", "QualUp");
	LumenText = AddStepper(Graphics, TEXT("Lumen (GI + reflections)"), "LumenDown", "LumenUp");
	FpsText = AddStepper(Graphics, TEXT("Frame rate cap"), "FpsDown", "FpsUp");
	VSyncText = AddStepper(Graphics, TEXT("VSync"), "VSyncDown", "VSyncUp");
	ResScaleText = AddStepper(Graphics, TEXT("Resolution scale"), "ResDown", "ResUp");
	FullscreenText = AddStepper(Graphics, TEXT("Fullscreen"), "FullDown", "FullUp");
	ShowTab(0);
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
		KeyButtons[i]->Label->SetText(i == CapturingAction ? FText::FromString(TEXT("PRESS A KEY...")) : S->Key((EDCAction)i).GetDisplayName());
	}
	const TPair<FName, float> Volumes[] = {{"Master", S->MasterVolume}, {"Sfx", S->SfxVolume}, {"Music", S->MusicVolume}};
	for (const TPair<FName, float>& Vol : Volumes)
	{
		VolumeSliders[Vol.Key]->SetValue(Vol.Value);
		VolumeTexts[Vol.Key]->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Vol.Value * 100.f))));
	}
	SensitivityText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), S->MouseSensitivityScale)));
	QualityText->SetText(FText::FromString(QualityNames[FMath::Clamp(S->Quality, 0, 3)]));
	LumenText->SetText(FText::FromString(S->bLumen ? TEXT("ON") : TEXT("OFF")));
	FpsText->SetText(FText::FromString(S->FpsCap > 0 ? FString::FromInt(S->FpsCap) : FString(TEXT("NONE"))));
	VSyncText->SetText(FText::FromString(S->bVSync ? TEXT("ON") : TEXT("OFF")));
	ResScaleText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), S->ResolutionScale)));
	FullscreenText->SetText(FText::FromString(S->bFullscreen ? TEXT("ON") : TEXT("OFF")));
}

void UDCSettingsWidget::HandleStep(FName Id)
{
	UDCSettings* S = UDCSettings::Get();
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
	DCUI::Header(WidgetTree, V, TEXT("PAUSED"));
	MainButtons = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(MainButtons);
	MenuButton(WidgetTree, MainButtons, TEXT("CONTINUE"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleContinue);
	MenuButton(WidgetTree, MainButtons, TEXT("OPTIONS"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleOptions);
	MenuButton(WidgetTree, MainButtons, TEXT("RESTART"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleRestart);
	MenuButton(WidgetTree, MainButtons, TEXT("BACK TO MAIN MENU"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleMainMenu);
	MenuButton(WidgetTree, MainButtons, TEXT("QUIT"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleQuit);
	Settings = WidgetTree->ConstructWidget<UDCSettingsWidget>();
	V->AddChild(Settings);
	OptionsBackButton = MenuButton(WidgetTree, V, TEXT("BACK"));
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
