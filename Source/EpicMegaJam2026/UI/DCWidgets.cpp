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

void UDCInventoryWidget::Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig, bool bInGrid, UAC_FilmCrafting* InCrafting)
{
	Inventory = InInventory;
	Config = InConfig;
	bGrid = bInGrid;
	Crafting = InCrafting;

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
			if (Row->Genre != EGenre::Paranormal) // DEPRECATED v1 genre: it has no tapes
			{
				AddTab(Row->DisplayName.ToString(), GenreName(Row->Genre));
			}
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

	// Detail panel: title, cover, rating, genre, skills, synopsis of the selected row
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
	DCUI::Text(WidgetTree, Facts, TEXT("Skills:"))->SetMargin(FMargin(0.f, 14.f, 0.f, 0.f));
	DetailSkills = DCUI::Text(WidgetTree, Facts, TEXT(""), UDCText_Accent::StaticClass());
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

	// Copies on offer: in the VHS player the ones not yet in a slot, on a shelf the whole stock
	auto Copies = [this](const FName& Row) { return Crafting ? Crafting->GetAvailableCount(Row) : Inventory->GetCount(Row); };
	TArray<FName> Rows = SelectedTab == AllTab ? Inventory->GetOwnedVHS()
		: Inventory->GetOwnedVHSByGenre((EGenre)StaticEnum<EGenre>()->GetValueByName(SelectedTab));
	Rows.RemoveAll([&Copies](const FName& Row) { return Copies(Row) <= 0; });
	if (!Rows.Contains(SelectedTape))
	{
		SelectedTape = Rows.IsEmpty() ? NAME_None : Rows[0];
	}
	if (Rows.IsEmpty())
	{
		DCUI::Text(WidgetTree, List, bGrid ? TEXT("No tapes left to combine.") : TEXT("No tapes on this shelf."));
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
			// Tile: cover over the title, the skills and the copies
			UImage* Cover = AddCover(WidgetTree, Btn->Content, TileCover);
			SetCover(Cover, &Tape, Config);
			Cast<UVerticalBoxSlot>(Cover->GetParent()->Slot)->SetHorizontalAlignment(HAlign_Center);
			Btn->Content->AddChildToVerticalBox(Btn->Label)->SetHorizontalAlignment(HAlign_Center);
			Btn->Label->SetWrapTextAt(TileCover.X + 16.f);
			Btn->Label->SetJustification(ETextJustify::Center);
			DCUI::SkillStrip(WidgetTree, Btn->Content, Tape.Skills);
			DCUI::Text(WidgetTree, Btn->Content, FString::Printf(TEXT("x%d"), Copies(Row)), UDCText_Accent::StaticClass());
			Cast<UUniformGridPanel>(List)->AddChildToUniformGrid(Btn, Index / GridColumns, Index % GridColumns);
		}
		else
		{
			// Row: thumbnail, title with the skills under it, copies, stars
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			Btn->Content->AddChild(H);
			SetCover(AddCover(WidgetTree, H, FVector2D(34.f, 48.f)), &Tape, Config);
			UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
			Text->AddChildToVerticalBox(Btn->Label);
			DCUI::SkillStrip(WidgetTree, Text, Tape.Skills);
			UHorizontalBoxSlot* TitleSlot = H->AddChildToHorizontalBox(Text);
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TitleSlot->SetVerticalAlignment(VAlign_Center);
			TitleSlot->SetPadding(FMargin(12.f, 0.f));
			H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("x%d  "), Copies(Row)), UDCText_Accent::StaticClass()))->SetVerticalAlignment(VAlign_Center);
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
	const FGenreData* Genre = FindGenre(Config, Tape.PrimaryGenre);
	DetailGenre->SetText(Genre ? Genre->DisplayName : FText::GetEmpty());
	DetailSkills->SetText(FText::FromString(DCUI::SkillLines(Tape.Skills)));
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
	DCUI::Header(WidgetTree, V, TEXT("COMBINE VHS"), TEXT("Mix tapes: their skills add up"));
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChild(Body);

	// Left: the tapes with copies left as cover tiles; a click fills the next empty slot
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(12.f))->SetContent(Left);
	DCUI::Text(WidgetTree, Left, TEXT("VHS INVENTORY"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 10.f));
	Picker = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	Left->AddChild(Picker);
	Picker->Setup(Inventory, Config, true, Crafting);
	Picker->OnVHSPicked.AddDynamic(this, &UDCFilmCraftingWidget::OnPicked);
	Inventory->OnInventoryChanged.AddDynamic(Picker, &UDCInventoryWidget::Refresh);
	Crafting->OnFilmStateChanged.AddDynamic(this, &UDCFilmCraftingWidget::OnFilmStateChanged);

	// Right: the slots (MaxSlots built, the locked ones collapsed), the live skill sum, the film title and the combine button
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	UCommonBorder* RightPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(16.f));
	RightPanel->SetContent(Right);
	Cast<UHorizontalBoxSlot>(RightPanel->Slot)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	SlotsHeader = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Heading::StaticClass());
	SlotsHeader->SetMargin(FMargin(0.f, 0.f, 0.f, 12.f));
	UUniformGridPanel* Slots = WidgetTree->ConstructWidget<UUniformGridPanel>();
	Slots->SetSlotPadding(FMargin(0.f, 0.f, 12.f, 8.f));
	Right->AddChild(Slots);
	constexpr int32 SlotColumnsPerRow = 3;
	for (int32 i = 0; i < FMath::Max(Config->MaxSlots, 1); ++i)
	{
		const FName SlotName(*FString::FromInt(i));
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
		Slots->AddChildToUniformGrid(Column, i / SlotColumnsPerRow, i % SlotColumnsPerRow);
		SlotColumns.Add(Column);
		UOverlay* CoverStack = WidgetTree->ConstructWidget<UOverlay>();
		Column->AddChild(CoverStack);
		SlotCovers.Add(AddCover(WidgetTree, CoverStack, TileCover));
		UDCButton* Clear = DCUI::Button(WidgetTree, CoverStack, TEXT("x"), SlotName, UDCButton_Tab::StaticClass());
		UOverlaySlot* ClearSlot = Cast<UOverlaySlot>(Clear->Slot);
		ClearSlot->SetHorizontalAlignment(HAlign_Right);
		ClearSlot->SetVerticalAlignment(VAlign_Top);
		ClearSlot->SetPadding(FMargin(4.f));
		Clear->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnClearClicked);
		SlotClears.Add(Clear);
		DCUI::Text(WidgetTree, Column, FString::Printf(TEXT("SLOT %d"), i + 1), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 8.f, 0.f, 2.f));
		UCommonTextBlock* Title = DCUI::Text(WidgetTree, Column, TEXT(""));
		Title->SetWrapTextAt(TileCover.X);
		SlotTexts.Add(Title);
	}

	DCUI::Text(WidgetTree, Right, TEXT("FILM PROFILE"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 8.f, 0.f, 4.f));
	ProfileText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());

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
	Picker->Refresh(); // copies already in a slot leave the list
	for (int32 i = 0; i < SlotColumns.Num(); ++i)
	{
		SlotColumns[i]->SetVisibility(i < Crafting->UnlockedSlots ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		const FName Row = Crafting->SlotRows.IsValidIndex(i) ? Crafting->SlotRows[i] : NAME_None;
		FVHSData Tape;
		const bool bHas = !Row.IsNone() && Inventory->GetVHSData(Row, Tape);
		SlotTexts[i]->SetText(bHas ? Tape.Title : FText::FromString(TEXT("(empty)")));
		SetCover(SlotCovers[i], bHas ? &Tape : nullptr, Config);
		SlotClears[i]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	const int32 Filled = Crafting->GetFilledSlots();
	SlotsHeader->SetText(FText::FromString(FString::Printf(TEXT("SELECTED VHS (%d/%d)"), Filled, Crafting->UnlockedSlots)));
	ProfileText->SetText(FText::FromString(DCUI::SkillLines(Crafting->GetProfile(), Config->SkillCap)));
	TitleBox->SetIsEnabled(Filled >= 1); // title only editable once a tape is in
	ConfirmButton->SetIsEnabled(Crafting->CanConfirm());
	DiscardButton->SetIsEnabled(Crafting->HeldFilms.Num() > 0);
	HeldText->SetText(Crafting->HeldFilms.Num() > 0
		? FText::FromString(FString::Printf(TEXT("In hand: %s"), *Crafting->HeldFilms[0].Title.ToString()))
		: FText::GetEmpty());
}

void UDCFilmCraftingWidget::OnPicked(FName VHSRow)
{
	for (int32 i = 0; i < Crafting->UnlockedSlots; ++i)
	{
		if (Crafting->SlotRows[i].IsNone())
		{
			const bool bOk = Crafting->SetSlot(i, VHSRow);
			StatusText->SetText(bOk ? FText::GetEmpty() : FText::FromString(TEXT("No copies of that tape left")));
			return;
		}
	}
	StatusText->SetText(FText::FromString(TEXT("All slots are full: remove a tape first")));
}

void UDCFilmCraftingWidget::OnClearClicked(FName SlotName)
{
	Crafting->ClearSlot(FCString::Atoi(*SlotName.ToString()));
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
	const FClientRuntime& Rt = Front->Runtime;
	OrderText->SetText(Rt.DisplayName.IsEmpty() ? Rt.OrderText : FText::Format(INVTEXT("{0}: {1}"), Rt.DisplayName, Rt.OrderText));
	PatienceBar->SetPercent(Front->PatienceMax > 0.f ? Front->Runtime.PatienceLeft / Front->PatienceMax : 0.f);
}
