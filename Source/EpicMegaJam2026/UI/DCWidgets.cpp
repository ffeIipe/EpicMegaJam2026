#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/DCPlayerController.h"
#include "Core/DCSettings.h"
#include "Core/BPFL_Scoring.h"
#include "Data/PDA_GameConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
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

#define LOCTEXT_NAMESPACE "DirectorsCut"

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
		if (Tape && DCUI::SetCoverTexture(Img, Tape->Cover))
		{
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

	template <typename Fn>
	void PopulateGenreTabs(UWidgetTree* Tree, UWrapBox* Tabs, const UPDA_GameConfig* Config, Fn&& OnCreated)
	{
		if (!Tabs)
		{
			return;
		}
		Tabs->ClearChildren();
		auto AddTab = [&](const FText& Label, FName Id)
		{
			UDCButton* Tab = DCUI::Button(Tree, Tabs, Label.ToString().ToUpper(), Id, UDCButton_Tab::StaticClass());
			Tab->SetIsSelectable(true);
			OnCreated(Tab);
		};
		AddTab(LOCTEXT("Inv_TabAll", "ALL"), AllTab);
		if (Config && Config->GenreTable)
		{
			for (const TPair<FName, uint8*>& Pair : Config->GenreTable->GetRowMap())
			{
				const FGenreData* Row = reinterpret_cast<const FGenreData*>(Pair.Value);
				if (Row->Genre != EGenre::Paranormal) // DEPRECATED v1 genre: it has no tapes
				{
					AddTab(Row->DisplayName, GenreName(Row->Genre));
				}
			}
		}
	}

	bool CraftedMatchesGenre(const FCreatedFilm& Film, const UAC_Inventory* Inventory, FName Tab)
	{
		if (Tab == AllTab || !Inventory)
		{
			return true;
		}
		const int64 EnumVal = StaticEnum<EGenre>()->GetValueByName(Tab);
		if (EnumVal == INDEX_NONE)
		{
			return true;
		}
		const EGenre Wanted = (EGenre)EnumVal;
		FVHSData Tape;
		for (const FName& Row : Film.SlotRows)
		{
			if (!Row.IsNone() && Inventory->GetVHSData(Row, Tape) && Tape.PrimaryGenre == Wanted)
			{
				return true;
			}
		}
		return false;
	}

	// Supplier-style card button: green T_UI_Panel frame, cover (54x96), uppercase title, stars, inline skills, optional copy count on the right
	UDCButton* AddFilmCardButton(UWidgetTree* Tree, UPanelWidget* Parent, FName Payload, const FString& Title, int32 Stars, const FSkillVector& Skills,
		const FVHSData* CoverTape, const UPDA_GameConfig* Config, int32 CopiesCount = -1)
	{
		UDCButton* Btn = DCUI::Button(Tree, nullptr, Title.ToUpper(), Payload, UDCButton_CardRow::StaticClass());
		Btn->Label->RemoveFromParent();
		UHorizontalBox* H = Tree->ConstructWidget<UHorizontalBox>();
		Btn->Content->AddChild(H);
		UImage* CoverImg = AddCover(Tree, H, FVector2D(54.f, 96.f));
		SetCover(CoverImg, CoverTape, Config);
		Cast<UHorizontalBoxSlot>(CoverImg->GetParent()->Slot)->SetVerticalAlignment(VAlign_Center);

		UVerticalBox* Info = Tree->ConstructWidget<UVerticalBox>();
		Btn->Label->SetStyle(UDCText_Heading::StaticClass());
		Btn->Label->SetAutoWrapText(true);
		Info->AddChildToVerticalBox(Btn->Label)->SetHorizontalAlignment(HAlign_Left);
		DCUI::Text(Tree, Info, DCUI::Stars(Stars), UDCText_Stars::StaticClass())->SetMargin(FMargin(0.f, 2.f));
		DCUI::InlineSkills(Tree, Info, Skills);
		UHorizontalBoxSlot* InfoSlot = H->AddChildToHorizontalBox(Info);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetVerticalAlignment(VAlign_Center);
		InfoSlot->SetPadding(FMargin(16.f, 0.f, 12.f, 0.f));

		if (CopiesCount >= 0)
		{
			H->AddChildToHorizontalBox(DCUI::Text(Tree, nullptr, FString::Printf(TEXT("x%d"), CopiesCount), UDCText_Accent::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBox* V = Cast<UVerticalBox>(Parent))
		{
			V->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
		}
		else if (Parent)
		{
			Parent->AddChild(Btn);
		}
		return Btn;
	}

}

// ---------------------------------------------------------------- Inventory widget

void UDCInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("PrevTab"), false, FSimpleDelegate::CreateUObject(this, &UDCInventoryWidget::CycleTab, -1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("NextTab"), false, FSimpleDelegate::CreateUObject(this, &UDCInventoryWidget::CycleTab, 1)));
	LanguageChangedHandle = UDCSettings::OnLanguageChanged.AddUObject(this, &UDCInventoryWidget::HandleLanguageChanged);
}

void UDCInventoryWidget::NativeDestruct()
{
	UDCSettings::OnLanguageChanged.Remove(LanguageChangedHandle);
	Super::NativeDestruct();
}

void UDCInventoryWidget::HandleLanguageChanged()
{
	RebuildTabs();
	Refresh();
}

void UDCInventoryWidget::RebuildTabs()
{
	PopulateGenreTabs(WidgetTree, Tabs, Config, [this](UDCButton* Tab)
	{
		Tab->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTabClicked);
	});
}

void UDCInventoryWidget::Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig, bool bInPicker, UAC_FilmCrafting* InCrafting)
{
	Inventory = InInventory;
	Config = InConfig;
	bPicker = bInPicker;
	Crafting = InCrafting;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;
	const float ListW = bPicker ? PickerWidth : ListWidth;
	Tabs = WidgetTree->ConstructWidget<UWrapBox>();
	Tabs->SetInnerSlotPadding(FVector2D(4.f, 4.f));
	if (bPicker)
	{
		// Auto-wrap to the left column width: fits on one row at 1080p+, wraps cleanly on smaller screens
		Tabs->SetExplicitWrapSize(false);
	}
	else
	{
		Tabs->SetExplicitWrapSize(true);
		Tabs->SetWrapSize(ListW + DetailWidth + 64.f);
	}
	Root->AddChildToVerticalBox(Tabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	RebuildTabs();

	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* BodySlot = Root->AddChildToVerticalBox(Body);
	if (bPicker)
	{
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UCommonBorder* ListPanel = DCUI::Border(WidgetTree, Body, UDCBorder_Inset::StaticClass(), FMargin(10.f));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
	if (bPicker)
	{
		Cast<UHorizontalBoxSlot>(ListPanel->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ListPanel->SetContent(Scroll);
	}
	else
	{
		USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>();
		ListSize->SetWidthOverride(ListW);
		ListSize->SetHeightOverride(440.f);
		ListPanel->SetContent(ListSize);
		ListSize->AddChild(Scroll);
	}
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
	DetailCover = AddCover(WidgetTree, Info, FVector2D(135.f, 240.f));
	UVerticalBox* Facts = WidgetTree->ConstructWidget<UVerticalBox>();
	Info->AddChildToHorizontalBox(Facts)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	DCUI::Text(WidgetTree, Facts, LOCTEXT("Inv_Rating", "Rating:"));
	DetailStars = DCUI::Text(WidgetTree, Facts, TEXT(""), UDCText_Stars::StaticClass());
	DCUI::Text(WidgetTree, Facts, LOCTEXT("Inv_Genre", "Genre:"))->SetMargin(FMargin(0.f, 14.f, 0.f, 0.f));
	DetailGenre = DCUI::Text(WidgetTree, Facts, TEXT(""), UDCText_Accent::StaticClass());
	DetailGenre->SetWrapTextAt(250.f);
	DCUI::Text(WidgetTree, Facts, LOCTEXT("Inv_Skills", "Genres:"))->SetMargin(FMargin(0.f, 14.f, 0.f, 0.f));
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
		DCUI::Text(WidgetTree, List, bPicker ? LOCTEXT("Inv_EmptyPicker", "No tapes left to combine.") : LOCTEXT("Inv_EmptyShelf", "No tapes on this shelf."));
	}

	for (const FName& Row : Rows)
	{
		FVHSData Tape;
		if (!Inventory->GetVHSData(Row, Tape))
		{
			continue;
		}
		if (bPicker)
		{
			UDCButton* Btn = AddFilmCardButton(WidgetTree, List, Row, DCUI::CleanTitle(Tape.Title), Tape.Rating, Tape.Skills, &Tape, Config, Copies(Row));
			DCUI::SkillTooltip(WidgetTree, Btn, Tape.Skills);
			Btn->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTapeClicked);
			Btn->SetIsSelectable(false); // picker needs repeat clicks (one copy per click)
			continue;
		}
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, DCUI::CleanTitle(Tape.Title), Row, UDCButton_Row::StaticClass());
		DCUI::SkillTooltip(WidgetTree, Btn, Tape.Skills);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTapeClicked);
		// Row: thumbnail, title with the skills under it, copies, stars
		Btn->Label->RemoveFromParent();
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Btn->Content->AddChild(H);
		SetCover(AddCover(WidgetTree, H, FVector2D(36.f, 64.f)), &Tape, Config);
		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>();
		Btn->Label->SetAutoWrapText(true);
		Text->AddChildToVerticalBox(Btn->Label)->SetHorizontalAlignment(HAlign_Left);
		DCUI::InlineSkills(WidgetTree, Text, Tape.Skills);
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
	// Esc (the default back) and Space both return to the counter; one hint on the bar says so: "ESC / SPACE Back"
	bIsBackActionDisplayedInActionBar = false;
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("BackSpace"), true, FSimpleDelegate::CreateLambda([this]()
	{
		if (ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer()))
		{
			PC->CloseScreen();
		}
	})));
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Inventory = GM->Inventory;
	Crafting = GM->FilmCrafting;
	Config = GM->Config;
	Director = GM->ClientDirector;

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UVerticalBox* RootStack = WidgetTree->ConstructWidget<UVerticalBox>();
	UCanvasPanelSlot* RootSlot = Canvas->AddChildToCanvas(RootStack);
	RootSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	// Leave top-right HUD strip and bottom-left "[ESC / SPACE] Back" action bar completely clear
	RootSlot->SetOffsets(FMargin(24.f, 12.f, 24.f, 84.f));

	// Top parallelogram tabs: COMBINE VHS | CRAFTED VHS
	UHorizontalBox* ViewTabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* TabsSlot = RootStack->AddChildToVerticalBox(ViewTabs);
	TabsSlot->SetHorizontalAlignment(HAlign_Left);
	TabsSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	for (const TPair<const TCHAR*, FText>& Def : {
		TPair<const TCHAR*, FText>(TEXT("Combine"), LOCTEXT("Craft_Title", "COMBINE VHS")),
		TPair<const TCHAR*, FText>(TEXT("Crafted"), LOCTEXT("Craft_TabCrafted", "CRAFTED VHS")) })
	{
		UDCButton* Tab = DCUI::Button(WidgetTree, ViewTabs, Def.Value, Def.Key, UDCButton_HeaderTab::StaticClass());
		Tab->SetIsSelectable(true);
		Tab->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnViewClicked);
		Cast<UHorizontalBoxSlot>(Tab->Slot)->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
		ViewTabButtons.Add(Tab);
	}

	// Single outer green-bordered frame shared by both views
	UCommonBorder* MainFrame = DCUI::Border(WidgetTree, RootStack, UDCBorder_Panel::StaticClass(), FMargin(14.f));
	Cast<UVerticalBoxSlot>(MainFrame->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// The client's order (title with level, hint lines, patience bar) in an inset box; both views of the screen have one
	auto AddOrderBlock = [this](UVerticalBox* Parent, TObjectPtr<UCommonTextBlock>& Heading, TObjectPtr<UVerticalBox>& Text, TObjectPtr<UProgressBar>& Bar)
	{
		UCommonBorder* Box = DCUI::Border(WidgetTree, Parent, UDCBorder_Inset::StaticClass(), FMargin(12.f, 8.f));
		Cast<UVerticalBoxSlot>(Box->Slot)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Box->SetContent(Content);
		Heading = DCUI::Text(WidgetTree, Content, LOCTEXT("Order_Heading", "ORDER"), UDCText_Heading::StaticClass());
		Heading->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
		Text = WidgetTree->ConstructWidget<UVerticalBox>();
		Content->AddChild(Text);
		Bar = DCUI::Bar(WidgetTree, Content, 200.f);
		Cast<USizeBox>(Bar->GetParent())->ClearWidthOverride();
		Cast<UVerticalBoxSlot>(Bar->GetParent()->Slot)->SetHorizontalAlignment(HAlign_Fill);
		Cast<UVerticalBoxSlot>(Bar->GetParent()->Slot)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	};

	// Both views share one overlay inside the main frame
	UOverlay* Views = WidgetTree->ConstructWidget<UOverlay>();
	MainFrame->SetContent(Views);
	UHorizontalBox* Body = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* BodySlot = Views->AddChildToOverlay(Body);
	BodySlot->SetHorizontalAlignment(HAlign_Fill);
	BodySlot->SetVerticalAlignment(VAlign_Fill);
	CombineBody = Body;

	// Library view: same 2-column layout (genre tabs + list + order on left, detail panel on right)
	UHorizontalBox* Library = WidgetTree->ConstructWidget<UHorizontalBox>();
	UOverlaySlot* LibrarySlot = Views->AddChildToOverlay(Library);
	LibrarySlot->SetHorizontalAlignment(HAlign_Fill);
	LibrarySlot->SetVerticalAlignment(VAlign_Fill);
	CraftedBody = Library;
	UVerticalBox* CraftedLeft = WidgetTree->ConstructWidget<UVerticalBox>();
	Cast<UHorizontalBoxSlot>(Library->AddChildToHorizontalBox(CraftedLeft))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CraftedTabs = WidgetTree->ConstructWidget<UWrapBox>();
	CraftedTabs->SetInnerSlotPadding(FVector2D(4.f, 4.f));
	CraftedTabs->SetExplicitWrapSize(false);
	CraftedLeft->AddChildToVerticalBox(CraftedTabs)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	RebuildCraftedTabs();
	UCommonBorder* CraftedPanel = DCUI::Border(WidgetTree, CraftedLeft, UDCBorder_Inset::StaticClass(), FMargin(10.f));
	Cast<UVerticalBoxSlot>(CraftedPanel->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UScrollBox* CraftedScroll = WidgetTree->ConstructWidget<UScrollBox>();
	CraftedPanel->SetContent(CraftedScroll);
	CraftedList = WidgetTree->ConstructWidget<UVerticalBox>();
	CraftedScroll->AddChild(CraftedList);
	AddOrderBlock(CraftedLeft, CraftedOrderHeading, CraftedOrderText, CraftedOrderPatience);

	constexpr float RightColWidth = 480.f;
	USizeBox* CraftedDetailSize = WidgetTree->ConstructWidget<USizeBox>();
	CraftedDetailSize->SetWidthOverride(RightColWidth);
	UHorizontalBoxSlot* CraftedDetailWrapSlot = Library->AddChildToHorizontalBox(CraftedDetailSize);
	CraftedDetailWrapSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	UCommonBorder* CraftedDetailPanel = DCUI::Border(WidgetTree, nullptr, UDCBorder_Inset::StaticClass(), FMargin(16.f));
	CraftedDetailSize->AddChild(CraftedDetailPanel);
	UVerticalBox* CraftedDetail = WidgetTree->ConstructWidget<UVerticalBox>();
	CraftedDetailPanel->SetContent(CraftedDetail);
	CraftedTitle = DCUI::Text(WidgetTree, CraftedDetail, TEXT(""), UDCText_Heading::StaticClass());
	CraftedTitle->SetWrapTextAt(RightColWidth - 40.f);
	CraftedStars = DCUI::Text(WidgetTree, CraftedDetail, TEXT(""), UDCText_Stars::StaticClass());
	CraftedStars->SetMargin(FMargin(0.f, 6.f, 0.f, 10.f));
	DCUI::Text(WidgetTree, CraftedDetail, LOCTEXT("Craft_Skills", "GENRES"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 4.f));
	CraftedSkills = WidgetTree->ConstructWidget<UVerticalBox>();
	CraftedDetail->AddChild(CraftedSkills);
	DCUI::Text(WidgetTree, CraftedDetail, LOCTEXT("Craft_MadeFrom", "MADE FROM"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 12.f, 0.f, 4.f));
	CraftedTapes = DCUI::Text(WidgetTree, CraftedDetail, TEXT(""), UDCText_Body::StaticClass());
	CraftedTapes->SetWrapTextAt(RightColWidth - 40.f);
	UCommonTextBlock* CraftedHint = DCUI::Text(WidgetTree, CraftedDetail, LOCTEXT("Craft_Hint", "Click a film to take it to the counter"), UDCText_Accent::StaticClass());
	CraftedHint->SetMargin(FMargin(0.f, 12.f, 0.f, 0.f));
	CraftedHint->SetWrapTextAt(RightColWidth - 40.f);

	// Combine Left: genre tabs + VHS list + ORDER block at the bottom
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>();
	Cast<UHorizontalBoxSlot>(Body->AddChildToHorizontalBox(Left))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Picker = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	Left->AddChildToVerticalBox(Picker)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Picker->Setup(Inventory, Config, true, Crafting);
	AddOrderBlock(Left, OrderHeading, OrderText, OrderPatience);
	Picker->OnVHSPicked.AddDynamic(this, &UDCFilmCraftingWidget::OnPicked);
	Inventory->OnInventoryChanged.AddDynamic(Picker, &UDCInventoryWidget::Refresh);
	Crafting->OnFilmStateChanged.AddDynamic(this, &UDCFilmCraftingWidget::OnFilmStateChanged);

	// Combine Right: SELECTED VHS slots, Film title, Combine + Discard, FILM PROFILE bars
	USizeBox* RightSize = WidgetTree->ConstructWidget<USizeBox>();
	RightSize->SetWidthOverride(RightColWidth);
	Body->AddChildToHorizontalBox(RightSize)->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>();
	RightSize->AddChild(Right);

	UCommonBorder* SlotsPanel = DCUI::Border(WidgetTree, Right, UDCBorder_Inset::StaticClass(), FMargin(12.f, 10.f));
	UVerticalBox* SlotsBox = WidgetTree->ConstructWidget<UVerticalBox>();
	SlotsPanel->SetContent(SlotsBox);
	SlotsHeader = DCUI::Text(WidgetTree, SlotsBox, TEXT(""), UDCText_Heading::StaticClass());
	SlotsHeader->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	for (int32 i = 0; i < FMath::Max(Config->MaxSlots, 1); ++i)
	{
		const FName SlotName(*FString::FromInt(i));
		UCommonBorder* Box = DCUI::Border(WidgetTree, SlotsBox, UDCBorder_Inset::StaticClass(), FMargin(10.f, 6.f));
		Cast<UVerticalBoxSlot>(Box->Slot)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		SlotBoxes.Add(Box);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		Box->SetContent(H);
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("%d"), i + 1), UDCText_Heading::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		UCommonTextBlock* Title = DCUI::Text(WidgetTree, nullptr, TEXT(""));
		Title->SetWrapTextAt(340.f);
		UHorizontalBoxSlot* TitleSlot = H->AddChildToHorizontalBox(Title);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		TitleSlot->SetPadding(FMargin(12.f, 0.f));
		SlotTexts.Add(Title);
		UDCButton* Clear = DCUI::Button(WidgetTree, H, TEXT("x"), SlotName, UDCButton_Tab::StaticClass());
		Cast<UHorizontalBoxSlot>(Clear->Slot)->SetVerticalAlignment(VAlign_Center);
		Clear->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnClearClicked);
		SlotClears.Add(Clear);
	}

	// Next slot to buy: padlock (shackle over body) + "SLOT N  LOCKED"
	UCommonBorder* Locked = DCUI::Border(WidgetTree, SlotsBox, UDCBorder_Inset::StaticClass(), FMargin(10.f, 6.f));
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
	LockedText->SetWrapTextAt(320.f);
	UHorizontalBoxSlot* LockedSlot = LockRow->AddChildToHorizontalBox(LockedText);
	LockedSlot->SetVerticalAlignment(VAlign_Center);
	LockedSlot->SetPadding(FMargin(14.f, 0.f));

	UCommonBorder* ProfileBox = DCUI::Border(WidgetTree, Right, UDCBorder_Inset::StaticClass(), FMargin(12.f, 10.f));
	UVerticalBoxSlot* ProfileBoxSlot = Cast<UVerticalBoxSlot>(ProfileBox->Slot);
	ProfileBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ProfileBoxSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	UVerticalBox* ProfileContent = WidgetTree->ConstructWidget<UVerticalBox>();
	ProfileBox->SetContent(ProfileContent);
	DCUI::Text(WidgetTree, ProfileContent, LOCTEXT("Craft_FilmProfile", "FILM PROFILE"), UDCText_Heading::StaticClass())->SetMargin(FMargin(0.f, 0.f, 0.f, 6.f));
	ProfileText = WidgetTree->ConstructWidget<UVerticalBox>();
	ProfileContent->AddChild(ProfileText);
	MatchText = DCUI::Text(WidgetTree, ProfileContent, TEXT(""), UDCText_Body::StaticClass());
	MatchText->SetMargin(FMargin(0.f, 8.f, 0.f, 0.f));
	PointsText = DCUI::Text(WidgetTree, ProfileContent, TEXT(""), UDCText_Body::StaticClass());

	TitleBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	TitleBox->SetHintText(LOCTEXT("Craft_TitleHint", "Film title"));
	FEditableTextBoxStyle FieldStyle = TitleBox->GetWidgetStyle();
	const FSlateBrush Field = DCUI::Box(DCPalette::Navy, DCPalette::Blue);
	const FSlateBrush FieldActive = DCUI::Box(DCPalette::Navy, DCPalette::Lime);
	FieldStyle.SetBackgroundImageNormal(Field).SetBackgroundImageReadOnly(Field).SetBackgroundImageHovered(FieldActive).SetBackgroundImageFocused(FieldActive)
		.SetFont(GetDefault<UDCText_Body>()->Font).SetForegroundColor(DCPalette::Beige).SetFocusedForegroundColor(DCPalette::Beige)
		.SetReadOnlyForegroundColor(DCPalette::Grey.CopyWithNewOpacity(0.5f)).SetPadding(FMargin(10.f, 6.f));
	TitleBox->SetWidgetStyle(FieldStyle);
	Right->AddChildToVerticalBox(TitleBox)->SetPadding(FMargin(0.f, 10.f, 0.f, 8.f));

	USizeBox* ConfirmSize = WidgetTree->ConstructWidget<USizeBox>();
	ConfirmSize->SetHeightOverride(58.f);
	Right->AddChildToVerticalBox(ConfirmSize);
	ConfirmButton = DCUI::Button(WidgetTree, ConfirmSize, LOCTEXT("Craft_CombineBtn", "COMBINE"));
	ConfirmButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnConfirm);
	DiscardButton = DCUI::Button(WidgetTree, Right, LOCTEXT("Craft_DiscardBtn", "DISCARD"), NAME_None, UDCButton_Discard::StaticClass());
	DiscardButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnDiscard);
	UVerticalBoxSlot* DiscardSlot = Cast<UVerticalBoxSlot>(DiscardButton->Slot);
	DiscardSlot->SetHorizontalAlignment(HAlign_Center);
	DiscardSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

	HeldText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());
	HeldText->SetWrapTextAt(RightColWidth - 20.f);
	StatusText = DCUI::Text(WidgetTree, Right, TEXT(""), UDCText_Accent::StaticClass());
	StatusText->SetWrapTextAt(RightColWidth - 20.f);

	LanguageChangedHandle = UDCSettings::OnLanguageChanged.AddUObject(this, &UDCFilmCraftingWidget::HandleLanguageChanged);
}

void UDCFilmCraftingWidget::NativeDestruct()
{
	UDCSettings::OnLanguageChanged.Remove(LanguageChangedHandle);
	Super::NativeDestruct();
}

void UDCFilmCraftingWidget::HandleLanguageChanged()
{
	RebuildCraftedTabs();
	CraftedShown = -1;
	Refresh();
}

void UDCFilmCraftingWidget::RebuildCraftedTabs()
{
	PopulateGenreTabs(WidgetTree, CraftedTabs, Config, [this](UDCButton* Tab)
	{
		Tab->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnCraftedTabClicked);
	});
}

void UDCFilmCraftingWidget::OnCraftedTabClicked(FName Tab)
{
	CraftedGenreTab = Tab;
	ADCAudioManager::PlayCueStatic(this, TEXT("TabSwitch"));
	RebuildCraftedList();
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
		const int32 FirstIdx = CraftedButtons.Num() > 0 ? FCString::Atoi(*CraftedButtons[0]->Payload.ToString()) : INDEX_NONE;
		ShowFilm(FirstIdx); // the newest matching the active filter until the mouse picks another
	}
}

void UDCFilmCraftingWidget::ShowFilm(int32 Index)
{
	const FCreatedFilm* Film = Crafting->CraftedFilms.IsValidIndex(Index) ? &Crafting->CraftedFilms[Index] : nullptr;
	ShownFilm = Film ? Index : INDEX_NONE;
	CraftedTitle->SetText(Film ? Film->Title : LOCTEXT("Craft_NoFilmsDetail", "No films yet"));
	CraftedSkills->ClearChildren();
	TMap<FName, int32> Counts;
	int32 RatingSum = 0, Filled = 0;
	if (Film)
	{
		DCUI::SkillBars(WidgetTree, CraftedSkills, Film->SkillProfile, 10);
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
		CraftedOrderPatience->SetPercent(Front->Runtime.PatienceLeft / Front->PatienceMax);
	}
}

void UDCFilmCraftingWidget::RebuildCraftedList()
{
	if (!Crafting || !Inventory || !CraftedList)
	{
		return;
	}
	if (CraftedTabs)
	{
		for (UWidget* W : CraftedTabs->GetAllChildren())
		{
			if (UDCButton* Tab = Cast<UDCButton>(W))
			{
				SetSelected(Tab, Tab->Payload == CraftedGenreTab);
			}
		}
	}
	CraftedShown = Crafting->CraftedFilms.Num();
	CraftedList->ClearChildren();
	CraftedButtons.Reset();
	int32 FirstShownIndex = INDEX_NONE;
	for (int32 i = CraftedShown - 1; i >= 0; --i)
	{
		const FCreatedFilm& Film = Crafting->CraftedFilms[i];
		if (!CraftedMatchesGenre(Film, Inventory, CraftedGenreTab))
		{
			continue;
		}
		if (FirstShownIndex == INDEX_NONE)
		{
			FirstShownIndex = i;
		}
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
		const int32 AvgStars = Filled ? FMath::RoundToInt((float)RatingSum / Filled) : 0;
		UDCButton* Btn = AddFilmCardButton(WidgetTree, CraftedList, FName(*FString::FromInt(i)), Film.Title.ToString(), AvgStars, Film.SkillProfile, bFirst ? &First : nullptr, Config);
		Btn->OnPayloadClicked.AddUniqueDynamic(this, &UDCFilmCraftingWidget::OnCraftedClicked);
		CraftedButtons.Add(Btn);
	}
	if (CraftedButtons.IsEmpty())
	{
		DCUI::Text(WidgetTree, CraftedList, LOCTEXT("Craft_NoFilmsList", "No films yet."));
	}
	if (CraftedBody->GetVisibility() != ESlateVisibility::Collapsed)
	{
		ShowFilm(FirstShownIndex);
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
		SlotTexts[i]->SetText(bHas ? FText::FromString(DCUI::CleanTitle(Tape.Title)) : LOCTEXT("Craft_EmptySlot", "(empty)"));
		SlotClears[i]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	if (Crafting->CraftedFilms.Num() != CraftedShown) // rebuilt only when a film is added: a click on one must not destroy its own button
	{
		RebuildCraftedList();
	}
	const bool bHasLocked = Crafting->UnlockedSlots < Config->MaxSlots;
	LockedBox->SetVisibility(bHasLocked ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	LockedText->SetText(FText::Format(LOCTEXT("Craft_SlotLocked", "SLOT {0}  LOCKED\nUnlock it at the computer"), FText::FromString(FString::FromInt(Crafting->UnlockedSlots + 1))));
	const int32 Filled = Crafting->GetFilledSlots();
	SlotsHeader->SetText(FText::Format(LOCTEXT("Craft_SelectedVHS", "SELECTED VHS ({0}/{1})"),
		FText::FromString(FString::FromInt(Filled)), FText::FromString(FString::FromInt(Crafting->UnlockedSlots))));
	ProfileText->ClearChildren();
	DCUI::SkillBars(WidgetTree, ProfileText, Crafting->GetProfile(), 10);
	const ADCClient* Front = Director ? Director->GetFrontClient() : nullptr;
	if (Front && Front->IsAtFront())
	{
		const FClientRuntime& Rt = Front->Runtime;
		DCUI::OrderLines(WidgetTree, OrderText, Rt, 0.f);
		OrderHeading->SetText(FText::Format(LOCTEXT("Order_HeadingTier", "ORDER   {0}"), DCUI::ClientTierLabel(Config, Rt.Tier)));
		OrderPatience->SetPercent(Front->PatienceMax > 0.f ? Rt.PatienceLeft / Front->PatienceMax : 0.f);
		DCUI::OrderLines(WidgetTree, CraftedOrderText, Rt, 0.f);
		CraftedOrderHeading->SetText(OrderHeading->GetText());
		CraftedOrderPatience->SetPercent(Front->PatienceMax > 0.f ? Rt.PatienceLeft / Front->PatienceMax : 0.f);
	}
	// The order's size is public: one tape never reaches it, so the player knows a combination is needed
	PointsText->SetText(Front && Front->IsAtFront()
		? FText::Format(LOCTEXT("Craft_OrderPoints", "Order size: {0} points   Your film: {1}"),
			FText::FromString(FString::FromInt(Front->Runtime.Target.Total())),
			FText::FromString(FString::FromInt(Crafting->GetProfile().Total())))
		: FText::GetEmpty());
	if (Front && Front->IsAtFront() && Filled >= 1)
	{
		const float Known = UBPFL_Scoring::ComputeErrorRatio(Crafting->GetProfile(), Front->Runtime.Target, Config); // same ratio as the real reaction, so the meter never lies
		switch (UBPFL_Scoring::GetReaction(Known, Config))
		{
		case EReaction::Loved: MatchText->SetStyle(UDCText_Accent::StaticClass()); MatchText->SetText(LOCTEXT("Craft_MatchGreat", "GREAT")); break;
		case EReaction::Liked: MatchText->SetStyle(UDCText_Accent::StaticClass()); MatchText->SetText(LOCTEXT("Craft_MatchGood", "GOOD")); break;
		case EReaction::Neutral: MatchText->SetStyle(UDCText_Caution::StaticClass()); MatchText->SetText(LOCTEXT("Craft_MatchOkay", "OKAY")); break;
		default: MatchText->SetStyle(UDCText_Warn::StaticClass()); MatchText->SetText(LOCTEXT("Craft_MatchPoor", "POOR")); break;
		}
	}
	else
	{
		MatchText->SetStyle(UDCText_Body::StaticClass());
		MatchText->SetText(Front && Front->IsAtFront()
			? LOCTEXT("Craft_PickTapeHint", "Pick a tape to see how it fits")
			: LOCTEXT("Craft_NoClient", "No client at the counter"));
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
	ConfirmButton->Label->SetText(Filled < Config->MinTapesToCombine
		? FText::Format(LOCTEXT("Craft_NeedTapes", "NEED {0}+ TAPES"), FText::FromString(FString::FromInt(Config->MinTapesToCombine)))
		: LOCTEXT("Craft_CombineBtn", "COMBINE"));
	DiscardButton->SetIsEnabled(Crafting->HeldFilms.Num() > 0);
	HeldText->SetText(Crafting->HeldFilms.Num() > 0
		? FText::Format(LOCTEXT("Stats_InHand", "In hand: {0}"), Crafting->HeldFilms[0].Title)
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
			StatusText->SetText(bOk ? FText::GetEmpty() : LOCTEXT("Craft_NoCopies", "No copies of that tape left"));
			return;
		}
	}
	ADCAudioManager::PlayCueStatic(this, TEXT("SlotFull"));
	StatusText->SetText(LOCTEXT("Craft_SlotsFull", "All slots are full: remove a tape first"));
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
	HeadingText = DCUI::Text(WidgetTree, V, LOCTEXT("Dialogue_Customer", "CUSTOMER"), UDCText_Heading::StaticClass());
	HeadingText->SetMargin(FMargin(0.f, 0.f, 0.f, 8.f));
	LineBox = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(LineBox);
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UVerticalBoxSlot* ButtonSlot = V->AddChildToVerticalBox(Buttons);
	ButtonSlot->SetHorizontalAlignment(HAlign_Right);
	ButtonSlot->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));
	RejectButton = DCUI::Button(WidgetTree, Buttons, LOCTEXT("Dialogue_Reject", "No, sorry"), NAME_None, UDCButton_Tab::StaticClass());
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
		HeadingText->SetText(FText::Format(LOCTEXT("Dialogue_CustomerTier", "CUSTOMER   {0}"), DCUI::ClientTierLabel(GetWorld()->GetAuthGameMode<ADCGameMode>()->Config, Order->Tier)));
	}
	else
	{
		LineBox->ClearChildren();
		HeadingText->SetText(LOCTEXT("Dialogue_Customer", "CUSTOMER"));
		DCUI::Text(WidgetTree, LineBox, Line)->SetWrapTextAt(1400.f);
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
	HeadingText = DCUI::Text(WidgetTree, V, LOCTEXT("Order_Heading", "ORDER"), UDCText_Heading::StaticClass());
	HeadingText->SetMargin(FMargin(0.f, 0.f, 0.f, 6.f));
	OrderText = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(OrderText);
	PatienceBar = DCUI::Bar(WidgetTree, V, 360.f);
	PanelRoot->SetVisibility(ESlateVisibility::Collapsed);
	LanguageChangedHandle = UDCSettings::OnLanguageChanged.AddUObject(this, &UDCOrderPanelWidget::HandleLanguageChanged);
}

void UDCOrderPanelWidget::NativeDestruct()
{
	UDCSettings::OnLanguageChanged.Remove(LanguageChangedHandle);
	Super::NativeDestruct();
}

void UDCOrderPanelWidget::HandleLanguageChanged()
{
	ShownClient.Reset();
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
		HeadingText->SetText(FText::Format(LOCTEXT("Order_HeadingTier", "ORDER   {0}"), DCUI::ClientTierLabel(GetWorld()->GetAuthGameMode<ADCGameMode>()->Config, Rt.Tier)));
	}
	PatienceBar->SetPercent(Front->PatienceMax > 0.f ? Front->Runtime.PatienceLeft / Front->PatienceMax : 0.f);
}

#undef LOCTEXT_NAMESPACE
