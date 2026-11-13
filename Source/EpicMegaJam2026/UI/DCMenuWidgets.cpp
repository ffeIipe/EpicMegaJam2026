#include "UI/DCMenuWidgets.h"
#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Data/PDA_GameConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	const FAnchors Center(0.5f, 0.5f);
	const FVector2D Middle(0.5f, 0.5f);

	// Stacked menu buttons, stretched to the panel width
	UDCButton* MenuButton(UWidgetTree* Tree, UVerticalBox* Parent, const TCHAR* Label)
	{
		UDCButton* Btn = DCUI::Button(Tree, Parent, Label);
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

// ---------------------------------------------------------------- Supplier

void UDCSupplierWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Inventory = GM->Inventory;
	DayNight = GM->DayNightCycle;
	Config = GM->Config;
	GM->Economy->OnMoneyChanged.AddDynamic(this, &UDCSupplierWidget::OnMoneyChanged);
	Inventory->OnInventoryChanged.AddDynamic(this, &UDCSupplierWidget::OnInventoryChanged);

	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("SUPPLIER"), TEXT("Tape catalogs for the store"));
	List = WidgetTree->ConstructWidget<UVerticalBox>();
	DCUI::Border(WidgetTree, V, UDCBorder_Inset::StaticClass(), FMargin(12.f))->SetContent(List);
}

void UDCSupplierWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	Refresh(); // catalogs unlock by day
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
		List->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 6.f));
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* InfoSlot = H->AddChildToHorizontalBox(Info);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetPadding(FMargin(0.f, 0.f, 24.f, 0.f));
		DCUI::Text(WidgetTree, Info, Row->DisplayName.ToString().ToUpper(), UDCText_Heading::StaticClass());
		DCUI::Text(WidgetTree, Info, Row->Description.ToString())->SetWrapTextAt(420.f);
		DCUI::Text(WidgetTree, Info, FString::Printf(TEXT("%d tapes"), Row->VHSRows.Num()), UDCText_Accent::StaticClass());
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("$%d"), Row->Price), UDCText_Heading::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, bOwned ? TEXT("OWNED") : TEXT("BUY"), P.Key);
		UHorizontalBoxSlot* BtnSlot = H->AddChildToHorizontalBox(Btn);
		BtnSlot->SetVerticalAlignment(VAlign_Center);
		BtnSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
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
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("VHS INVENTORY"));
	List = WidgetTree->ConstructWidget<UDCInventoryWidget>();
	V->AddChild(List);
	List->Setup(GM->Inventory, GM->Config, false);
	GM->Inventory->OnInventoryChanged.AddDynamic(List, &UDCInventoryWidget::Refresh);
	List->OnVHSPicked.AddDynamic(this, &UDCShelfWidget::OnTapePicked);
}

void UDCShelfWidget::OnTapePicked(FName VHSRow)
{
	GetWorld()->GetAuthGameMode<ADCGameMode>()->Inventory->TakeTape(VHSRow); // refreshes the list: the tape leaves the shelf
}

void UDCShelfWidget::ShowGenre(EGenre Genre)
{
	List->SetGenre(Genre);
}

// ---------------------------------------------------------------- Tutorial sign

void UDCTutorialWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCommonBorder* Border = nullptr;
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -140.f), &Border, UDCBorder_Header::StaticClass());
	Border->SetPadding(DCUI::HeaderPadding);
	Panel = Border;
	HintText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Heading::StaticClass());
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

UDCMainMenuWidget::UDCMainMenuWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	bIsBackHandler = false;
}

void UDCMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("DIRECTOR'S CUT")); // TODO(DESIGN): final game name
	MenuButton(WidgetTree, V, TEXT("PLAY"))->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandlePlay);
	MenuButton(WidgetTree, V, TEXT("QUIT"))->OnPayloadClicked.AddDynamic(this, &UDCMainMenuWidget::HandleQuit);
}

void UDCMainMenuWidget::HandleQuit(FName Unused)
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

UDCPauseWidget::UDCPauseWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
}

void UDCPauseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("PAUSED"));
	MenuButton(WidgetTree, V, TEXT("CONTINUE"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleContinue);
	MenuButton(WidgetTree, V, TEXT("RESTART"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleRestart);
	MenuButton(WidgetTree, V, TEXT("QUIT"))->OnPayloadClicked.AddDynamic(this, &UDCPauseWidget::HandleQuit);
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
