#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/BPFL_Scoring.h"
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
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Input/CommonUIInputTypes.h"

namespace
{
	const FName AllTab = "All";
	constexpr float ListWidth = 560.f, PickerWidth = 740.f, DetailWidth = 460.f;

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
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("NextTab"), false, FSimpleDelegate::CreateUObject(this, &UDCInventoryWidget::CycleTab, 1)));
}

void UDCInventoryWidget::Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig, bool bInPicker, UAC_FilmCrafting* InCrafting)
{
	Inventory = InInventory;
	Config = InConfig;
	bPicker = bInPicker;
	Crafting = InCrafting;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;
	// Eleven tabs don't fit one row: they wrap at the width of the list (plus the detail panel on shelves)
	const float ListW = bPicker ? PickerWidth : ListWidth;
	Tabs = WidgetTree->ConstructWidget<UWrapBox>();
	Tabs->SetInnerSlotPadding(FVector2D(4.f, 4.f));
	Tabs->SetExplicitWrapSize(true);
	Tabs->SetWrapSize(bPicker ? ListW + 20.f : ListW + DetailWidth + 64.f);
	Root->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	auto AddTab = [this](const FString& Label, FName Id)
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, Tabs, Label.ToUpper(), Id, UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTabClicked);
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
	ListSize->SetWidthOverride(ListW);
	ListSize->SetHeightOverride(440.f);
	ListPanel->SetContent(ListSize);
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	ListSize->AddChild(Scroll);
	List = WidgetTree->ConstructWidget<UVerticalBox>();
	Scroll->AddChild(List);
	if (bPicker)
	{
		Refresh();
		return;
	}

	// Detail panel: title, cover, rating, genre, skills, synopsis of the selected row
	UCommonBorder* DetailPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Inset::StaticClass(), FMargin(16.f));
	Cast<UHorizontalBoxSlot>(DetailPanel->Slot)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	USizeBox* DetailSize = WidgetTree->ConstructWidget<USizeBox>();
	DetailSize->SetWidthOverride(DetailWidth);
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
	DetailSkills = WidgetTree->ConstructWidget<UVerticalBox>();
	Facts->AddChild(DetailSkills);
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
	ADCAudioManager::PlayCueStatic(this, TEXT("TabSwitch"));
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
		DCUI::Text(WidgetTree, List, bPicker ? TEXT("No tapes left to combine.") : TEXT("No tapes on this shelf."));
	}

	for (const FName& Row : Rows)
	{
		FVHSData Tape;
		if (!Inventory->GetVHSData(Row, Tape))
		{
			continue;
		}
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, DCUI::CleanTitle(Tape.Title), Row, UDCButton_Row::StaticClass());
			DCUI::SkillTooltip(WidgetTree, Btn, Tape.Skills);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTapeClicked);
		// Row: thumbnail, title with the skills under it, copies, stars
		Btn->Label->RemoveFromParent();
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Btn->Content->AddChild(H);
		SetCover(AddCover(WidgetTree, H, FVector2D(34.f, 48.f)), &Tape, Config);
		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
		Text->AddChildToVerticalBox(Btn->Label)->SetHorizontalAlignment(HAlign_Left);
		DCUI::InlineSkills(WidgetTree, Text, Tape.Skills);
		UHorizontalBoxSlot* TitleSlot = H->AddChildToHorizontalBox(Text);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetPadding(FMargin(12.f, 0.f));
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("x%d  "), Copies(Row)), UDCText_Accent::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, DCUI::Stars(Tape.Rating), UDCText_Stars::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		// A selected button ignores clicks: the picker needs repeat clicks (one copy per click), so only shelf rows select
		Btn->SetIsSelectable(!bPicker);
		Btn->SetIsSelected(!bPicker && Row == SelectedTape, false);
		List->AddChild(Btn);
	}
	ShowDetail(SelectedTape);
}

void UDCInventoryWidget::OnTapeClicked(FName Row)
{
	if (!bPicker)
	{
		ADCAudioManager::PlayCueStatic(this, TEXT("FilmSelect"));
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
		return; // the picker has no detail panel
	}
	const bool bHas = !Row.IsNone() && Inventory->GetVHSData(Row, Tape);
	Detail->SetVisibility(bHas ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	if (!bHas)
	{
		return;
	}
	DetailTitle->SetText(FText::FromString(DCUI::CleanTitle(Tape.Title)));
	DetailStars->SetText(FText::FromString(DCUI::Stars(Tape.Rating)));
	const FGenreData* Genre = FindGenre(Config, Tape.PrimaryGenre);
	DetailGenre->SetText(Genre ? Genre->DisplayName : FText::GetEmpty());
	DetailSkills->ClearChildren();
	DCUI::SkillList(WidgetTree, DetailSkills, Tape.Skills);
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
	Director = GM->ClientDirector;

	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("COMBINE VHS"), TEXT("Mix tapes: their skills add up"));
	// Two views of the same screen: mixing tapes, and the permanent library of films already made
	UHorizontalBox* ViewTabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChildToVerticalBox(ViewTabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
	for (const TPair<const TCHAR*, const TCHAR*>& Def : { TPair<const TCHAR*, const TCHAR*>(TEXT("Combine"), TEXT("COMBINE")), TPair<const TCHAR*, const TCHAR*>(TEXT("Crafted"), TEXT("CRAFTED FILMS")) })
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, ViewTabs, Def.Value, Def.Key, UDCButton_Tab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnViewClicked);
		ViewTabButtons.Add(Tab);
	}
	// Both views share one overlay: the hidden one keeps its layout space, so the library is exactly as big as the combine view
	UOverlay* Views = WidgetTree->ConstructWidget<UOverlay>();
	V->AddChild(Views);
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	Views->AddChild(Body);
	CombineBody = Body;

	// Library view: list of crafted films on the left, the hovered one's stars and stats on the right; a click puts it in hand
	UHorizontalBox* Library = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* LibrarySlot = Views->AddChildToOverlay(Library);
	LibrarySlot->SetHorizontalAlignment(HAlign_Fill);
	LibrarySlot->SetVerticalAlignment(VAlign_Fill);
	CraftedBody = Library;
	UCommonBorder* CraftedPanel = DCUI::Border(WidgetTree, Library, UDCBorder_Inset::StaticClass(), FMargin(10.f));
	Cast<UHorizontalBoxSlot>(CraftedPanel->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UScrollBox* CraftedScroll = WidgetTree->ConstructWidget<UScrollBox>();
	CraftedPanel->SetContent(CraftedScroll);
	CraftedList = WidgetTree->ConstructWidget<UVerticalBox>();
	CraftedScroll->AddChild(CraftedList);
	UCommonBorder* CraftedDetailPanel = DCUI::Border(WidgetTree, Library, UDCBorder_Inset::StaticClass(), FMargin(16.f));
	Cast<UHorizontalBoxSlot>(CraftedDetailPanel->Slot)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
	USizeBox* CraftedDetailSize = WidgetTree->ConstructWidget<USizeBox>();
	CraftedDetailSize->SetWidthOverride(DetailWidth);
	CraftedDetailPanel->SetContent(CraftedDetailSize);
	UVerticalBox* CraftedDetail = WidgetTree->ConstructWidget<UVerticalBox>();
	CraftedDetailSize->AddChild(CraftedDetail);
	CraftedTitle = DCUI::Text(WidgetTree, CraftedDetail, TEXT(""), UDCText_Heading::StaticClass());
	CraftedTitle->SetWrapTextAt(DetailWidth - 40.f);
	CraftedStars = DCUI::Text(WidgetTree, CraftedDetail, TEXT(""), UDCText_Stars::StaticClass());
	CraftedStars->SetMargin(FMargin(0.f, 6.f, 0.f, 10.f));
	DCUI::Text(WidgetTree, CraftedDetail, TEXT("SKILLS"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
	CraftedSkills = WidgetTree->ConstructWidget<UVerticalBox>();
	CraftedDetail->AddChild(CraftedSkills);
	DCUI::Text(WidgetTree, CraftedDetail, TEXT("MADE FROM"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 10.f, 0.f, 4.f));
	CraftedTapes = DCUI::Text(WidgetTree, CraftedDetail, TEXT(""), UDCText_Body::StaticClass());
	CraftedTapes->SetWrapTextAt(DetailWidth - 40.f);
	UCommonTextBlock* CraftedHint = DCUI::Text(WidgetTree, CraftedDetail, TEXT("Click a film to take it to the counter"), UDCText_Accent::StaticClass());
	CraftedHint->SetMargin(FMargin(0.f, 10.f, 0.f, 0.f));
	CraftedHint->SetWrapTextAt(DetailWidth - 40.f);

	// Left: the tapes with copies left; a click fills the next empty slot
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(12.f))->SetContent(Left);
	DCUI::Text(WidgetTree, Left, TEXT("VHS INVENTORY"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 10.f));
	Picker = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	Left->AddChild(Picker);
	Picker->Setup(Inventory, Config, true, Crafting);
	// The floating order panel is hidden while this screen is up (ADCPlayerController::HandleScreenChanged): the order is read here, under the list
	UCommonBorder* OrderBox = DCUI::Border(WidgetTree, Left, UDCBorder_Inset::StaticClass(), FMargin(12.f, 8.f));
	Cast<UVerticalBoxSlot>(OrderBox->Slot)->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
	UVerticalBox* OrderV = WidgetTree->ConstructWidget<UVerticalBox>();
	OrderBox->SetContent(OrderV);
	DCUI::Text(WidgetTree, OrderV, TEXT("ORDER"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
	OrderText = WidgetTree->ConstructWidget<UVerticalBox>();
	OrderV->AddChild(OrderText);
	OrderPatience = DCUI::Bar(WidgetTree, OrderV, PickerWidth - 24.f);
	Picker->OnVHSPicked.AddDynamic(this, &UDCFilmCraftingWidget::OnPicked);
	Inventory->OnInventoryChanged.AddDynamic(Picker, &UDCInventoryWidget::Refresh);
	Crafting->OnFilmStateChanged.AddDynamic(this, &UDCFilmCraftingWidget::OnFilmStateChanged);

	// Middle: one row per slot (MaxSlots built, the locked ones collapsed). Wide and short: three columns instead of two tall ones
	UVerticalBox* Mid = WidgetTree->ConstructWidget<UVerticalBox>();
	UCommonBorder* MidPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(16.f));
	MidPanel->SetContent(Mid);
	Cast<UHorizontalBoxSlot>(MidPanel->Slot)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	SlotsHeader = DCUI::Text(WidgetTree, Mid, TEXT(""), UDCText_Heading::StaticClass());
	SlotsHeader->SetMargin(FMargin(0.f, 0.f, 0.f, 12.f));
	for (int32 i = 0; i < FMath::Max(Config->MaxSlots, 1); ++i)
	{
		const FName SlotName(*FString::FromInt(i));
		UCommonBorder* Box = DCUI::Border(WidgetTree, Mid, UDCBorder_Inset::StaticClass(), FMargin(12.f, 8.f));
		Cast<UVerticalBoxSlot>(Box->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		SlotBoxes.Add(Box);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Box->SetContent(H);
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("%d"), i + 1), UDCText_Heading::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Title = DCUI::Text(WidgetTree, nullptr, TEXT(""));
		Title->SetWrapTextAt(360.f);
		UHorizontalBoxSlot* TitleSlot = H->AddChildToHorizontalBox(Title);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetPadding(FMargin(14.f, 0.f));
		SlotTexts.Add(Title);
		UDCButton* Clear = DCUI::Button(WidgetTree, H, TEXT("x"), SlotName, UDCButton_Tab::StaticClass());
		Cast<UHorizontalBoxSlot>(Clear->Slot)->SetVerticalAlignment(VAlign_Center);
		Clear->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnClearClicked);
		SlotClears.Add(Clear);
	}

	// Next slot to buy: padlock (shackle over body) + "SLOT N  LOCKED"
	UCommonBorder* Locked = DCUI::Border(WidgetTree, Mid, UDCBorder_Inset::StaticClass(), FMargin(12.f, 8.f));
	Cast<UVerticalBoxSlot>(Locked->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	LockedBox = Locked;
	UHorizontalBox* LockRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Locked->SetContent(LockRow);
	UVerticalBox* Padlock = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* Shackle = WidgetTree->ConstructWidget<USizeBox>();
	Shackle->SetWidthOverride(14.f);
	Shackle->SetHeightOverride(12.f);
	Shackle->AddChild(DCUI::Border(WidgetTree, nullptr, UDCBorder_LockShackle::StaticClass(), FMargin(0.f)));
	Padlock->AddChildToVerticalBox(Shackle)->SetHorizontalAlignment(HAlign_Center);
	USizeBox* LockBody = WidgetTree->ConstructWidget<USizeBox>();
	LockBody->SetWidthOverride(24.f);
	LockBody->SetHeightOverride(16.f);
	LockBody->AddChild(DCUI::Border(WidgetTree, nullptr, UDCBorder_LockBody::StaticClass(), FMargin(0.f)));
	Padlock->AddChildToVerticalBox(LockBody);
	LockRow->AddChildToHorizontalBox(Padlock)->SetVerticalAlignment(VAlign_Center);
	LockedText = DCUI::Text(WidgetTree, nullptr, TEXT(""), UDCText_Caution::StaticClass());
	LockedText->SetWrapTextAt(300.f);
	UHorizontalBoxSlot* LockedSlot = LockRow->AddChildToHorizontalBox(LockedText);
	LockedSlot->SetVerticalAlignment(VAlign_Center);
	LockedSlot->SetPadding(FMargin(14.f, 0.f));

	// Right: the live skill sum, the match with the order, the film title and the combine button
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	UCommonBorder* RightPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Panel::StaticClass(), FMargin(16.f));
	RightPanel->SetContent(Right);
	Cast<UHorizontalBoxSlot>(RightPanel->Slot)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	// Each block sits in an inset box, like the ORDER one
	auto InsetBlock = [this, Right](const TCHAR* Heading, float TopGap)
	{
		UCommonBorder* Box = DCUI::Border(WidgetTree, Right, UDCBorder_Inset::StaticClass(), FMargin(12.f, 8.f));
		Cast<UVerticalBoxSlot>(Box->Slot)->SetPadding(FMargin(0.f, TopGap, 0.f, 0.f));
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Box->SetContent(Content);
		DCUI::Text(WidgetTree, Content, Heading, UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
		return Content;
	};
	ProfileText = WidgetTree->ConstructWidget<UVerticalBox>(); // rebuilt on every refresh, so the heading stays outside it
	InsetBlock(TEXT("FILM PROFILE"), 0.f)->AddChild(ProfileText);
	UVerticalBox* MatchBox = InsetBlock(TEXT("MATCH WITH THE ORDER"), 8.f);
	MatchText = DCUI::Text(WidgetTree, MatchBox, TEXT(""), UDCText_Body::StaticClass());
	PointsText = DCUI::Text(WidgetTree, MatchBox, TEXT(""), UDCText_Body::StaticClass());

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
	DiscardButton = DCUI::Button(WidgetTree, Right, TEXT("Put away held film"), NAME_None, UDCButton_Tab::StaticClass());
	DiscardButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnDiscard);
	Cast<UVerticalBoxSlot>(DiscardButton->Slot)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	HeldText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());
	StatusText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());
	StatusText->SetWrapTextAt(400.f);
}

void UDCFilmCraftingWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	StatusText->SetText(FText::GetEmpty());
	ShowView(false);
	Refresh();
}

void UDCFilmCraftingWidget::ShowView(bool bCrafted)
{
	CombineBody->SetVisibility(bCrafted ? ESlateVisibility::Hidden : ESlateVisibility::SelfHitTestInvisible); // Hidden, not Collapsed: it keeps the size
	CraftedBody->SetVisibility(bCrafted ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetSelected(ViewTabButtons[0], !bCrafted);
	SetSelected(ViewTabButtons[1], bCrafted);
	if (bCrafted)
	{
		ShowFilm(Crafting->CraftedFilms.Num() - 1); // the newest until the mouse picks another
	}
}

void UDCFilmCraftingWidget::ShowFilm(int32 Index)
{
	const FCreatedFilm* Film = Crafting->CraftedFilms.IsValidIndex(Index) ? &Crafting->CraftedFilms[Index] : nullptr;
	ShownFilm = Film ? Index : INDEX_NONE;
	CraftedTitle->SetText(Film ? Film->Title : FText::FromString(TEXT("No films yet")));
	CraftedSkills->ClearChildren();
	TMap<FName, int32> Counts;
	int32 RatingSum = 0, Filled = 0;
	if (Film)
	{
		DCUI::SkillList(WidgetTree, CraftedSkills, Film->SkillProfile, Config->SkillCap);
		FVHSData Tape;
		for (const FName& Row : Film->SlotRows)
		{
			if (!Row.IsNone() && Inventory->GetVHSData(Row, Tape))
			{
				Counts.FindOrAdd(Row)++;
				RatingSum += Tape.Rating;
				++Filled;
			}
		}
	}
	// A film has no rating of its own: it shows the average of the tapes it was made from
	CraftedStars->SetText(FText::FromString(Filled ? DCUI::Stars(FMath::RoundToInt((float)RatingSum / Filled)) : FString()));
	FString Made;
	FVHSData Tape;
	for (const TPair<FName, int32>& Pair : Counts)
	{
		Inventory->GetVHSData(Pair.Key, Tape);
		Made += FString::Printf(TEXT("%s%s\n"), *DCUI::CleanTitle(Tape.Title), Pair.Value > 1 ? *FString::Printf(TEXT(" x%d"), Pair.Value) : TEXT(""));
	}
	CraftedTapes->SetText(FText::FromString(Made.TrimEnd()));
}

void UDCFilmCraftingWidget::OnViewClicked(FName View)
{
	ShowView(View == "Crafted");
}

void UDCFilmCraftingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (CraftedBody->GetVisibility() != ESlateVisibility::Collapsed)
	{
		for (const UDCButton* Btn : CraftedButtons)
		{
			const int32 Index = FCString::Atoi(*Btn->Payload.ToString());
			if (Btn->IsHovered() && Index != ShownFilm)
			{
				ShowFilm(Index);
			}
		}
	}
	// The game keeps running behind this screen: follow the client's patience live
	const ADCClient* Front = Director ? Director->GetFrontClient() : nullptr;
	if (Front && Front->IsAtFront() && Front->PatienceMax > 0.f)
	{
		OrderPatience->SetPercent(Front->Runtime.PatienceLeft / Front->PatienceMax);
	}
}

void UDCFilmCraftingWidget::Refresh()
{
	if (!Crafting || !Inventory)
	{
		return;
	}
	Picker->Refresh(); // copies already in a slot leave the list
	for (int32 i = 0; i < SlotBoxes.Num(); ++i)
	{
		SlotBoxes[i]->SetVisibility(i < Crafting->UnlockedSlots ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		const FName Row = Crafting->SlotRows.IsValidIndex(i) ? Crafting->SlotRows[i] : NAME_None;
		FVHSData Tape;
		const bool bHas = !Row.IsNone() && Inventory->GetVHSData(Row, Tape);
		SlotTexts[i]->SetText(bHas ? FText::FromString(DCUI::CleanTitle(Tape.Title)) : FText::FromString(TEXT("(empty)")));
		SlotClears[i]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (Crafting->CraftedFilms.Num() != CraftedShown) // rebuilt only when a film is added: a click on one must not destroy its own button
	{
		CraftedShown = Crafting->CraftedFilms.Num();
		CraftedList->ClearChildren();
		CraftedButtons.Reset();
		for (int32 i = CraftedShown - 1; i >= 0; --i)
		{
			const FCreatedFilm& Film = Crafting->CraftedFilms[i];
			UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, Film.Title.ToString(), FName(*FString::FromInt(i)), UDCButton_Row::StaticClass());
			Btn->OnPayloadClicked.AddUniqueDynamic(this, &UDCFilmCraftingWidget::OnCraftedClicked);
			// Same row as the inventory: cover (the first tape's), title with the skills under it, stars (average of the tapes)
			Btn->Label->RemoveFromParent();
			UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
			Btn->Content->AddChild(H);
			FVHSData Tape, First;
			bool bFirst = false;
			int32 RatingSum = 0, Filled = 0;
			for (const FName& Row : Film.SlotRows)
			{
				if (!Row.IsNone() && Inventory->GetVHSData(Row, Tape))
				{
					if (!bFirst)
					{
						First = Tape;
						bFirst = true;
					}
					RatingSum += Tape.Rating;
					++Filled;
				}
			}
			SetCover(AddCover(WidgetTree, H, FVector2D(34.f, 48.f)), bFirst ? &First : nullptr, Config);
			UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
			Text->AddChildToVerticalBox(Btn->Label)->SetHorizontalAlignment(HAlign_Left);
			DCUI::InlineSkills(WidgetTree, Text, Film.SkillProfile);
			UHorizontalBoxSlot* TitleSlot = H->AddChildToHorizontalBox(Text);
			TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			TitleSlot->SetVerticalAlignment(VAlign_Center);
			TitleSlot->SetPadding(FMargin(12.f, 0.f));
			H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, DCUI::Stars(Filled ? FMath::RoundToInt((float)RatingSum / Filled) : 0), UDCText_Stars::StaticClass()))->SetVerticalAlignment(VAlign_Center);
			CraftedList->AddChild(Btn);
			CraftedButtons.Add(Btn);
		}
		if (CraftedShown == 0)
		{
			DCUI::Text(WidgetTree, CraftedList, TEXT("No films yet."));
		}
		if (CraftedBody->GetVisibility() != ESlateVisibility::Collapsed)
		{
			ShowFilm(CraftedShown - 1);
		}
	}
	const bool bHasLocked = Crafting->UnlockedSlots < Config->MaxSlots;
	LockedBox->SetVisibility(bHasLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	LockedText->SetText(FText::FromString(FString::Printf(TEXT("SLOT %d  LOCKED\nUnlock it at the computer"), Crafting->UnlockedSlots + 1)));
	const int32 Filled = Crafting->GetFilledSlots();
	SlotsHeader->SetText(FText::FromString(FString::Printf(TEXT("SELECTED VHS (%d/%d)"), Filled, Crafting->UnlockedSlots)));
	ProfileText->ClearChildren();
	DCUI::SkillList(WidgetTree, ProfileText, Crafting->GetProfile(), Config->SkillCap);
	const ADCClient* Front = Director ? Director->GetFrontClient() : nullptr;
	if (Front && Front->IsAtFront())
	{
		const FClientRuntime& Rt = Front->Runtime;
		DCUI::OrderLines(WidgetTree, OrderText, Rt, PickerWidth - 24.f);
		OrderPatience->SetPercent(Front->PatienceMax > 0.f ? Rt.PatienceLeft / Front->PatienceMax : 0.f);
	}
	// The order's size is public: one tape never reaches it, so the player knows a combination is needed
	PointsText->SetText(FText::FromString(Front && Front->IsAtFront()
		? FString::Printf(TEXT("Order size: %d points   Your film: %d"), Front->Runtime.Target.Total(), Crafting->GetProfile().Total())
		: FString()));
	if (Front && Front->IsAtFront() && Filled >= 1)
	{
		const float Known = UBPFL_Scoring::ComputeKnownErrorRatio(Crafting->GetProfile(), Front->Runtime.HintedSkills, Front->Runtime.bFullyHinted, Front->Runtime.Target.Total(), Config);
		switch (UBPFL_Scoring::GetReaction(Known, Config))
		{
		case EReaction::Loved: MatchText->SetStyle(UDCText_Accent::StaticClass()); MatchText->SetText(FText::FromString(TEXT("GREAT"))); break;
		case EReaction::Liked: MatchText->SetStyle(UDCText_Accent::StaticClass()); MatchText->SetText(FText::FromString(TEXT("GOOD"))); break;
		case EReaction::Neutral: MatchText->SetStyle(UDCText_Caution::StaticClass()); MatchText->SetText(FText::FromString(TEXT("OKAY"))); break;
		default: MatchText->SetStyle(UDCText_Warn::StaticClass()); MatchText->SetText(FText::FromString(TEXT("POOR"))); break;
		}
	}
	else
	{
		MatchText->SetStyle(UDCText_Body::StaticClass());
		MatchText->SetText(FText::FromString(Front && Front->IsAtFront() ? TEXT("Pick a tape to see how it fits") : TEXT("No client at the counter")));
	}
	TitleBox->SetIsEnabled(Filled >= 1); // title only editable once a tape is in
	if (Crafting->SlotRows != LastSlotRows) // new mix: rename it, unless the player wrote their own title
	{
		LastSlotRows = Crafting->SlotRows;
		const FString Current = TitleBox->GetText().ToString();
		if (Current.IsEmpty() || Current == AutoTitle)
		{
			AutoTitle = Crafting->SuggestTitle();
			TitleBox->SetText(FText::FromString(AutoTitle));
		}
	}
	ConfirmButton->SetIsEnabled(Crafting->CanConfirm());
	ConfirmButton->Label->SetText(FText::FromString(Filled < Config->MinTapesToCombine ? FString::Printf(TEXT("NEED %d+ TAPES"), Config->MinTapesToCombine) : FString(TEXT("COMBINE"))));
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
			ADCAudioManager::PlayCueStatic(this, bOk ? TEXT("FilmPlace") : TEXT("SlotFull"));
			StatusText->SetText(bOk ? FText::GetEmpty() : FText::FromString(TEXT("No copies of that tape left")));
			return;
		}
	}
	ADCAudioManager::PlayCueStatic(this, TEXT("SlotFull"));
	StatusText->SetText(FText::FromString(TEXT("All slots are full: remove a tape first")));
}

void UDCFilmCraftingWidget::OnClearClicked(FName SlotName)
{
	ADCAudioManager::PlayCueStatic(this, TEXT("FilmRemove"));
	Crafting->ClearSlot(FCString::Atoi(*SlotName.ToString()));
}

void UDCFilmCraftingWidget::OnConfirm(FName Unused)
{
	if (Crafting->ConfirmFilm(TitleBox->GetText()))
	{
		TitleBox->SetText(FText::GetEmpty());
		AutoTitle.Empty();
	}
}

void UDCFilmCraftingWidget::OnCraftedClicked(FName Index)
{
	Crafting->EquipFilm(FCString::Atoi(*Index.ToString())); // HandleFilmCreated closes this screen and sends the camera home
}

void UDCFilmCraftingWidget::OnDiscard(FName Unused)
{
	ADCAudioManager::PlayCueStatic(this, TEXT("FilmDiscard"));
	Crafting->DiscardHeldFilm();
}

// ---------------------------------------------------------------- Dialogue widget

void UDCDialogueWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Wide strip along the bottom of the screen, between the key bar (left) and the cash box (right)
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -48.f));
	DCUI::Text(WidgetTree, V, TEXT("CUSTOMER"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	LineBox = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(LineBox);
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* ButtonSlot = V->AddChildToVerticalBox(Buttons);
	ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	RejectButton = DCUI::Button(WidgetTree, Buttons, TEXT("No, sorry"), NAME_None, UDCButton_Tab::StaticClass());
	RejectButton->OnPayloadClicked.AddDynamic(this, &UDCDialogueWidget::HandleReject);
	Button = DCUI::Button(WidgetTree, Buttons, TEXT(""));
	Button->OnPayloadClicked.AddDynamic(this, &UDCDialogueWidget::HandleButton);
	Cast<UHorizontalBoxSlot>(Button->Slot)->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
}

void UDCDialogueWidget::Show(const FText& Line, const FText& ButtonLabel, bool bCanReject, const FClientRuntime* Order)
{
	if (Order)
	{
		DCUI::OrderLines(WidgetTree, LineBox, *Order, 1400.f);
	}
	else
	{
		LineBox->ClearChildren();
		DCUI::Text(WidgetTree, LineBox, Line.ToString())->SetWrapTextAt(1400.f);
	}
	Button->Label->SetText(ButtonLabel);
	RejectButton->SetVisibility(bCanReject ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------- Order panel

void UDCOrderPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCommonBorder* Panel = nullptr;
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.f, 0.f), FVector2D::ZeroVector, FVector2D(24.f, 24.f), &Panel);
	PanelRoot = Panel;
	DCUI::Text(WidgetTree, V, TEXT("ORDER"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 6.f));
	OrderText = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(OrderText);
	PatienceBar = DCUI::Bar(WidgetTree, V, 360.f);
	PanelRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCOrderPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ADCClient* Front = Director ? Director->GetFrontClient() : nullptr;
	if (!Front || !Front->IsAtFront() || !Front->bOrderAcknowledged) // the order is pinned only once the player has taken it
	{
		PanelRoot->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	PanelRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FClientRuntime& Rt = Front->Runtime;
	if (ShownClient.Get() != Front)
	{
		ShownClient = Front;
		DCUI::OrderLines(WidgetTree, OrderText, Rt, 360.f);
	}
	PatienceBar->SetPercent(Front->PatienceMax > 0.f ? Front->Runtime.PatienceLeft / Front->PatienceMax : 0.f);
}
