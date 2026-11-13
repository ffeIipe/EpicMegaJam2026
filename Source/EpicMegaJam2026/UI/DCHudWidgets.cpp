#include "UI/DCHudWidgets.h"
#include "Core/DCGameMode.h"
#include "Core/DCPlayerController.h"
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
	GoalBar = DCUI::Bar(WidgetTree, V, 260.f);
	Cast<UVerticalBoxSlot>(GoalBar->GetParent()->Slot)->SetPadding(FMargin(0.f, 10.f, 0.f, 4.f));
	GoalText = DCUI::Text(WidgetTree, V, TEXT(""));
	DayText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Accent::StaticClass());
	ClockText = DCUI::Text(WidgetTree, V, TEXT(""));
	RentText = DCUI::Text(WidgetTree, V, TEXT(""));
	HeldText = DCUI::Text(WidgetTree, V, TEXT(""), UDCText_Accent::StaticClass());

	// Crosshair with the interaction prompt ("[CLICK] Talk to the customer") under it
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
	Cap->SetContent(DCUI::Text(WidgetTree, nullptr, TEXT("CLICK"), UDCText_Heading::StaticClass()));
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

	// Hotbar, bottom center: the tapes picked from the shelves (Minecraft style)
	const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
	const int32 HotbarSize = GM && GM->Config ? GM->Config->HotbarSize : 3;
	UHorizontalBox* Tapes = WidgetTree->ConstructWidget<UHorizontalBox>();
	UCanvasPanelSlot* TS = Cast<UCanvasPanel>(WidgetTree->RootWidget)->AddChildToCanvas(Tapes);
	TS->SetAnchors(FAnchors(0.5f, 1.f));
	TS->SetAlignment(FVector2D(0.5f, 1.f));
	TS->SetPosition(FVector2D(0.f, -24.f));
	TS->SetAutoSize(true);
	for (int32 i = 0; i < HotbarSize; ++i)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
		Box->SetWidthOverride(110.f);
		Box->SetHeightOverride(110.f);
		Tapes->AddChildToHorizontalBox(Box)->SetPadding(FMargin(4.f, 0.f));
		UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(); // the selected slot lights up in lime
		Frame->SetPadding(FMargin(4.f));
		Box->AddChild(Frame);
		UOverlay* Stack = WidgetTree->ConstructWidget<UOverlay>();
		Frame->SetContent(Stack);
		UImage* Cover = WidgetTree->ConstructWidget<UImage>();
		Cover->SetColorAndOpacity(DCPalette::NavyLight);
		Stack->AddChild(Cover);
		UCommonTextBlock* Label = DCUI::Text(WidgetTree, Stack, FString::FromInt(i + 1));
		Label->SetWrapTextAt(92.f);
		Label->SetJustification(ETextJustify::Center);
		UOverlaySlot* LabelSlot = Cast<UOverlaySlot>(Label->Slot);
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		HotbarFrames.Add(Frame);
		HotbarCovers.Add(Cover);
		HotbarTexts.Add(Label);
	}
	ShownHotbar.Init(NAME_None, HotbarSize);
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
	// Animated counter: lime while rising, red while falling
	const bool bMoving = !FMath::IsNearlyEqual(DisplayedMoney, (float)Money, 0.5f);
	const float Diff = Money - DisplayedMoney;
	const float Step = FMath::Max(40.f, FMath::Abs(Diff) * 3.f) * InDeltaTime;
	DisplayedMoney = bMoving ? DisplayedMoney + FMath::Sign(Diff) * FMath::Min(Step, FMath::Abs(Diff)) : (float)Money;
	MoneyText->SetText(FText::FromString(FString::Printf(TEXT("$ %d"), FMath::RoundToInt(DisplayedMoney))));
	MoneyText->SetColorAndOpacity(FSlateColor(!bMoving ? DCPalette::Beige : (Diff > 0 ? DCPalette::Lime : DCPalette::Red)));

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
	UpdateHotbar(*GM);
	const ADCPlayerController* PC = Cast<ADCPlayerController>(GetOwningPlayer());
	const FText PromptLabel = PC ? PC->GetInteractPrompt() : FText::GetEmpty();
	PromptText->SetText(PromptLabel);
	Prompt->SetVisibility(PromptLabel.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UDCHUDWidget::UpdateHotbar(const ADCGameMode& GM)
{
	const UAC_Inventory* Inventory = GM.Inventory;
	for (int32 i = 0; i < HotbarCovers.Num(); ++i)
	{
		HotbarFrames[i]->SetBrushColor(Inventory->SelectedHotbar == i ? DCPalette::Lime : DCPalette::Navy);
		const FName Row = Inventory->Hotbar.IsValidIndex(i) ? Inventory->Hotbar[i] : NAME_None;
		if (ShownHotbar[i] == Row)
		{
			continue; // unchanged: don't re-request the cover texture every frame
		}
		ShownHotbar[i] = Row;

		FVHSData Tape;
		const bool bHas = !Row.IsNone() && Inventory->GetVHSData(Row, Tape);
		UImage* Cover = HotbarCovers[i];
		if (bHas && !Tape.Cover.IsNull())
		{
			Cover->SetColorAndOpacity(FLinearColor::White);
			Cover->SetBrushFromSoftTexture(Tape.Cover);
		}
		else
		{
			FLinearColor Color = DCPalette::NavyLight;
			if (bHas && GM.Config->GenreTable)
			{
				for (const TPair<FName, uint8*>& G : GM.Config->GenreTable->GetRowMap())
				{
					const FGenreData* Genre = reinterpret_cast<const FGenreData*>(G.Value);
					if (Genre->Genre == Tape.PrimaryGenre)
					{
						Color = Genre->Color;
					}
				}
			}
			Cover->SetBrush(FSlateBrush());
			Cover->SetColorAndOpacity(Color);
		}
		HotbarTexts[i]->SetText(bHas ? Tape.Title : FText::AsNumber(i + 1));
	}
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
}

void UDCReviewToastWidget::Hide()
{
	Panel->SetVisibility(ESlateVisibility::Collapsed);
}

void UDCReviewToastWidget::ShowReview(const FReview& Review, int32 Stars, float DurationSec)
{
	TitleText->SetText(Review.FilmTitle);
	StarsText->SetText(FText::FromString(DCUI::Stars(Stars)));
	LineText->SetText(Review.Line);
	MoneyText->SetText(FText::FromString(Review.MoneyDelta == 0 ? FString() : FString::Printf(TEXT("%+d"), Review.MoneyDelta)));
	MoneyText->SetColorAndOpacity(FSlateColor(Review.MoneyDelta < 0 ? DCPalette::Red : DCPalette::Lime));
	Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
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
}

void UDCWorldLabelWidget::SetLabel(const FText& Text)
{
	if (LabelText)
	{
		LabelText->SetText(Text);
		Plate->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}
