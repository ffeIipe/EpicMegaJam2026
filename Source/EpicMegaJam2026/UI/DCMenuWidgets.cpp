#include "UI/DCMenuWidgets.h"
#include "UI/DCWidgets.h"
#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Core/DCSettings.h"
#include "Data/PDA_GameConfig.h"
#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Input/CommonUIInputTypes.h"
#include "Components/SizeBox.h"
#include "Containers/Ticker.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
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

/// ---------------------------------------------------------------- Supplier

void UDCSupplierWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Inventory = GM->Inventory;
	Unlocks = GM->Unlocks;
	Config = GM->Config;
	GM->Economy->OnMoneyChanged.AddDynamic(this, &UDCSupplierWidget::OnMoneyChanged);
	Unlocks->OnUnlockPurchased.AddDynamic(this, &UDCSupplierWidget::OnUnlockPurchased);
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("PrevTab"), false, FSimpleDelegate::CreateUObject(this, &UDCSupplierWidget::CycleTab, -1)));
	RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action("NextTab"), true, FSimpleDelegate::CreateUObject(this, &UDCSupplierWidget::CycleTab, 1)));

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
	Refresh(); // what is on sale changes with the unlocks
}

void UDCSupplierWidget::CycleTab(int32 Dir)
{
	const int32 Num = Tabs ? Tabs->GetChildrenCount() : 0;
	for (int32 i = 0; i < Num; ++i)
	{
		if (Cast<UDCButton>(Tabs->GetChildAt(i))->Payload == SelectedGenre)
		{
			SelectedGenre = Cast<UDCButton>(Tabs->GetChildAt((i + Dir + Num) % Num))->Payload;
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
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		List->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 4.f));
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* InfoSlot = H->AddChildToHorizontalBox(Info);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		InfoSlot->SetVerticalAlignment(VAlign_Center);
		DCUI::Text(WidgetTree, Info, Tape->Title.ToString());
		DCUI::SkillStrip(WidgetTree, Info, Tape->Skills);
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, DCUI::Stars(Tape->Rating), UDCText_Stars::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* PriceSlot = H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("$%d"), Inventory->GetTapePrice(Row)), UDCText_Heading::StaticClass()));
		PriceSlot->SetVerticalAlignment(VAlign_Center);
		PriceSlot->SetPadding(FMargin(16.f, 0.f));
		for (const int32 Count : { 1, 5 })
		{
			UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, FString::Printf(TEXT("x%d"), Count), Row);
			if (Count == 1)
			{
				Btn->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::HandleBuyOne);
			}
			else
			{
				Btn->OnPayloadClicked.AddDynamic(this, &UDCSupplierWidget::HandleBuyFive);
			}
			UHorizontalBoxSlot* BtnSlot = H->AddChildToHorizontalBox(Btn);
			BtnSlot->SetVerticalAlignment(VAlign_Center);
			BtnSlot->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
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
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	Unlocks = GM->Unlocks;
	Config = GM->Config;
	GM->Economy->OnMoneyChanged.AddDynamic(this, &UDCComputerWidget::OnMoneyChanged);
	Unlocks->OnUnlockPurchased.AddDynamic(this, &UDCComputerWidget::OnUnlockPurchased);

	UVerticalBox* V = DCUI::Panel(WidgetTree, Center, Middle, FVector2D::ZeroVector);
	DCUI::Header(WidgetTree, V, TEXT("COMPUTER"), TEXT("Genres, VHS player slots and star licenses"));
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
	Refresh();
}

void UDCComputerWidget::Refresh()
{
	if (!Unlocks || !Config || !Config->UnlockTable)
	{
		return;
	}
	const float Offset = Scroll->GetScrollOffset(); // a purchase rebuilds the list: keep the place
	List->ClearChildren();
	Buttons.Reset();
	for (const TPair<FName, uint8*>& P : Config->UnlockTable->GetRowMap())
	{
		const FUnlockData* Row = reinterpret_cast<const FUnlockData*>(P.Value);
		const bool bOwned = Unlocks->IsOwned(P.Key);
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		List->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 6.f));
		UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
		UHorizontalBoxSlot* InfoSlot = H->AddChildToHorizontalBox(Info);
		InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
			DCUI::Text(WidgetTree, Info, TEXT("Requires: ") + FString::Join(Needs, TEXT(", ")), UDCText_Accent::StaticClass())->SetWrapTextAt(440.f);
		}
		H->AddChildToHorizontalBox(DCUI::Text(WidgetTree, nullptr, FString::Printf(TEXT("$%d"), Row->Price), UDCText_Heading::StaticClass()))->SetVerticalAlignment(VAlign_Center);
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, bOwned ? TEXT("OWNED") : TEXT("BUY"), P.Key);
		UHorizontalBoxSlot* BtnSlot = H->AddChildToHorizontalBox(Btn);
		BtnSlot->SetVerticalAlignment(VAlign_Center);
		BtnSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
		Btn->OnPayloadClicked.AddDynamic(this, &UDCComputerWidget::HandleBuy);
		Buttons.Add({ Btn, P.Key });
	}
	UpdateAffordability();
	Scroll->SetScrollOffset(Offset);
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
	void StepValue(float& Value, float Delta, float Min, float Max)
	{
		Value = FMath::Clamp(FMath::RoundToFloat((Value + Delta) * 10.f) / 10.f, Min, Max);
	}
}

void UDCSettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
	WidgetTree->RootWidget = V;
	auto AddRow = [this, V](const FString& Label)
	{
		UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
		V->AddChildToVerticalBox(H)->SetPadding(FMargin(0.f, 4.f));
		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
		LabelBox->SetMinDesiredWidth(300.f);
		LabelBox->AddChild(DCUI::Text(WidgetTree, nullptr, Label));
		H->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);
		return H;
	};

	DCUI::Text(WidgetTree, V, TEXT("CONTROLS"), UDCText_Heading::StaticClass());
	for (int32 i = 0; i < (int32)EDCAction::Count; ++i)
	{
		UHorizontalBox* H = AddRow(UDCSettings::ActionLabel((EDCAction)i));
		UDCButton* Btn = DCUI::Button(WidgetTree, nullptr, TEXT(""), FName(*FString::FromInt(i)));
		USizeBox* BtnBox = WidgetTree->ConstructWidget<USizeBox>();
		BtnBox->SetMinDesiredWidth(240.f);
		BtnBox->AddChild(Btn);
		H->AddChildToHorizontalBox(BtnBox);
		Btn->OnPayloadClicked.AddDynamic(this, &UDCSettingsWidget::HandleRebind);
		KeyButtons.Add(Btn);
	}

	UCommonTextBlock* Header = DCUI::Text(WidgetTree, V, TEXT("SETTINGS"), UDCText_Heading::StaticClass());
	Header->SetMargin(FMargin(0.f, 14.f, 0.f, 0.f));
	// Value-plus-arrows control: [◀] value [▶] (Press Start 2P has the glyphs)
	auto AddStepper = [this, &AddRow](const TCHAR* Label, FName Down, FName Up)
	{
		UHorizontalBox* H = AddRow(Label);
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
	VolumeText = AddStepper(TEXT("Volume"), "VolDown", "VolUp");
	SensitivityText = AddStepper(TEXT("Mouse sensitivity"), "SensDown", "SensUp");
	Refresh();
}

void UDCSettingsWidget::Refresh()
{
	UDCSettings* S = UDCSettings::Get();
	for (int32 i = 0; i < KeyButtons.Num(); ++i)
	{
		KeyButtons[i]->Label->SetText(i == CapturingAction ? FText::FromString(TEXT("PRESS A KEY...")) : S->Key((EDCAction)i).GetDisplayName());
	}
	VolumeText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S->MasterVolume * 100.f))));
	SensitivityText->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), S->MouseSensitivityScale)));
}

void UDCSettingsWidget::HandleStep(FName Id)
{
	UDCSettings* S = UDCSettings::Get();
	const float Dir = Id == "VolUp" || Id == "SensUp" ? 0.1f : -0.1f;
	if (Id == "VolUp" || Id == "VolDown")
	{
		StepValue(S->MasterVolume, Dir, 0.f, 1.f);
	}
	else
	{
		StepValue(S->MouseSensitivityScale, Dir, 0.1f, 3.f);
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
	if (VolumeText)
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

void UDCPauseWidget::HandleQuit(FName Unused)
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
