#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Data/PDA_GameConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Core/DCClient.h"

static FString GenreName(EGenre G) { return StaticEnum<EGenre>()->GetNameStringByValue((int64)G); }
static EFilmSlot SlotFromName(FName N) { return (EFilmSlot)StaticEnum<EFilmSlot>()->GetValueByName(N); }

static UTextBlock* AddText(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Text)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
	T->SetText(FText::FromString(Text));
	Parent->AddChild(T);
	return T;
}

// ---------------------------------------------------------------- Payload button

void UDCPayloadButton::Init(FName InPayload)
{
	Payload = InPayload;
	OnClicked.AddDynamic(this, &UDCPayloadButton::HandleClicked);
}

// ---------------------------------------------------------------- Inventory widget

void UDCInventoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = Root;
	Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
	Root->AddChild(Tabs);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetHeightOverride(260.f);
	Root->AddChild(Size);
	List = WidgetTree->ConstructWidget<UScrollBox>();
	Size->AddChild(List);
}

void UDCInventoryWidget::Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig)
{
	Inventory = InInventory;
	Config = InConfig;
	Refresh();
}

void UDCInventoryWidget::OnTabClicked(FName GenreName)
{
	SelectedGenre = (EGenre)StaticEnum<EGenre>()->GetValueByName(GenreName);
	Refresh();
}

void UDCInventoryWidget::Refresh()
{
	if (!Inventory || !Config || !Config->GenreTable || !Tabs)
	{
		return;
	}
	Tabs->ClearChildren();
	List->ClearChildren();

	for (const TPair<FName, uint8*>& Pair : Config->GenreTable->GetRowMap())
	{
		const FGenreData* Row = reinterpret_cast<const FGenreData*>(Pair.Value);
		UDCPayloadButton* Btn = WidgetTree->ConstructWidget<UDCPayloadButton>();
		Btn->Init(FName(*GenreName(Row->Genre)));
		Btn->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTabClicked);
		Btn->SetBackgroundColor(Row->Color);
		AddText(WidgetTree, Btn, Row->DisplayName.ToString());
		Tabs->AddChild(Btn);
	}

	for (const FName& TapeRow : Inventory->GetOwnedVHSByGenre(SelectedGenre))
	{
		FVHSData Tape;
		if (!Inventory->GetVHSData(TapeRow, Tape))
		{
			continue;
		}
		UDCPayloadButton* Btn = WidgetTree->ConstructWidget<UDCPayloadButton>();
		Btn->Init(TapeRow);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCInventoryWidget::OnTapeClicked);
		AddText(WidgetTree, Btn, FString::Printf(TEXT("%s  %s"), *Tape.Title.ToString(), *FString::ChrN(Tape.Rating, TEXT('*'))));
		List->AddChild(Btn);
	}
}

// ---------------------------------------------------------------- Film crafting widget

UDCPayloadButton* UDCFilmCraftingWidget::MakeButton(const FString& Label, FName Payload, UPanelWidget* Parent, UTextBlock** OutText)
{
	UDCPayloadButton* Btn = WidgetTree->ConstructWidget<UDCPayloadButton>();
	Btn->Init(Payload);
	UTextBlock* T = AddText(WidgetTree, Btn, Label);
	if (OutText)
	{
		*OutText = T;
	}
	Parent->AddChild(Btn);
	return Btn;
}

void UDCFilmCraftingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Panel->SetPadding(FMargin(16.f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(V);
	AddText(WidgetTree, V, TEXT("DIRECTOR'S CUT"));

	for (EFilmSlot S : {EFilmSlot::Intro, EFilmSlot::Middle, EFilmSlot::Ending})
	{
		const FName SlotName(*StaticEnum<EFilmSlot>()->GetNameStringByValue((int64)S));
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		V->AddChild(Row);
		UTextBlock* T = nullptr;
		MakeButton(SlotName.ToString(), SlotName, Row, &T)->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnSlotClicked);
		SlotTexts.Add(S, AddText(WidgetTree, Row, TEXT("(empty)")));
		MakeButton(TEXT("X"), SlotName, Row)->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnClearClicked);
	}

	Picker = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	Picker->OnVHSPicked.AddDynamic(this, &UDCFilmCraftingWidget::OnPicked);
	V->AddChild(Picker);

	TitleBox = WidgetTree->ConstructWidget<UEditableTextBox>();
	TitleBox->SetHintText(FText::FromString(TEXT("Film title")));
	V->AddChild(TitleBox);

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	V->AddChild(Buttons);
	ConfirmButton = MakeButton(TEXT("Confirm"), NAME_None, Buttons);
	ConfirmButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnConfirm);
	DiscardButton = MakeButton(TEXT("Discard"), NAME_None, Buttons);
	DiscardButton->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnDiscard);
	MakeButton(TEXT("Close"), NAME_None, Buttons)->OnPayloadClicked.AddDynamic(this, &UDCFilmCraftingWidget::OnBack);

	HeldText = AddText(WidgetTree, V, TEXT(""));
	StatusText = AddText(WidgetTree, V, TEXT(""));
}

void UDCFilmCraftingWidget::Setup(UAC_Inventory* InInventory, UAC_FilmCrafting* InCrafting, UPDA_GameConfig* InConfig)
{
	Inventory = InInventory;
	Crafting = InCrafting;
	Config = InConfig;
	Picker->Setup(Inventory, Config);
	Inventory->OnInventoryChanged.AddDynamic(Picker, &UDCInventoryWidget::Refresh);
	Crafting->OnFilmStateChanged.AddDynamic(this, &UDCFilmCraftingWidget::OnFilmStateChanged);
	Refresh();
}

void UDCFilmCraftingWidget::Refresh()
{
	if (!Crafting || !Inventory)
	{
		return;
	}
	for (const TPair<EFilmSlot, TObjectPtr<UTextBlock>>& Pair : SlotTexts)
	{
		const FName* Row = Crafting->Slots.Find(Pair.Key);
		FVHSData Tape;
		const bool bHas = Row && Inventory->GetVHSData(*Row, Tape);
		Pair.Value->SetText(bHas ? Tape.Title : FText::FromString(TEXT("(empty)")));
	}
	TitleBox->SetIsEnabled(Crafting->AreSlotsFull()); // title only editable with all 3 slots filled
	ConfirmButton->SetIsEnabled(Crafting->CanConfirm());
	DiscardButton->SetIsEnabled(Crafting->HeldFilms.Num() > 0);
	HeldText->SetText(Crafting->HeldFilms.Num() > 0
		? FText::FromString(FString::Printf(TEXT("In hand: %s"), *Crafting->HeldFilms[0].Title.ToString()))
		: FText::GetEmpty());
}

void UDCFilmCraftingWidget::OnSlotClicked(FName SlotName)
{
	ActiveSlot = SlotFromName(SlotName);
	StatusText->SetText(FText::FromString(FString::Printf(TEXT("Pick a tape for %s"), *SlotName.ToString())));
}

void UDCFilmCraftingWidget::OnClearClicked(FName SlotName)
{
	Crafting->ClearSlot(SlotFromName(SlotName));
}

void UDCFilmCraftingWidget::OnPicked(FName VHSRow)
{
	const bool bOk = Crafting->SetSlot(ActiveSlot, VHSRow);
	StatusText->SetText(bOk ? FText::GetEmpty() : FText::FromString(TEXT("That tape is already in another slot")));
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
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.8f));
	Panel->SetPadding(FMargin(16.f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.5f, 1.f));
	PanelSlot->SetAlignment(FVector2D(0.5f, 1.f));
	PanelSlot->SetPosition(FVector2D(0.f, -60.f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(V);
	LineText = AddText(WidgetTree, V, TEXT(""));
	LineText->SetAutoWrapText(true);
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(600.f);
	V->RemoveChild(LineText);
	Size->AddChild(LineText);
	V->AddChild(Size);

	UDCPayloadButton* Btn = WidgetTree->ConstructWidget<UDCPayloadButton>();
	Btn->Init(NAME_None);
	Btn->OnPayloadClicked.AddDynamic(this, &UDCDialogueWidget::HandleButton);
	ButtonText = AddText(WidgetTree, Btn, TEXT(""));
	V->AddChild(Btn);
}

void UDCDialogueWidget::Show(const FText& Line, const FText& ButtonLabel)
{
	LineText->SetText(Line);
	ButtonText->SetText(ButtonLabel);
}

// ---------------------------------------------------------------- Order panel

void UDCOrderPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	PanelRoot = Panel;
	Panel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Panel->SetPadding(FMargin(12.f));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(0.f, 0.f));
	PanelSlot->SetPosition(FVector2D(20.f, 20.f));
	PanelSlot->SetAutoSize(true);

	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(V);
	AddText(WidgetTree, V, TEXT("ORDER"));
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
	Size->SetWidthOverride(360.f);
	V->AddChild(Size);
	OrderText = WidgetTree->ConstructWidget<UTextBlock>();
	OrderText->SetAutoWrapText(true);
	Size->AddChild(OrderText);
	PatienceBar = WidgetTree->ConstructWidget<UProgressBar>();
	V->AddChild(PatienceBar);
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
