#include "UI/DCIntroWidget.h"
#include "Core/DCAudioManager.h"
#include "Core/DCSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "FileMediaSource.h"
#include "Input/CommonUIInputTypes.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "DirectorsCut"

namespace
{
	constexpr float IntroFade = 0.6f;
	// Hold time of each card, in order: Unreal, FMOD, Ampersand Games
	constexpr float IntroHold[] = { 3.f, 3.f, 4.5f };
	constexpr int32 IntroStudioCard = 2;
}

UDCIntroWidget::UDCIntroWidget(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bIsBackHandler = false;
	bIsBackActionDisplayedInActionBar = false;
}

void UDCIntroWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true); // any-key skip needs keyboard focus; the action bindings in NativeOnActivated cover Enter, Esc, Space and the gamepad

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Root;

	UCommonBorder* Stage = DCUI::Border(WidgetTree, nullptr, UDCBorder_Black::StaticClass(), FMargin(0.f));
	UCanvasPanelSlot* StageSlot = Root->AddChildToCanvas(Stage);
	StageSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	StageSlot->SetOffsets(FMargin(0.f));

	// Centered, auto-sized card with the given content
	auto AddCard = [this, Root](UWidget* Content)
	{
		UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(Content);
		CardSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CardSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CardSlot->SetPosition(FVector2D::ZeroVector);
		CardSlot->SetAutoSize(true);
		Content->SetVisibility(ESlateVisibility::Collapsed);
		Cards.Add(Content);
	};
	auto Line = [this](UVerticalBox* Box, const FText& Text, TSubclassOf<UCommonTextStyle> Style, float Top)
	{
		UCommonTextBlock* T = DCUI::Text(WidgetTree, Box, Text, Style);
		T->SetJustification(ETextJustify::Center);
		T->SetMargin(FMargin(0.f, Top, 0.f, 0.f));
	};

	// Card 0: Unreal Engine (Epic's license asks for the notice, not for a logo file)
	UVerticalBox* Unreal = WidgetTree->ConstructWidget<UVerticalBox>();
	Line(Unreal, LOCTEXT("Intro_MadeWith", "MADE WITH"), UDCText_Accent::StaticClass(), 0.f);
	Line(Unreal, INVTEXT("UNREAL ENGINE"), UDCText_Title::StaticClass(), 8.f);
	Line(Unreal, LOCTEXT("Intro_UnrealLegal", "Unreal® Engine. Copyright 1998 – 2026, Epic Games, Inc. All rights reserved."), UDCText_Body::StaticClass(), 24.f);
	AddCard(Unreal);

	// Card 1: FMOD attribution required by its license
	UVerticalBox* Fmod = WidgetTree->ConstructWidget<UVerticalBox>();
	Line(Fmod, LOCTEXT("Intro_AudioEngine", "AUDIO ENGINE"), UDCText_Accent::StaticClass(), 0.f);
	Line(Fmod, INVTEXT("FMOD STUDIO"), UDCText_Title::StaticClass(), 8.f);
	Line(Fmod, LOCTEXT("Intro_FmodLegal", "FMOD Studio by Firelight Technologies Pty Ltd."), UDCText_Body::StaticClass(), 24.f);
	AddCard(Fmod);

	// Card 2: studio logo; falls back to the name when the texture asset is missing
	UTexture2D* Logo = LoadObject<UTexture2D>(nullptr, TEXT("/Game/DirectorsCut/UI/Textures/T_AmpersandLogo.T_AmpersandLogo"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Logo)
	{
		USizeBox* LogoBox = WidgetTree->ConstructWidget<USizeBox>();
		LogoBox->SetWidthOverride(560.f);
		LogoBox->SetHeightOverride(560.f);
		UImage* LogoImage = WidgetTree->ConstructWidget<UImage>();
		LogoImage->SetBrushFromTexture(Logo, true);
		LogoBox->AddChild(LogoImage);
		AddCard(LogoBox);
	}
	else
	{
		AddCard(DCUI::Text(WidgetTree, nullptr, INVTEXT("AMPERSAND GAMES"), UDCText_Title::StaticClass()));
	}

	// Opening cinematic, letterboxed on the black stage
	UScaleBox* VideoScale = WidgetTree->ConstructWidget<UScaleBox>();
	VideoScale->SetStretch(EStretch::ScaleToFit);
	UCanvasPanelSlot* VideoSlot = Root->AddChildToCanvas(VideoScale);
	VideoSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	VideoSlot->SetOffsets(FMargin(0.f));
	VideoImage = WidgetTree->ConstructWidget<UImage>();
	VideoScale->AddChild(VideoImage);
	VideoScale->SetVisibility(ESlateVisibility::Collapsed);
	VideoLayer = VideoScale;

	// Skip hint, bottom right (the key bar owns the bottom left)
	UCommonTextBlock* SkipHint = DCUI::Text(WidgetTree, nullptr, LOCTEXT("Intro_SkipHint", "▶ PRESS ANY KEY TO SKIP"), UDCText_Body::StaticClass());
	UCanvasPanelSlot* HintSlot = Root->AddChildToCanvas(SkipHint);
	HintSlot->SetAnchors(FAnchors(1.f, 1.f));
	HintSlot->SetAlignment(FVector2D(1.f, 1.f));
	HintSlot->SetPosition(FVector2D(-48.f, -36.f));
	HintSlot->SetAutoSize(true);
}

void UDCIntroWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	for (const FName Action : { FName("Confirm"), FName("Back"), FName("BackSpace") })
	{
		RegisterUIActionBinding(FBindUIActionArgs(UDCUIInputData::Action(Action), false, FSimpleDelegate::CreateUObject(this, &UDCIntroWidget::Skip)));
	}
	if (Slide == INDEX_NONE)
	{
		ShowSlide(0);
	}
}

void UDCIntroWidget::NativeDestruct()
{
	if (MediaPlayer)
	{
		MediaPlayer->Close();
	}
	Super::NativeDestruct();
}

void UDCIntroWidget::ShowSlide(int32 Index)
{
	for (UWidget* Card : Cards)
	{
		Card->SetVisibility(ESlateVisibility::Collapsed);
	}
	Slide = Index;
	SlideTime = 0.f;
	if (Cards.IsValidIndex(Index))
	{
		Cards[Index]->SetRenderOpacity(0.f);
		Cards[Index]->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (Index == IntroStudioCard)
		{
			ADCAudioManager::PlayCueStatic(this, TEXT("IntroStart")); // VHS tape insert, assigned in DA_AudioConfig
		}
	}
	else
	{
		StartVideo();
	}
}

void UDCIntroWidget::StartVideo()
{
	MediaPlayer = NewObject<UMediaPlayer>(this);
	MediaTexture = NewObject<UMediaTexture>(this);
	MediaTexture->NewStyleOutput = true;
	MediaTexture->SetMediaPlayer(MediaPlayer);
	MediaTexture->UpdateResource();

	FSlateBrush Brush;
	Brush.SetResourceObject(MediaTexture);
	Brush.ImageSize = FVector2D(1920.f, 1080.f);
	VideoImage->SetBrush(Brush);
	VideoLayer->SetRenderOpacity(0.f);
	VideoLayer->SetVisibility(ESlateVisibility::HitTestInvisible);

	UFileMediaSource* Source = NewObject<UFileMediaSource>(this);
	Source->SetFilePath(FPaths::ProjectContentDir() / TEXT("Movies/Intro.mp4"));
	Source->PrecacheFile = true;

	MediaPlayer->PlayOnOpen = true;
	MediaPlayer->SetLooping(false);
	MediaPlayer->SetNativeVolume(UDCSettings::Get()->MasterVolume);
	MediaPlayer->OnEndReached.AddUniqueDynamic(this, &UDCIntroWidget::Finish);
	MediaPlayer->OnMediaOpenFailed.AddUniqueDynamic(this, &UDCIntroWidget::HandleVideoFailed);
	if (!MediaPlayer->OpenSource(Source))
	{
		Finish();
	}
}

void UDCIntroWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bFinished)
	{
		return;
	}
	SlideTime += InDeltaTime;

	if (Cards.IsValidIndex(Slide))
	{
		const float Hold = IntroHold[Slide];
		if (SlideTime >= Hold)
		{
			ShowSlide(Slide + 1);
			return;
		}
		float Alpha = FMath::Min(SlideTime, Hold - SlideTime) / IntroFade;
		// Tape-style flicker while a card comes in
		if (SlideTime < 0.4f && FMath::Frac(SlideTime * 24.f) < 0.5f)
		{
			Alpha *= 0.5f;
		}
		Cards[Slide]->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
		return;
	}

	// Video: fade in from black, fade out over the last moments, give up if it never starts
	bVideoStarted |= MediaPlayer && MediaPlayer->IsPlaying();
	if (!bVideoStarted)
	{
		if (SlideTime > 6.f)
		{
			Finish();
		}
		return;
	}
	float Alpha = SlideTime / IntroFade;
	if (MediaPlayer->GetDuration() > FTimespan::Zero())
	{
		Alpha = FMath::Min(Alpha, (MediaPlayer->GetDuration() - MediaPlayer->GetTime()).GetTotalSeconds() / IntroFade);
	}
	VideoLayer->SetRenderOpacity(FMath::Clamp(Alpha, 0.f, 1.f));
}

FReply UDCIntroWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	Skip();
	return FReply::Handled();
}

FReply UDCIntroWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Skip();
	return FReply::Handled();
}

void UDCIntroWidget::Skip()
{
	if (!bFinished)
	{
		ADCAudioManager::PlayCueStatic(this, TEXT("IntroSkip"));
		Finish();
	}
}

void UDCIntroWidget::Finish()
{
	if (!bFinished)
	{
		bFinished = true;
		OnFinished.Broadcast();
	}
}

#undef LOCTEXT_NAMESPACE
