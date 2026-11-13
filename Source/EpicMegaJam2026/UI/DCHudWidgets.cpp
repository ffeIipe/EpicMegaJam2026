#include "UI/DCHudWidgets.h"
#include "UI/DCWidgets.h"
#include "Core/DCGameMode.h"
#include "Core/DCPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Kismet/GameplayStatics.h"

static UTextBlock* HudAddText(UWidgetTree* Tree, UPanelWidget* Parent, const FString& Text)
{
	UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
	T->SetText(FText::FromString(Text));
	Parent->AddChild(T);
	return T;
}

static void HudSetFontSize(UTextBlock* T, int32 Size)
{
	FSlateFontInfo F = T->GetFont();
	F.Size = Size;
	T->SetFont(F);
}

// Canvas + dark border anchored on the screen; returns the border's content box
static UVerticalBox* HudMakePanel(UWidgetTree* Tree, FAnchors Anchors, FVector2D Alignment, FVector2D Position, UBorder** OutBorder = nullptr)
{
	UCanvasPanel* Canvas = Tree->ConstructWidget<UCanvasPanel>();
	Tree->RootWidget = Canvas;
	UBorder* Border = Tree->ConstructWidget<UBorder>();
	Border->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	Border->SetPadding(FMargin(12.f));
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

// ---------------------------------------------------------------- HUD

void UDCHUDWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = HudMakePanel(WidgetTree, FAnchors(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-20.f, 20.f));
	MoneyText = HudAddText(WidgetTree, V, TEXT("$0"));
	HudSetFontSize(MoneyText, 28);
	USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>();
	BarSize->SetWidthOverride(260.f);
	V->AddChild(BarSize);
	GoalBar = WidgetTree->ConstructWidget<UProgressBar>();
	BarSize->AddChild(GoalBar);
	GoalText = HudAddText(WidgetTree, V, TEXT(""));
	DayText = HudAddText(WidgetTree, V, TEXT(""));
	ClockText = HudAddText(WidgetTree, V, TEXT(""));
	RentText = HudAddText(WidgetTree, V, TEXT(""));
	HeldText = HudAddText(WidgetTree, V, TEXT(""));
	UDCPayloadButton* Btn = WidgetTree->ConstructWidget<UDCPayloadButton>();
	Btn->Init(NAME_None);
	Btn->OnPayloadClicked.AddDynamic(this, &UDCHUDWidget::OnZoneButton);
	ZoneText = HudAddText(WidgetTree, Btn, TEXT(""));
	V->AddChild(Btn);
}

void UDCHUDWidget::OnZoneButton(FName Unused)
{
	if (ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer()))
	{
		PC->ToggleZone();
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
	// Animated counter: green while rising, red while falling
	const bool bMoving = !FMath::IsNearlyEqual(DisplayedMoney, (float)Money, 0.5f);
	const float Diff = Money - DisplayedMoney;
	const float Step = FMath::Max(40.f, FMath::Abs(Diff) * 3.f) * InDeltaTime;
	DisplayedMoney = bMoving ? DisplayedMoney + FMath::Sign(Diff) * FMath::Min(Step, FMath::Abs(Diff)) : (float)Money;
	MoneyText->SetText(FText::FromString(FString::Printf(TEXT("$%d"), FMath::RoundToInt(DisplayedMoney))));
	MoneyText->SetColorAndOpacity(FSlateColor(!bMoving ? FLinearColor::White : (Diff > 0 ? FLinearColor::Green : FLinearColor::Red)));

	const int32 Goal = GM->Config->MovieBudgetGoal;
	GoalBar->SetPercent(Goal > 0 ? FMath::Clamp((float)Money / Goal, 0.f, 1.f) : 0.f);
	GoalText->SetText(FText::FromString(FString::Printf(TEXT("Goal: $%d"), Goal)));
	DayText->SetText(FText::FromString(FString::Printf(TEXT("Day %d - %s"), GM->DayNightCycle->DayIndex,
		GM->DayNightCycle->CurrentPhase == EPhase::Day ? TEXT("Day") : TEXT("Night"))));
	ClockText->SetText(GM->DayNightCycle->GetClockText());
	RentText->SetText(FText::FromString(FString::Printf(TEXT("Rent tonight: $%d"), GM->Economy->GetRentForDay(GM->DayNightCycle->DayIndex))));
	HeldText->SetText(GM->FilmCrafting->HeldFilms.Num() > 0
		? FText::FromString(FString::Printf(TEXT("In hand: %s"), *GM->FilmCrafting->HeldFilms[0].Title.ToString()))
		: FText::GetEmpty());
	const ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer());
	ZoneText->SetText(FText::FromString(PC && PC->CurrentZone == EZone::BackRoom ? TEXT("Counter [Tab]") : TEXT("Back room [Tab]")));
}

// ---------------------------------------------------------------- Review toast

void UDCReviewToastWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UBorder* Border = nullptr;
	UVerticalBox* V = HudMakePanel(WidgetTree, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f), &Border);
	Panel = Border;
	HudAddText(WidgetTree, V, TEXT("ReelTalk.net"));
	TitleText = HudAddText(WidgetTree, V, TEXT(""));
	StarsText = HudAddText(WidgetTree, V, TEXT(""));
	LineText = HudAddText(WidgetTree, V, TEXT(""));
	MoneyText = HudAddText(WidgetTree, V, TEXT(""));
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCReviewToastWidget::Hide()
{
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCReviewToastWidget::ShowReview(const FReview& Review, int32 Stars, float DurationSec)
{
	TitleText->SetText(Review.FilmTitle);
	StarsText->SetText(FText::FromString(FString::ChrN(FMath::Clamp(Stars, 0, 5), TEXT('*'))));
	LineText->SetText(Review.Line);
	MoneyText->SetText(FText::FromString(Review.MoneyDelta == 0 ? FString() : FString::Printf(TEXT("%+d"), Review.MoneyDelta)));
	MoneyText->SetColorAndOpacity(FSlateColor(Review.MoneyDelta < 0 ? FLinearColor::Red : FLinearColor::Green));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	GetWorld()->GetTimerManager().SetTimer(HideTimer, this, &UDCReviewToastWidget::Hide, DurationSec);
}

// ---------------------------------------------------------------- End screen

void UDCEndScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	UVerticalBox* V = HudMakePanel(WidgetTree, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	TitleBlock = HudAddText(WidgetTree, V, TEXT(""));
	HudSetFontSize(TitleBlock, 32);
	DayBlock = HudAddText(WidgetTree, V, TEXT(""));
	UDCPayloadButton* Btn = WidgetTree->ConstructWidget<UDCPayloadButton>();
	Btn->Init(NAME_None);
	Btn->OnPayloadClicked.AddDynamic(this, &UDCEndScreenWidget::OnRestart);
	HudAddText(WidgetTree, Btn, TEXT("Restart"));
	V->AddChild(Btn);
}

void UDCEndScreenWidget::Show(int32 DayReached)
{
	TitleBlock->SetText(Title);
	DayBlock->SetText(FText::FromString(FString::Printf(TEXT("Day reached: %d"), DayReached)));
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDCEndScreenWidget::OnRestart(FName Unused)
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
}
