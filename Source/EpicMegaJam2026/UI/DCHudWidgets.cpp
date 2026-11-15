#include "UI/DCHudWidgets.h"
#include "Core/DCGameInstance.h"
#include "Core/DCGameMode.h"
#include "Core/DCPlayerController.h"
#include "Core/DCSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ProgressBar.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet/GameplayStatics.h"

// Shared money "punch": P goes 1 -> 0. A gain swells and jumps up; a loss swells, sinks and shakes sideways.
static void ApplyPunch(UWidget* W, float P, bool bGain, float Time)
{
	FWidgetTransform T;
	T.Scale = FVector2D(1.f + 0.45f * P);
	T.Translation = bGain ? FVector2D(0.f, -18.f * P) : FVector2D(FMath::Sin(Time * 70.f) * 7.f * P, 18.f * P);
	W->SetRenderTransform(T);
}

static float PunchFromDelta(int32 Delta)
{
	return FMath::Clamp(0.5f + FMath::Abs(Delta) / 300.f, 0.5f, 1.f);
}

// ---------------------------------------------------------------- HUD

UDCHUDWidget::UDCHUDWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bAutoActivate = true;
}

TOptional<FUIInputConfig> UDCHUDWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown);
}

void UDCHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Money box and day info, top right
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-24.f, 24.f));
	MoneyText = DCUI::Text(WidgetTree, V, TEXT("$0"), UDCText_Title::StaticClass());
	MoneyText->SetRenderTransformPivot(FVector2D(1.f, 0.5f));
	GoalBar = DCUI::Bar(WidgetTree, V, 260.f);
	Cast<UVerticalBoxSlot>(GoalBar->GetParent()->Slot)->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));
	GoalText = DCUI::Text(WidgetTree, V, TEXT(""));
	DayText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Accent::StaticClass());
	ClockText = DCUI::Text(WidgetTree, V, TEXT(""));
	RentText = DCUI::Text(WidgetTree, V, TEXT(""));
	HeldText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Accent::StaticClass());

	// Back-room hint, bottom right. The mouse is captured in game mode, so it's a hint for the key rather than a clickable button.
	UCommonBorder* HintBorder = nullptr;
	UVerticalBox* HintBox = DCUI::Panel(WidgetTree, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f), &HintBorder);
	BackRoomHint = HintBorder;
	UHorizontalBox* HintRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	HintBox->AddChild(HintRow);
	UCommonBorder* HintCap = DCUI::Border(WidgetTree, HintRow, UDCBorder_KeyCap::StaticClass(), FMargin(8.f, 4.f));
	BackRoomKeyText = DCUI::Text(WidgetTree, nullptr, TEXT("SPACE"), UDCText_Heading::StaticClass()); // set each tick from the rebindable key
	HintCap->SetContent(BackRoomKeyText);
	Cast<UHorizontalBoxSlot>(HintCap->Slot)->SetVerticalAlignment(VAlign_Center);
	UCommonTextBlock* HintLabel = DCUI::Text(WidgetTree, HintRow, TEXT("Back room"));
	Cast<UHorizontalBoxSlot>(HintLabel->Slot)->SetPadding(FMargin(10.f, 0.f, 4.f, 0.f));
	Cast<UHorizontalBoxSlot>(HintLabel->Slot)->SetVerticalAlignment(VAlign_Center);

	// Crosshair with the interaction prompt ("[E] Talk to the customer") under it
	UVerticalBox* Center = WidgetTree->ConstructWidget<UVerticalBox>();
	UCanvasPanelSlot* CS = Cast<UCanvasPanel>(WidgetTree->RootWidget)->AddChildToCanvas(Center);
	CS->SetAnchors(FAnchors(0.5f, 0.5f));
	CS->SetAlignment(FVector2D(0.5f, 0.f));
	CS->SetPosition(FVector2D(0.f, -8.f)); // puts the "+" glyph on the screen center
	CS->SetAutoSize(true);
	DCUI::Text(WidgetTree, Center, TEXT("+"), UDCText_Heading::StaticClass());
	UCommonBorder* PromptPlate = DCUI::Border(WidgetTree, Center, UDCBorder_Panel::StaticClass(), FMargin(14.f, 10.f));
	Cast<UVerticalBoxSlot>(PromptPlate->Slot)->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
	UHorizontalBox* H = WidgetTree->ConstructWidget<UHorizontalBox>();
	PromptPlate->SetContent(H);
	UCommonBorder* Cap = DCUI::Border(WidgetTree, H, UDCBorder_KeyCap::StaticClass(), FMargin(8.f, 4.f));
	PromptKeyText = DCUI::Text(WidgetTree, nullptr, TEXT("E"), UDCText_Heading::StaticClass()); // set each tick from the rebindable key
	Cap->SetContent(PromptKeyText);
	Cast<UHorizontalBoxSlot>(Cap->Slot)->SetVerticalAlignment(VAlign_Center);
	PromptText = DCUI::Text(WidgetTree, H, TEXT(""));
	UHorizontalBoxSlot* TextSlot = Cast<UHorizontalBoxSlot>(PromptText->Slot);
	TextSlot->SetPadding(FMargin(10.f, 0.f, 4.f, 0.f));
	TextSlot->SetVerticalAlignment(VAlign_Center);
	Prompt = PromptPlate;
	for (UWidget* W : Center->GetAllChildren())
	{
		Cast<UVerticalBoxSlot>(W->Slot)->SetHorizontalAlignment(HAlign_Center);
	}
}

void UDCHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
	if (!GM || !GM->Config)
	{
		return;
	}
	const int32 Money = GM->Economy->Money;
	if (DisplayedMoney < 0.f)
	{
		DisplayedMoney = Money;
	}
	if (LastMoney >= 0 && Money != LastMoney)
	{
		bPunchGain = Money > LastMoney;
		Punch = PunchFromDelta(Money - LastMoney);
	}
	LastMoney = Money;
	Punch = FMath::Max(0.f, Punch - InDeltaTime * 1.4f);
	ApplyPunch(MoneyText, Punch, bPunchGain, GetWorld()->GetTimeSeconds());
	// Animated counter: lime while rising, red while falling
	const bool bMoving = !FMath::IsNearlyEqual(DisplayedMoney, (float)Money, 0.5f);
	const float Diff = Money - DisplayedMoney;
	const float Step = FMath::Max(40.f, FMath::Abs(Diff) * 2.f) * InDeltaTime; // slower than before so the roll is readable
	DisplayedMoney = bMoving ? DisplayedMoney + FMath::Sign(Diff) * FMath::Min(Step, FMath::Abs(Diff)) : (float)Money;
	MoneyText->SetText(FText::FromString(FString::Printf(TEXT("$ %d"), FMath::RoundToInt(DisplayedMoney))));
	MoneyText->SetColorAndOpacity(FSlateColor(!bMoving ? DCPalette::Beige : (Diff > 0 ? DCPalette::Lime : DCPalette::Red)));

	const int32 Goal = GM->Config->MovieBudgetGoal;
	GoalBar->SetPercent(Goal > 0 ? FMath::Clamp((float)Money / Goal, 0.f, 1.f) : 0.f);
	GoalText->SetText(FText::FromString(FString::Printf(TEXT("Goal: $%d"), Goal)));
	DayText->SetText(FText::FromString(FString::Printf(TEXT("Day %d - %s"), GM->DayNightCycle->DayIndex,
		GM->DayNightCycle->CurrentPhase == EPhase::Day ? TEXT("Day") : TEXT("Night"))));
	ClockText->SetText(GM->DayNightCycle->bTutorialPaused
		? FText::Format(INVTEXT("{0} (Tutorial)"), GM->DayNightCycle->GetClockText())
		: GM->DayNightCycle->GetClockText());
	RentText->SetText(FText::FromString(FString::Printf(TEXT("Rent tonight: $%d"), GM->Economy->GetRentForDay(GM->DayNightCycle->DayIndex))));
	HeldText->SetText(GM->FilmCrafting->HeldFilms.Num() > 0
		? FText::FromString(FString::Printf(TEXT("In hand: %s"), *GM->FilmCrafting->HeldFilms[0].Title.ToString()))
		: FText::GetEmpty());
	const ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer());
	const FText PromptLabel = PC ? PC->GetInteractPrompt() : FText::GetEmpty();
	PromptText->SetText(PromptLabel);
	PromptKeyText->SetText(UDCSettings::Get()->Interact.GetDisplayName(false).ToUpper());
	Prompt->SetVisibility(PromptLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	BackRoomKeyText->SetText(UDCSettings::Get()->BackRoom.GetDisplayName(false).ToUpper());
	BackRoomHint->SetVisibility(PC && PC->CanGoToBackRoom() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------- Review toast

void UDCReviewToastWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCommonBorder* Border = nullptr;
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 24.f), &Border);
	Panel = Border;
	DCUI::Text(WidgetTree, V, TEXT("ReelTalk.net"), UDCText_Heading::StaticClass());
	TitleText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Accent::StaticClass());
	StarsText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Stars::StaticClass());
	LineText = DCUI::Text(WidgetTree, V, TEXT(""));
	LineText->SetWrapTextAt(520.f);
	MoneyText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Heading::StaticClass());
	Panel->SetVisibility(ESlateVisibility::Collapsed);
	Panel->SetRenderTransformPivot(FVector2D(0.5f, 0.f));
}

void UDCReviewToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Pop = FMath::Max(0.f, Pop - InDeltaTime * 2.f);
	// Bounce in: a good review pops up with a growing swell, a bad one slams down and shakes
	ApplyPunch(Panel, Pop * 0.7f, bPopGain, GetWorld()->GetTimeSeconds());
}

void UDCReviewToastWidget::Hide()
{
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCReviewToastWidget::ShowReview(const FReview& Review, int32 Stars, float DurationSec)
{
	TitleText->SetText(Review.FilmTitle);
	StarsText->SetText(FText::FromString(DCUI::Stars(Stars)));
	LineText->SetText(Review.FeedbackLine.IsEmpty() ? Review.Line : FText::Format(INVTEXT("{0}\n{1}"), Review.Line, Review.FeedbackLine));
	MoneyText->SetText(FText::FromString(Review.MoneyDelta == 0 ? FString() : FString::Printf(TEXT("%+d"), Review.MoneyDelta)));
	MoneyText->SetColorAndOpacity(FSlateColor(Review.MoneyDelta < 0 ? DCPalette::Red : DCPalette::Lime));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	Pop = 1.f;
	bPopGain = Review.MoneyDelta >= 0;
	GetWorld()->GetTimerManager().SetTimer(HideTimer, this, &UDCReviewToastWidget::Hide, DurationSec);
}

// ---------------------------------------------------------------- End screen

UDCEndScreenWidget::UDCEndScreenWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bPausesGame = true;
	bIsBackHandler = false; // only Restart leaves it
}

void UDCEndScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = DCUI::Panel(WidgetTree, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	TitleBlock = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Title::StaticClass());
	DayBlock = DCUI::Text(WidgetTree, V, TEXT(""));
	DayBlock->SetMargin(FMargin(0.f, 12.f));
	DCUI::Button(WidgetTree, V, TEXT("RESTART"))->OnPayloadClicked.AddDynamic(this, &UDCEndScreenWidget::OnRestart);
	UDCButton* MainMenuBtn = DCUI::Button(WidgetTree, V, TEXT("BACK TO MAIN MENU"));
	MainMenuBtn->OnPayloadClicked.AddDynamic(this, &UDCEndScreenWidget::OnMainMenu);
	Cast<UVerticalBoxSlot>(MainMenuBtn->Slot)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

void UDCEndScreenWidget::Show(int32 DayReached)
{
	TitleBlock->SetText(Title);
	DayBlock->SetText(FText::FromString(FString::Printf(TEXT("Day reached: %d"), DayReached)));
}

void UDCEndScreenWidget::OnRestart(FName Unused)
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}

void UDCEndScreenWidget::OnMainMenu(FName Unused)
{
	if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
	{
		GI->TravelToMainMenu();
		return;
	}
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Game/_MainGame/Level/MAP_MainMenu")));
}

// ---------------------------------------------------------------- World label

void UDCWorldLabelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UCommonBorder* Border = DCUI::Border(WidgetTree, nullptr, UDCBorder_Panel::StaticClass(), FMargin(16.f, 12.f));
	WidgetTree->RootWidget = Border;
	LabelText = DCUI::Text(WidgetTree, nullptr, TEXT(""), UDCText_Title::StaticClass());
	Border->SetContent(LabelText);
	Plate = Border;
	Plate->SetVisibility(ESlateVisibility::Collapsed);
	Plate->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

void UDCWorldLabelWidget::PunchMoney(int32 Delta)
{
	bPunchGain = Delta > 0;
	Punch = PunchFromDelta(Delta);
}

void UDCWorldLabelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (Punch <= 0.f)
	{
		return;
	}
	Punch = FMath::Max(0.f, Punch - InDeltaTime * 1.4f);
	ApplyPunch(Plate, Punch, bPunchGain, GetWorld()->GetTimeSeconds());
	LabelText->SetColorAndOpacity(FSlateColor(Punch > 0.f ? (bPunchGain ? DCPalette::Lime : DCPalette::Red) : DCPalette::Beige));
}

void UDCWorldLabelWidget::SetLabel(const FText& Text)
{
	if (LabelText)
	{
		LabelText->SetText(Text);
		Plate->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

// ---------------------------------------------------------------- Intro skip prompt

void UDCIntroSkipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* Box = DCUI::Panel(WidgetTree, FAnchors(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f));
	DCUI::Text(WidgetTree, Box, TEXT("Hold any key to skip"), UDCText_Body::StaticClass());
	ProgressBar = DCUI::Bar(WidgetTree, Box, 220.f);
	if (UVerticalBoxSlot* BarSlot = Cast<UVerticalBoxSlot>(ProgressBar->GetParent()->Slot))
	{
		BarSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
	}
	ProgressBar->SetPercent(0.f);
}

void UDCIntroSkipWidget::SetSkipProgress(float Progress)
{
	if (ProgressBar)
	{
		ProgressBar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f));
	}
}
