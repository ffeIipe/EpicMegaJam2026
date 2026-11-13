#include "UI/DCMenuWidgets.h"
#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Data/PDA_GameConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	UTextBlock* MenuAddText(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Text)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
		T->SetText(FText::FromString(Text));
		Parent->AddChild(T);
		return T;
	}

	// Canvas + dark border anchored on screen; returns the content box
	UVerticalBox* MenuMakePanel(UWidgetTree* Tree, FAnchors Anchors, FVector2D Alignment, FVector2D Position, UBorder** OutBorder = nullptr)
	{
		UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>();
		Tree->RootWidget = Canvas;
		UBorder* Border = Tree->ConstructWidget<UBorder>();
		Border->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.85f));
		Border->SetPadding(FMargin(20.f));
		UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Border);
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
		UVerticalBox* V = Tree->ConstructWidget<UVerticalBox>();
		Border->SetContent(V);
		if (OutBorder) *OutBorder = Border;
		return V;
	}

	UDCPayloadButton* MenuAddButton(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Label, FName Payload, UTextBlock** OutText = nullptr)
	{
		UDCPayloadButton* Btn = Tree->ConstructWidget<UDCPayloadButton>();
		Btn->Init(Payload);
		UTextBlock* T = MenuAddText(Tree, Btn, Label);
		if (OutText) *OutText = T;
		Parent->AddChild(Btn);
		return Btn;
	}

	const FAnchors Center(0.5f, 0.5f);
}

// ---------------------------------------------------------------- Day summary

void UDCDaySummaryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = MenuMakePanel(WidgetTree, Center, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	TitleText = MenuAddText(WidgetTree, V, TEXT(""));
	BodyText = MenuAddText(WidgetTree, V, TEXT(""));
	MenuAddButton(WidgetTree, V, TEXT("Open the store"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCDaySummaryWidget::HandleOpen);
}

void UDCDaySummaryWidget::Show(const FDaySummary& S)
{
	TitleText->SetText(FText::FromString(FString::Printf(TEXT("Day %d complete"), S.DayIndex)));
	BodyText->SetText(FText::FromString(FString::Printf(
		TEXT("Clients served: %d\nClients lost: %d\nLoved: %d   Liked: %d   Neutral: %d   Hated: %d\nMoney earned: $%d\nRent paid: $%d\nMoney: $%d / goal $%d"),
		S.ClientsServed, S.ClientsLost, S.Loved, S.Liked, S.Neutral, S.Hated, S.MoneyEarned, S.RentPaid, S.MoneyNow, S.Goal)));
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

// ---------------------------------------------------------------- Supplier

void UDCSupplierWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = MenuMakePanel(WidgetTree, Center, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	MenuAddText(WidgetTree, V, TEXT("SUPPLIER"));
	List = WidgetTree->ConstructWidget<UVerticalBox>();
	V->AddChild(List);
	MenuAddButton(WidgetTree, V, TEXT("Close"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::HandleClose);
}

void UDCSupplierWidget::Setup(UAC_Inventory* InInventory, UAC_Economy* InEconomy, UAC_DayNightCycle* InDayNight, UPDA_GameConfig* InConfig)
{
	Inventory = InInventory;
	Economy = InEconomy;
	DayNight = InDayNight;
	Config = InConfig;
	Economy->OnMoneyChanged.AddDynamic(this, &UDCSupplierWidget::OnMoneyChanged);
	Inventory->OnInventoryChanged.AddDynamic(this, &UDCSupplierWidget::OnInventoryChanged);
	Refresh();
}

void UDCSupplierWidget::Refresh()
{
	if (!Inventory || !Config || !Config->CatalogTable)
	{
		return;
	}
	List->ClearChildren();
	for (const TPair<FName, uint8*>& P : Config->CatalogTable->GetRowMap())
	{
		const FCatalogData* Row = reinterpret_cast<const FCatalogData*>(P.Value);
		if (Row->UnlockDay > DayNight->DayIndex)
		{
			continue; // not available yet
		}
		const bool bOwned = Inventory->OwnedCatalogs.Contains(P.Key);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		List->AddChild(H);
		MenuAddText(WidgetTree, H, FString::Printf(TEXT("%s - %s (%d tapes)  $%d   "), *Row->DisplayName.ToString(), *Row->Description.ToString(), Row->VHSRows.Num(), Row->Price));
		UDCPayloadButton* Btn = MenuAddButton(WidgetTree, H, bOwned ? TEXT("Owned") : TEXT("Buy"), P.Key);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::HandleBuy);
		Btn->SetIsEnabled(!bOwned && Inventory->CanBuyCatalog(P.Key)); // disabled when owned or too expensive
	}
}

void UDCSupplierWidget::HandleBuy(FName CatalogRow)
{
	Inventory->BuyCatalog(CatalogRow);
}

// ---------------------------------------------------------------- Shelf popup

void UDCShelfWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = MenuMakePanel(WidgetTree, Center, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	MenuAddText(WidgetTree, V, TEXT("SHELF"));
	List = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	V->AddChild(List);
	MenuAddButton(WidgetTree, V, TEXT("Close"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCShelfWidget::HandleClose);
}

void UDCShelfWidget::Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig)
{
	List->Setup(InInventory, InConfig);
	InInventory->OnInventoryChanged.AddDynamic(List, &UDCInventoryWidget::Refresh);
}

void UDCShelfWidget::ShowGenre(EGenre Genre)
{
	List->SetGenre(Genre);
}

// ---------------------------------------------------------------- Tutorial sign

void UDCTutorialWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Border = nullptr;
	UVerticalBox* V = MenuMakePanel(WidgetTree, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -140.f), &Border);
	Panel = Border;
	HintText = MenuAddText(WidgetTree, V, TEXT(""));
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCTutorialWidget::ShowHint(const FText& Text)
{
	HintText->SetText(Text);
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDCTutorialWidget::HideHint()
{
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------- Main menu / pause

void UDCMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = MenuMakePanel(WidgetTree, Center, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	MenuAddText(WidgetTree, V, TEXT("DIRECTOR'S CUT")); // TODO(DESIGN): final game name
	MenuAddButton(WidgetTree, V, TEXT("Play"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandlePlay);
	MenuAddButton(WidgetTree, V, TEXT("Quit"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleQuit);
}

void UDCMainMenuWidget::HandleQuit(FName Unused)
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UDCPauseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = MenuMakePanel(WidgetTree, Center, FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	MenuAddText(WidgetTree, V, TEXT("PAUSED"));
	MenuAddButton(WidgetTree, V, TEXT("Continue"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleContinue);
	MenuAddButton(WidgetTree, V, TEXT("Restart"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleRestart);
	MenuAddButton(WidgetTree, V, TEXT("Quit"), NAME_None)->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleQuit);
}

void UDCPauseWidget::HandleRestart(FName Unused)
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void UDCPauseWidget::HandleQuit(FName Unused)
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
