#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Data/PDA_GameConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/CommonUIInputTypes.h"

namespace
{
	const FName AllTab = "All";
	constexpr int32 GridColumns = 4;
	const FVector2D TileCover(110.f, 156.f);

	FName GenreName(EGenre G) { return FName(*StaticEnum<EGenre>()->GetNameStringByValue((int64)G)); }

	const FGenreData* FindGenre(const UPDA_GameConfig* Config, EGenre Genre)
	{
		if (Config && Config->GenreTable)
		{
			for (const TPair<FName, uint8*>& Pair : Config->GenreTable->GetRowMap())
			{
				const FGenreData* Row = reinterpret_cast<const FGenreData*>(Pair.Value);
				if (Row->Genre == Genre)
				{
					return Row;
				}
			}
		}
		return nullptr;
	}

	// Cover art; the genre's color until the tape has one, dark when there's no tape
	void SetCover(UImage* Img, const FVHSData* Tape, const UPDA_GameConfig* Config)
	{
		if (Tape && !Tape->Cover.IsNull())
		{
			Img->SetColorAndOpacity(FLinearColor::White);
			Img->SetBrushFromSoftTexture(Tape->Cover);
			return;
		}
		const FGenreData* Genre = Tape ? FindGenre(Config, Tape->PrimaryGenre) : nullptr;
		Img->SetBrush(FSlateBrush());
		Img->SetColorAndOpacity(Genre ? Genre->Color : DCPalette::NavyLight);
	}

	// SetIsSelected(false) only deselects toggleable buttons
	void SetSelected(UDCButton* Btn, bool bSelected)
	{
		bSelected ? Btn->SetIsSelected(true, false) : Btn->ClearSelection();
	}

	UImage* AddCover(UWidgetTree* Tree, UPanelWidget* Parent, FVector2D Size)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(Size.X);
		Box->SetHeightOverride(Size.Y);
		UImage* Img = Tree->ConstructWidget<UImage>();
		Box->AddChild(Img);
		Parent->AddChild(Box);
		return Img;
	}
}

// ---------------------------------------------------------------- Inventory widget

void UDCInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("PrevTab"), false, FSimpleDelegate::CreateUObject(this, &UDCInventoryWidget::CycleTab, -1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("NextTab"), true, FSimpleDelegate::CreateUObject(this, &UDCInventoryWidget::CycleTab, 1)));
}

void UDCInventoryWidget::Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig, bool bInGrid)
{
	Inventory = InInventory;
	Config = InConfig;
	bGrid = bInGrid;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;
	Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	auto AddTab = [this](const FString& Label, FName Id)
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, Tabs, Label.ToUpper(), Id, UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTabClicked);
		Cast<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
	};
	AddTab(TEXT("All"), AllTab);
	if (Config && Config->GenreTable)
	{
		for (const TPair<FName, uint8*>& Pair : Config->GenreTable->GetRowMap())
		{
			const FGenreData* Row = reinterpret_cast<const FGenreData*>(Pair.Value);
			AddTab(Row->DisplayName.ToString(), GenreName(Row->Genre));
		}
	}

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChild(Body);
	UCommonBorder* ListPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Inset::StaticClass(), FMargin(10.f));
	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
	ListSize->SetWidthOverride(bGrid ? GridColumns * (TileCover.X + 34.f) : 520.f);
	ListSize->SetHeightOverride(440.f);
	ListPanel->SetContent(ListSize);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	ListSize->AddChild(Scroll);
	if (bGrid)
	{
		UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
		Grid->SetSlotPadding(FMargin(4.f));
		List = Grid;
	}
	else
	{
		List = WidgetTree->ConstructWidget<UVerticalBox>();
	}
	Scroll->AddChild(List);
	if (bGrid)
	{
		Refresh();
		return;
	}

	// Detail panel: title, cover, rating, genres, synopsis of the selected row
	UCommonBorder* DetailPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Inset::StaticClass(), FMargin(16.f));
	Cast<UHorizontalBoxSlot>(DetailPanel->Slot)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	USizeBox* DetailSize = WidgetTree->ConstructWidget<USizeBox>();
	DetailSize->SetWidthOverride(460.f);
	DetailSize->SetHeightOverride(440.f);
	DetailPanel->SetContent(DetailSize);
	UVerticalBox* D = WidgetTree->ConstructWidget<UVerticalBox>();
	DetailSize->AddChild(D);
	Detail = D;
	DetailTitle = DCUI::Text(WidgetTree, D, TEXT(""), UDCText_Title::StaticClass());
	DetailTitle->SetWrapTextAt(428.f);
	UHorizontalBox* Info = WidgetTree->ConstructWidget<UHorizontalBox>();
	D->AddChildToVerticalBox(Info)->SetPadding(FMargin(0.f, 14.f));
	DetailCover = AddCover(WidgetTree, Info, FVector2D(150.f, 214.f));
	UVerticalBox* Facts = WidgetTree->ConstructWidget<UVerticalBox>();
	Info->AddChildToHorizontalBox(Facts)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	DCUI::Text(WidgetTree, Facts, TEXT("Rating:"));
	DetailStars = DCUI::Text(WidgetTree, Facts, TEXT(""), UDCText_Stars::StaticClass());
	DCUI::Text(WidgetTree, Facts, TEXT("Genre:"))->SetMargin(FMargin(0.f, 14.f, 0.f, 0.f));
	DetailGenre = DCUI::Text(WidgetTree, Facts, TEXT(""), UDCText_Accent::StaticClass());
	DetailGenre->SetWrapTextAt(250.f);
	DetailSynopsis = DCUI::Text(WidgetTree, D, TEXT(""));
	DetailSynopsis->SetWrapTextAt(428.f);
	Refresh();
}

void UDCInventoryWidget::SetGenre(EGenre Genre)
{
	SelectedTab = GenreName(Genre);
	Refresh();
}

void UDCInventoryWidget::CycleTab(int32 Dir)
{
	const int32 Num = Tabs ? Tabs->GetChildrenCount() : 0;
	if (Num == 0)
	{
		return;
	}
	int32 Index = 0;
	for (int32 i = 0; i < Num; ++i)
	{
		if (Cast<UDCButton>(Tabs->GetChildAt(i))->Payload == SelectedTab)
		{
			Index = i;
		}
	}
	SelectedTab = Cast<UDCButton>(Tabs->GetChildAt((Index + Dir + Num) % Num))->Payload;
	Refresh();
}

void UDCInventoryWidget::Refresh()
{
	if (!Inventory || !List)
	{
		return;
	}
	for (UWidget* W : Tabs->GetAllChildren())
	{
		UDCButton* Tab = Cast<UDCButton>(W);
		SetSelected(Tab, Tab->Payload == SelectedTab);
	}
	List->ClearChildren();

	TArray<FName> Rows = SelectedTab == AllTab ? Inventory->OwnedVHS
		: Inventory->GetOwnedVHSByGenre((EGenre)StaticEnum<EGenre>()->GetValueByName(SelectedTab));
	Rows.RemoveAll([this](const FName& Row) { return Inventory->IsCarried(Row) != bGrid; }); // grid (VHS player): carried tapes only; shelf: the ones left on it
	if (!Rows.Contains(SelectedTape))
	{
		SelectedTape = Rows.IsEmpty() ? NAME_None : Rows[0];
	}
	if (Rows.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, bGrid ? TEXT("Take tapes from the shelves first.") : TEXT("No tapes left on this shelf."));
	}

	int32 Index = 0;
	for (const FName& Row : Rows)
	{
		FVHSData Tape;
		if (!Inventory->GetVHSData(Row, Tape))
		{
			continue;
		}
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, Tape.Title.ToString(), Row, UDCButton_Row::StaticClass());
		Btn->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTapeClicked);
		Btn->Label->RemoveFromParent();
		if (bGrid)
		{
			// Tile: cover over the title
			UImage* Cover = AddCover(WidgetTree, Btn->Content, TileCover);
			SetCover(Cover, &Tape, Config);
			Cast<UVerticalBoxSlot>(Cover->GetParent()->Slot)->SetHorizontalAlignment(HAlign_Center);
			Btn->Content->AddChildToVerticalBox(Btn->Label)->SetHorizontalAlignment(HAlign_Center);
			Btn->Label->SetWrapTextAt(TileCover.X + 16.f);
			Btn->Label->SetJustification(ETextJustify::Center);
			Cast<UUniformGridPanel>(List)->AddChildToUniformGrid(Btn, Index / GridColumns, Index % GridColumns);
		}
		else
		{
			// Row: thumbnail, title, stars
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			Btn->Content->AddChild(H);
			SetCover(AddCover(WidgetTree, H, FVector2D(34.f, 48.f)), &Tape, Config);
			UHorizontalBoxSlot* TitleSlot = H->AddChildToHorizontalBox(Btn->Label);
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TitleSlot->SetVerticalAlignment(VAlign_Center);
			TitleSlot->SetPadding(FMargin(12.f, 0.f));
			H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, DCUI::Stars(Tape.Rating), UDCText_Stars::StaticClass()))->SetVerticalAlignment(VAlign_Center);
			Btn->SetIsSelectable(true);
			Btn->SetIsSelected(Row == SelectedTape, false);
			List->AddChild(Btn);
		}
		++Index;
	}
	ShowDetail(SelectedTape);
}

void UDCInventoryWidget::OnTapeClicked(FName Row)
{
	if (!bGrid)
	{
		SelectedTape = Row;
		for (UWidget* W : List->GetAllChildren())
		{
			if (UDCButton* Btn = Cast<UDCButton>(W))
			{
				SetSelected(Btn, Btn->Payload == Row);
			}
		}
		ShowDetail(Row);
	}
	OnVHSPicked.Broadcast(Row);
}

void UDCInventoryWidget::ShowDetail(FName Row)
{
	FVHSData Tape;
	if (!Detail)
	{
		return; // grid layout has no detail panel
	}
	const bool bHas = !Row.IsNone() && Inventory->GetVHSData(Row, Tape);
	Detail->SetVisibility(bHas ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	if (!bHas)
	{
		return;
	}
	DetailTitle->SetText(Tape.Title);
	DetailStars->SetText(FText::FromString(DCUI::Stars(Tape.Rating)));
	TArray<FString> Names;
	for (const FGenreValue& Value : Tape.GenreValues)
	{
		if (const FGenreData* Genre = FindGenre(Config, Value.Genre))
		{
			Names.Add(Genre->DisplayName.ToString());
		}
	}
	DetailGenre->SetText(FText::FromString(FString::Join(Names, TEXT(" / "))));
	DetailSynopsis->SetText(Tape.Synopsis);
	SetCover(DetailCover, &Tape, Config);
}

// ---------------------------------------------------------------- Film crafting ("Combine VHS")

void UDCFilmCraftingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Inventory = GM->Inventory;
	Crafting = GM->FilmCrafting;
	Config = GM->Config;

	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("COMBINE VHS"), TEXT("Pick 3 tapes to combine"));
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChild(Body);

	// Left: the owned tapes as cover tiles; a click fills the next empty slot
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(12.f))->SetContent(Left);
	DCUI::Text(WidgetTree, Left, TEXT("VHS INVENTORY"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 10.f));
	Picker = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	Left->AddChild(Picker);
	Picker->Setup(Inventory, Config, true);
	Picker->OnVHSPicked.AddDynamic(this, &UDCFilmCraftingWidget::OnPicked);
	Inventory->OnInventoryChanged.AddDynamic(Picker, &UDCInventoryWidget::Refresh);
	Crafting->OnFilmStateChanged.AddDynamic(this, &UDCFilmCraftingWidget::OnFilmStateChanged);

	// Right: the three slots, the film title and the combine button
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	UCommonBorder* RightPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(16.f));
	RightPanel->SetContent(Right);
	Cast<UHorizontalBoxSlot>(RightPanel->Slot)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	SlotsHeader = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Heading::StaticClass());
	SlotsHeader->SetMargin(FMargin(0.f, 0.f, 0.f, 12.f));
	UHorizontalBox* Slots = WidgetTree->ConstructWidget<UHorizontalBox>();
	Right->AddChild(Slots);
	for (EFilmSlot S : {EFilmSlot::Intro, EFilmSlot::Middle, EFilmSlot::Ending})
	{
		const FName SlotName(*StaticEnum<EFilmSlot>()->GetNameStringByValue((int64)S));
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Slots->AddChildToHorizontalBox(Column)->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
		UOverlay* CoverStack = WidgetTree->ConstructWidget<UOverlay>();
		Column->AddChild(CoverStack);
		SlotCovers.Add(S, AddCover(WidgetTree, CoverStack, TileCover));
		UDCButton* Clear = DCUI::Button(WidgetTree, CoverStack, TEXT("×"), SlotName, UDCButton_Tab::StaticClass());
		UOverlaySlot* ClearSlot = Cast<UOverlaySlot>(Clear->Slot);
		ClearSlot->SetHorizontalAlignment(HAlign_Right);
		ClearSlot->SetVerticalAlignment(VAlign_Top);
		ClearSlot->SetPadding(FMargin(4.f));
		Clear->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnClearClicked);
		SlotClears.Add(S, Clear);
		DCUI::Text(WidgetTree, Column, SlotName.ToString().ToUpper(), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 8.f, 0.f, 2.f));
		UCommonTextBlock* Title = DCUI::Text(WidgetTree, Column, TEXT(""));
		Title->SetWrapTextAt(TileCover.X);
		SlotTexts.Add(S, Title);
	}

	TitleBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	TitleBox->SetHintText(FText::FromString(TEXT("Film title")));
	FEditableTextBoxStyle FieldStyle = TitleBox->GetWidgetStyle();
	const FSlateBrush Field = DCUI::Box(DCPalette::Navy, DCPalette::Blue);
	const FSlateBrush FieldActive = DCUI::Box(DCPalette::Navy, DCPalette::Lime);
	FieldStyle.SetBackgroundImageNormal(Field).SetBackgroundImageReadOnly(Field).SetBackgroundImageHovered(FieldActive).SetBackgroundImageFocused(FieldActive)
		.SetFont(GetDefault<UDCText_Body>()->Font).SetForegroundColor(DCPalette::Beige).SetFocusedForegroundColor(DCPalette::Beige)
		.SetReadOnlyForegroundColor(DCPalette::Grey.CopyWithNewOpacity(0.5f)).SetPadding(FMargin(10.f, 6.f));
	TitleBox->SetWidgetStyle(FieldStyle);
	Right->AddChildToVerticalBox(TitleBox)->SetPadding(FMargin(0.f, 16.f, 0.f, 10.f));

	ConfirmButton = DCUI::Button(WidgetTree, Right, TEXT("COMBINE"));
	ConfirmButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnConfirm);
	DiscardButton = DCUI::Button(WidgetTree, Right, TEXT("Discard held film"), NAME_None, UDCButton_Tab::StaticClass());
	DiscardButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnDiscard);
	Cast<UVerticalBoxSlot>(DiscardButton->Slot)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	HeldText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());
	StatusText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());
	StatusText->SetWrapTextAt(3.f * (TileCover.X + 12.f));
}

void UDCFilmCraftingWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	StatusText->SetText(FText::GetEmpty());
	Refresh();
}

void UDCFilmCraftingWidget::Refresh()
{
	if (!Crafting || !Inventory)
	{
		return;
	}
	int32 Filled = 0;
	for (const TPair<EFilmSlot, TObjectPtr<UCommonTextBlock>>& Pair : SlotTexts)
	{
		const FName* Row = Crafting->Slots.Find(Pair.Key);
		FVHSData Tape;
		const bool bHas = Row && Inventory->GetVHSData(*Row, Tape);
		Filled += bHas ? 1 : 0;
		Pair.Value->SetText(bHas ? Tape.Title : FText::FromString(TEXT("(empty)")));
		SetCover(SlotCovers[Pair.Key], bHas ? &Tape : nullptr, Config);
		SlotClears[Pair.Key]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	SlotsHeader->SetText(FText::FromString(FString::Printf(TEXT("SELECTED VHS (%d/3)"), Filled)));
	TitleBox->SetIsEnabled(Crafting->AreSlotsFull()); // title only editable with all 3 slots filled
	ConfirmButton->SetIsEnabled(Crafting->CanConfirm());
	DiscardButton->SetIsEnabled(Crafting->HeldFilms.Num() > 0);
	HeldText->SetText(Crafting->HeldFilms.Num() > 0
		? FText::FromString(FString::Printf(TEXT("In hand: %s"), *Crafting->HeldFilms[0].Title.ToString()))
		: FText::GetEmpty());
}

void UDCFilmCraftingWidget::OnPicked(FName VHSRow)
{
	for (EFilmSlot S : {EFilmSlot::Intro, EFilmSlot::Middle, EFilmSlot::Ending})
	{
		const FName* Row = Crafting->Slots.Find(S);
		if (Row && !Row->IsNone())
		{
			continue;
		}
		const bool bOk = Crafting->SetSlot(S, VHSRow);
		StatusText->SetText(bOk ? FText::GetEmpty() : FText::FromString(TEXT("That tape is already in another slot")));
		return;
	}
	StatusText->SetText(FText::FromString(TEXT("All 3 slots are full: remove a tape first")));
}

void UDCFilmCraftingWidget::OnClearClicked(FName SlotName)
{
	Crafting->ClearSlot((EFilmSlot)StaticEnum<EFilmSlot>()->GetValueByName(SlotName));
}

void UDCFilmCraftingWidget::OnConfirm(FName Unused)
{
	if (Crafting->ConfirmFilm(TitleBox->GetText()))
	{
		TitleBox->SetText(FText::GetEmpty());
	}
}

void UDCFilmCraftingWidget::OnDiscard(FName Unused)
{
	Crafting->DiscardHeldFilm();
}

// ---------------------------------------------------------------- Dialogue widget

void UDCDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Above the tutorial sign (bottom center, -140)
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -210.f));
	DCUI::Text(WidgetTree, V, TEXT("CUSTOMER"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	LineText = DCUI::Text(WidgetTree, V, TEXT(""));
	LineText->SetWrapTextAt(640.f);
	Button = DCUI::Button(WidgetTree, V, TEXT(""));
	Button->OnPayloadClicked.AddDynamic(this, &UDCDialogueWidget::HandleButton);
	UVerticalBoxSlot* ButtonSlot = Cast<UVerticalBoxSlot>(Button->Slot);
	ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
}

void UDCDialogueWidget::Show(const FText& Line, const FText& ButtonLabel)
{
	LineText->SetText(Line);
	Button->Label->SetText(ButtonLabel);
}

// ---------------------------------------------------------------- Order panel

void UDCOrderPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCommonBorder* Panel = nullptr;
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.f, 0.f), FVector2D::ZeroVector, FVector2D(24.f, 24.f), &Panel);
	PanelRoot = Panel;
	DCUI::Text(WidgetTree, V, TEXT("ORDER"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 6.f));
	OrderText = DCUI::Text(WidgetTree, V, TEXT(""));
	OrderText->SetWrapTextAt(360.f);
	PatienceBar = DCUI::Bar(WidgetTree, V, 360.f);
	PanelRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCOrderPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ADCClient* Front = Director ? Director->GetFrontClient() : nullptr;
	if (!Front || !Front->IsAtFront())
	{
		PanelRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	PanelRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	OrderText->SetText(Front->Runtime.OrderText);
	PatienceBar->SetPercent(Front->PatienceMax > 0.f ? Front->Runtime.PatienceLeft / Front->PatienceMax : 0.f);
}
