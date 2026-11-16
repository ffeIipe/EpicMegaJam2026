#include "Core/DCPlayerController.h"
#include "Core/BPFL_Scoring.h"
#include "Core/DCGameInstance.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/DCInteractable.h"
#include "Core/DCSettings.h"
#include "UI/DCWidgets.h"
#include "UI/DCHudWidgets.h"
#include "UI/DCMenuWidgets.h"
#include "UI/DCCommonUI.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Containers/Ticker.h"
#include "Misc/App.h"

namespace
{
	template <typename T>
	T* MakeWidget(APlayerController* PC, TSubclassOf<T> Cls, int32 ZOrder, ESlateVisibility Visibility)
	{
		T* W = CreateWidget<T>(PC, Cls ? Cls : TSubclassOf<T>(T::StaticClass()));
		W->AddToViewport(ZOrder);
		W->SetVisibility(Visibility);
		return W;
	}

	// Linear up to the knee (limit minus the soft zone), then tanh eases the output towards the limit. Slope is 1 at the knee: no jolt.
	float SoftLimit(float Raw, float Limit, float SoftZone)
	{
		const float Knee = Limit * (1.f - SoftZone);
		const float Range = Limit - Knee;
		const float Abs = FMath::Abs(Raw);
		return Range <= KINDA_SMALL_NUMBER || Abs <= Knee ? FMath::Clamp(Raw, -Limit, Limit) : FMath::Sign(Raw) * (Knee + Range * FMath::Tanh((Abs - Knee) / Range));
	}

	// Raw offset where the output is ~96% of the way through the soft zone; accumulating further would only make coming back feel sticky
	float SoftLimitReach(float Limit, float SoftZone)
	{
		const float Range = Limit * SoftZone;
		return Limit * (1.f - SoftZone) + 2.f * Range;
	}

	// Replaces whatever screen is up (the stack has no transition, so the old one is gone right away)
	template <typename T>
	T* PushScreen(UDCRootLayout* Root, TSubclassOf<T> Cls)
	{
		Root->Stack->ClearWidgets();
		return Root->Stack->AddWidget<T>(Cls ? Cls : TSubclassOf<T>(T::StaticClass()));
	}
}

void ADCPlayerController::Cue(const TCHAR* Name) const
{
	if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
	{
		GM->PlayCue(Name);
	}
}

// ---------------------------------------------------------------- Setup

void ADCPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UDCSettings::Get()->Apply(); // saved volume
	LookRot = CounterRot = GetPawn() ? GetPawn()->GetActorRotation() : FRotator::ZeroRotator;
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config)
	{
		return;
	}
	const ESlateVisibility PassThrough = ESlateVisibility::SelfHitTestInvisible;

	HUDWidget = MakeWidget<UDCHUDWidget>(this, HUDWidgetClass, 0, PassThrough);
	StatsWidget = MakeWidget<UDCStatsWidget>(this, nullptr, 5, PassThrough); // above the screens so the numbers are always visible
	OrderPanelWidget = MakeWidget<UDCOrderPanelWidget>(this, OrderPanelWidgetClass, 1, ESlateVisibility::HitTestInvisible);
	OrderPanelWidget->Setup(GM->ClientDirector);
	ReviewToastWidget = MakeWidget<UDCReviewToastWidget>(this, ReviewToastWidgetClass, 6, PassThrough); // above everything: it only lasts a few seconds
	RootLayout = MakeWidget<UDCRootLayout>(this, nullptr, 4, PassThrough);
	// Above RootLayout (ZOrder 4) so the guide remains visible over modal screens without blocking clicks
	TutorialWidget = MakeWidget<UDCTutorialWidget>(this, TutorialWidgetClass, 5, PassThrough);
	TutorialWidget->OnSkipped.AddDynamic(this, &ADCPlayerController::HandleTutorialSkipped);
	RootLayout->Stack->OnDisplayedWidgetChanged().AddUObject(this, &ADCPlayerController::HandleScreenChanged);

	GM->ClientDirector->OnReviewReceived.AddDynamic(this, &ADCPlayerController::HandleReview);
	GM->ClientDirector->OnClientReachedCounter.AddDynamic(this, &ADCPlayerController::HandleClientReached);
	GM->ClientDirector->OnClientLeft.AddDynamic(this, &ADCPlayerController::HandleClientLeft);
	GM->FilmCrafting->OnFilmCreated.AddDynamic(this, &ADCPlayerController::HandleFilmCreated);
	GM->FilmCrafting->OnFilmStateChanged.AddDynamic(this, &ADCPlayerController::HandleTutorialFilmStateChanged);
	GM->Inventory->OnTapePurchased.AddDynamic(this, &ADCPlayerController::HandleTutorialTapePurchased);
	GM->Unlocks->OnUnlockPurchased.AddDynamic(this, &ADCPlayerController::HandleUnlockPurchased);
	GM->Economy->OnGameOver.AddDynamic(this, &ADCPlayerController::HandleGameOver);
	GM->Economy->OnVictory.AddDynamic(this, &ADCPlayerController::HandleVictory);
	GM->Economy->OnRentCharged.AddDynamic(this, &ADCPlayerController::HandleRentCharged);
	GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCPlayerController::HandleMoneyChanged);
	GM->DayNightCycle->OnDayClosed.AddDynamic(this, &ADCPlayerController::HandleDayClosed);
	GM->DayNightCycle->OnPhaseChanged.AddDynamic(this, &ADCPlayerController::HandlePhaseChanged);

	// Main menu now runs in its own level (MAP_MainMenu); apply any active save slot when entering MAP_Rewind.
	UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr;
	if (GI)
	{
		GI->ApplyActiveSaveToGameMode(GM);
	}
	const bool bRunTutorial = GM->Config->bShowTutorial && GM->DayNightCycle->DayIndex == 1 && (!GI || GI->ShouldRunTutorial());
	TutorialStep = bRunTutorial ? ETutorialStep::WaitForClient : ETutorialStep::Inactive;
	GM->ClientDirector->bHoldExtraClientsForTutorial = bRunTutorial;
	TryStartIntro();
	if (!bPlayingIntro && GM->AudioManager)
	{
		GM->AudioManager->EndIntroMusicFadeIn(0.f);
		GM->AudioManager->SetZoneAmbience(CameraZone);
		if (GM->DayNightCycle)
		{
			GM->AudioManager->SetPhaseMusic(GM->DayNightCycle->CurrentPhase);
		}
	}
}

void ADCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Interact and back-room keys are rebindable (UDCSettings): PlayerTick polls them
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADCPlayerController::TogglePause).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::F3, IE_Pressed, this, &ADCPlayerController::SkipTutorial);
#if !UE_BUILD_SHIPPING
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ADCPlayerController::ToggleDebugAttributes);
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ADCPlayerController::DebugSpawnTutorialClient);
#endif
}

// ---------------------------------------------------------------- Screens

bool ADCPlayerController::IsBlockedByUI() const
{
	return RootLayout && RootLayout->Stack->GetActiveWidget() != nullptr;
}

void ADCPlayerController::HandleScreenChanged(UCommonActivatableWidget* Screen)
{
	const UDCScreen* DCScreen = Cast<UDCScreen>(Screen);
	SetPause(DCScreen && DCScreen->bPausesGame);
	const bool bPauseOrDialogue = Cast<UDCPauseWidget>(Screen) != nullptr || Cast<UDCDialogueWidget>(Screen) != nullptr;
	if (const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr; GM && GM->AudioManager)
	{
		GM->AudioManager->SetPauseOrDialogueActive(bPauseOrDialogue);
	}
	if (OrderPanelWidget)
	{
		// The combine screen shows the order itself, and the dialogue says the same thing as the panel
		const bool bShowsOrder = Cast<UDCFilmCraftingWidget>(Screen) || (Cast<UDCDialogueWidget>(Screen) && !bDialogueDelivering);
		OrderPanelWidget->SetVisibility(bShowsOrder ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (!Cast<UDCDialogueWidget>(Screen))
	{
		DialogueClient.Reset();
		if (TutorialStep == ETutorialStep::DialogueOrder)
		{
			SetTutorialStep(ETutorialStep::ApproachClient); // cancelled dialogue without accepting
		}
	}
	if (TutorialWidget)
	{
		const bool bTopRight = Cast<UDCFilmCraftingWidget>(Screen) || Cast<UDCComputerWidget>(Screen) || Cast<UDCSupplierWidget>(Screen);
		TutorialWidget->SetPlacement(Cast<UDCDialogueWidget>(Screen) ? ETutorialPlacement::TopCenter
			: (bTopRight ? ETutorialPlacement::TopRight : ETutorialPlacement::BottomCenter));
	}
	// The crafting panel was closed (Back, Combine) or replaced (day summary): the camera goes home
	if (CameraZone == ECameraZone::BackRoom && !Cast<UDCFilmCraftingWidget>(Screen))
	{
		const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
		if (TutorialStep == ETutorialStep::CombinePick && (!GM || GM->FilmCrafting->HeldFilms.Num() == 0))
		{
			SetTutorialStep(ETutorialStep::GoToBackRoom); // left back room without combining yet
		}
		BeginTransition(false);
	}
	RefreshTutorial();
	if (!Screen && bDialogueDue)
	{
		// Next tick: PushScreen clears the stack before adding, and that must not count as "nothing is up"
		GetWorldTimerManager().SetTimerForNextTick(this, &ADCPlayerController::TryOpenDialogue);
	}
}

void ADCPlayerController::CloseScreen()
{
	if (UCommonActivatableWidget* Screen = RootLayout ? RootLayout->Stack->GetActiveWidget() : nullptr)
	{
		Cue(TEXT("ScreenClose"));
		Screen->DeactivateWidget();
	}
}

void ADCPlayerController::TogglePause()
{
	if (bPlayingIntro || bPlayingWin)
	{
		return;
	}
	// Only reached in game mode: with a screen up, Esc is the CommonUI back action
	if (RootLayout && !IsBlockedByUI())
	{
		Cue(TEXT("PauseOpen"));
		PushScreen<UDCPauseWidget>(RootLayout, PauseWidgetClass);
	}
}

// ---------------------------------------------------------------- Look, keys and camera zone

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const ADCInteractable* Aimed = nullptr;
	if (!bPlayingIntro && !bPlayingWin && !bFallbackTransition && !DialogueClient.IsValid() && CameraZone == ECameraZone::Counter && !IsBlockedByUI())
	{
		ADCClient* AimedClient;
		TraceTarget(AimedClient, Aimed);
	}
	if (Aimed != HighlightedProp.Get())
	{
		if (HighlightedProp.IsValid())
		{
			HighlightedProp->SetHighlighted(false);
		}
		if (Aimed)
		{
			Aimed->SetHighlighted(true);
		}
		HighlightedProp = Aimed;
	}

	if (bPlayingWin)
	{
		return;
	}
	if (bPlayingIntro)
	{
		TickIntro(DeltaTime);
		return;
	}

	if (IsTutorialActive() && WasInputKeyJustPressed(EKeys::F3))
	{
		SkipTutorial();
	}

	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config || !GetPawn())
	{
		return; // BP_DC_GameMode must have DA_GameConfig assigned
	}
	if (bFallbackTransition)
	{
		TickFallbackTransition(DeltaTime);
		return;
	}
	if (CameraZone == ECameraZone::Counter && DialogueClient.IsValid())
	{
		LookAtDialogueClient(DeltaTime); // the dialogue took the mouse: the camera turns to whoever is talking
		return;
	}
	// No looking or interacting during the sequences or in the back room
	if (CameraZone != ECameraZone::Counter || IsBlockedByUI())
	{
		return;
	}

	UDCSettings* Settings = UDCSettings::Get();
	if (WasInputKeyJustPressed(Settings->Interact))
	{
		OnInteract();
	}
	if (WasInputKeyJustPressed(Settings->BackRoom))
	{
		GoToBackRoom();
		return;
	}

	float DX = 0.f, DY = 0.f;
	GetInputMouseDelta(DX, DY);
	const float Sens = GM->Config->MouseSensitivity * Settings->MouseSensitivityScale;
	// The range never wraps or passes its edges; near them the look eases to a stop (SoftLimit)
	const float Soft = GM->Config->LookSoftZone;
	LookRaw.X = FMath::Clamp(LookRaw.X + DX * Sens, -SoftLimitReach(GM->Config->LookYawLimit, Soft), SoftLimitReach(GM->Config->LookYawLimit, Soft));
	LookRaw.Y = FMath::Clamp(LookRaw.Y + DY * Sens, -SoftLimitReach(GM->Config->LookPitchLimit, Soft), SoftLimitReach(GM->Config->LookPitchLimit, Soft));
	LookRot.Yaw = CounterRot.Yaw + SoftLimit(LookRaw.X, GM->Config->LookYawLimit, Soft);
	LookRot.Pitch = CounterRot.Pitch + SoftLimit(LookRaw.Y, GM->Config->LookPitchLimit, Soft);
	FRotator Rot = LookRot;
	if (ShakeLeft > 0.f)
	{
		ShakeLeft -= DeltaTime;
		const float Strength = ShakeMagnitudeDeg * FMath::Clamp(ShakeLeft / FMath::Max(ShakeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		Rot += FRotator(FMath::FRandRange(-Strength, Strength), FMath::FRandRange(-Strength, Strength), 0.f);
	}
	SetControlRotation(Rot);
}

void ADCPlayerController::LookAtDialogueClient(float DeltaTime)
{
	const UPDA_GameConfig* Cfg = GetWorld()->GetAuthGameMode<ADCGameMode>()->Config;
	const ADCClient* Client = DialogueClient.Get();
	const FVector Eye = GetPawn()->GetActorLocation() + FVector(0.f, 0.f, 64.f); // same eye height as the camera
	const FVector Head = Client->GetActorLocation() + FVector(0.f, 0.f, Client->GetSimpleCollisionHalfHeight() * 0.7f);
	const FRotator Target = (Head - Eye).Rotation();
	// Aim only inside the linear part of the look range, so LookRaw and LookRot stay equal and the player takes over without a jump
	const float YawKnee = Cfg->LookYawLimit * (1.f - Cfg->LookSoftZone);
	const float PitchKnee = Cfg->LookPitchLimit * (1.f - Cfg->LookSoftZone);
	const float TargetYaw = FMath::Clamp(FRotator::NormalizeAxis(Target.Yaw - CounterRot.Yaw), -YawKnee, YawKnee);
	const float TargetPitch = FMath::Clamp(FRotator::NormalizeAxis(Target.Pitch - CounterRot.Pitch), -PitchKnee, PitchKnee);
	LookRaw.X = FMath::FInterpTo(LookRaw.X, TargetYaw, DeltaTime, Cfg->DialogueLookSpeed);
	LookRaw.Y = FMath::FInterpTo(LookRaw.Y, TargetPitch, DeltaTime, Cfg->DialogueLookSpeed);
	LookRot.Yaw = CounterRot.Yaw + SoftLimit(LookRaw.X, Cfg->LookYawLimit, Cfg->LookSoftZone);
	LookRot.Pitch = CounterRot.Pitch + SoftLimit(LookRaw.Y, Cfg->LookPitchLimit, Cfg->LookSoftZone);
	SetControlRotation(LookRot);
}

bool ADCPlayerController::CanGoToBackRoom() const
{
	return CameraZone == ECameraZone::Counter && RootLayout && !IsBlockedByUI();
}

void ADCPlayerController::GoToBackRoom()
{
	if (CanGoToBackRoom() && GetPawn())
	{
		BeginTransition(true);
		RefreshTutorial();
	}
}

void ADCPlayerController::SetCameraZone(ECameraZone Zone)
{
	if (CameraZone != Zone)
	{
		CameraZone = Zone;
		if (const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr; GM && GM->AudioManager)
		{
			GM->AudioManager->SetZoneAmbience(Zone);
		}
		OnCameraZoneChanged.Broadcast(Zone);
	}
}

ULevelSequencePlayer* ADCPlayerController::GetSequencePlayer()
{
	if (!SequencePlayer)
	{
		const UPDA_GameConfig* Cfg = GetWorld()->GetAuthGameMode<ADCGameMode>()->Config;
		ULevelSequence* Sequence = Cfg->BackRoomSequence.LoadSynchronous();
		if (!Sequence)
		{
			return nullptr; // no sequence assigned: BeginTransition glides the camera instead
		}
		FMovieSceneSequencePlaybackSettings Settings;
		Settings.PlayRate = Cfg->BackRoomPlayRate;
		Settings.bPauseAtEnd = true; // hold the last frame while the crafting panel is up; reverse then starts from it
		ALevelSequenceActor* Actor = nullptr;
		SequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, Actor);
		if (SequencePlayer)
		{
			SequencePlayer->OnFinished.AddDynamic(this, &ADCPlayerController::HandleSequenceFinished);
		}
	}
	return SequencePlayer;
}

void ADCPlayerController::BeginTransition(bool bForward)
{
	Cue(bForward ? TEXT("WarehouseDoorOpen") : TEXT("WarehouseDoorClose"));
	SetCameraZone(bForward ? ECameraZone::ToBackRoom : ECameraZone::ToCounter);
	if (ULevelSequencePlayer* Sequence = GetSequencePlayer())
	{
		bForward ? Sequence->Play() : Sequence->PlayReverse();
		return;
	}
	if (bForward)
	{
		const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
		CounterLoc = BackRoomLoc = GetPawn()->GetActorLocation();
		BackRoomRot = LookRot;
		TArray<AActor*> Targets;
		UGameplayStatics::GetAllActorsWithTag(this, GM->Config->BackRoomCameraTag, Targets);
		if (Targets.Num() > 0)
		{
			BackRoomLoc = Targets[0]->GetActorLocation();
			BackRoomRot = Targets[0]->GetActorRotation();
		}
	}
	bFallbackTransition = true;
	bFallbackForward = bForward;
	TransitionTime = 0.f;
}

void ADCPlayerController::TickFallbackTransition(float DeltaTime)
{
	const float Duration = FMath::Max(GetWorld()->GetAuthGameMode<ADCGameMode>()->Config->BackRoomTransitionSec, KINDA_SMALL_NUMBER);
	TransitionTime = FMath::Min(TransitionTime + DeltaTime, Duration);
	float Alpha = FMath::InterpEaseInOut(0.f, 1.f, TransitionTime / Duration, 2.f);
	Alpha = bFallbackForward ? Alpha : 1.f - Alpha;
	GetPawn()->SetActorLocation(FMath::Lerp(CounterLoc, BackRoomLoc, Alpha));
	SetControlRotation(FQuat::Slerp(LookRot.Quaternion(), BackRoomRot.Quaternion(), Alpha).Rotator());
	if (TransitionTime >= Duration)
	{
		bFallbackTransition = false;
		FinishTransition();
	}
}

void ADCPlayerController::HandleSequenceFinished()
{
	if (CameraZone == ECameraZone::ToBackRoom || CameraZone == ECameraZone::ToCounter)
	{
		FinishTransition();
	}
}

void ADCPlayerController::FinishTransition()
{
	if (CameraZone == ECameraZone::ToBackRoom)
	{
		if (IsBlockedByUI())
		{
			BeginTransition(false); // a menu or the day summary came up on the way: don't open the panel under it
			return;
		}
		PushScreen<UDCFilmCraftingWidget>(RootLayout, FilmCraftingWidgetClass);
		SetCameraZone(ECameraZone::BackRoom);
		if (TutorialStep == ETutorialStep::GoToBackRoom)
		{
			SetTutorialStep(ETutorialStep::CombinePick);
		}
		else
		{
			RefreshTutorial();
		}
	}
	else if (CameraZone == ECameraZone::ToCounter)
	{
		if (SequencePlayer)
		{
			SequencePlayer->Stop(); // bPauseAtEnd keeps the sequence's camera cut alive; stopping gives the view back to the pawn
		}
		SetControlRotation(LookRot); // aim where the player was looking before leaving
		SetCameraZone(ECameraZone::Counter); // the dialogue (order or delivery) reopens with E
		RefreshTutorial();
	}
}

// ---------------------------------------------------------------- Intro sequence

void ADCPlayerController::TryStartIntro()
{
	UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr;
	ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
	if (!GI || !GM || !GM->Config || !GI->ShouldPlayIntro())
	{
		return;
	}

	ULevelSequence* Sequence = GM->Config->IntroSequence.LoadSynchronous();
	if (!Sequence)
	{
		Sequence = LoadObject<ULevelSequence>(nullptr, TEXT("/Game/_MainGame/Sequences/Intro/MS_Intro.MS_Intro"));
	}
	if (!Sequence)
	{
		return;
	}

	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = false;
	ALevelSequenceActor* Actor = nullptr;
	IntroSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, Actor);
	if (!IntroSequencePlayer)
	{
		return;
	}

	IntroSequencePlayer->OnFinished.AddDynamic(this, &ADCPlayerController::FinishIntro);
	GM->DayNightCycle->bRunning = false;
	if (HUDWidget) HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (StatsWidget) StatsWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (OrderPanelWidget) OrderPanelWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (TutorialWidget) TutorialWidget->SetVisibility(ESlateVisibility::Collapsed);
	IntroSkipWidget = MakeWidget<UDCIntroSkipWidget>(this, IntroSkipWidgetClass, 6, ESlateVisibility::HitTestInvisible);

	bPlayingIntro = true;
	IntroSkipHoldElapsed = 0.f;
	if (GM->AudioManager)
	{
		GM->AudioManager->BeginIntroMusicFadeOut(2.f);
	}
	Cue(TEXT("IntroStart"));
	IntroSequencePlayer->Play();
}

bool ADCPlayerController::IsAnyDigitalKeyDown() const
{
	static TArray<FKey> DigitalKeys;
	if (DigitalKeys.IsEmpty())
	{
		TArray<FKey> AllKeys;
		EKeys::GetAllKeys(AllKeys);
		for (const FKey& Key : AllKeys)
		{
			if (Key.IsValid() && Key != EKeys::AnyKey && !Key.IsAxis1D() && !Key.IsAxis2D() && !Key.IsAxis3D())
			{
				DigitalKeys.Add(Key);
			}
		}
	}
	for (const FKey& Key : DigitalKeys)
	{
		if (IsInputKeyDown(Key))
		{
			return true;
		}
	}
	return false;
}

void ADCPlayerController::TickIntro(float DeltaTime)
{
	const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
	const float HoldDuration = (GM && GM->Config) ? FMath::Max(GM->Config->IntroSkipHoldSec, 0.1f) : 3.f;
	IntroSkipHoldElapsed = IsAnyDigitalKeyDown() ? (IntroSkipHoldElapsed + DeltaTime) : 0.f;
	if (IntroSkipWidget)
	{
		IntroSkipWidget->SetSkipProgress(IntroSkipHoldElapsed / HoldDuration);
	}
	if (IntroSkipHoldElapsed >= HoldDuration)
	{
		Cue(TEXT("IntroSkip"));
		FinishIntro();
	}
}

void ADCPlayerController::FinishIntro()
{
	if (!bPlayingIntro)
	{
		return;
	}
	bPlayingIntro = false;
	IntroSkipHoldElapsed = 0.f;

	if (IntroSequencePlayer)
	{
		IntroSequencePlayer->OnFinished.RemoveDynamic(this, &ADCPlayerController::FinishIntro);
		IntroSequencePlayer->Stop();
		IntroSequencePlayer = nullptr;
	}
	if (IntroSkipWidget)
	{
		IntroSkipWidget->RemoveFromParent();
		IntroSkipWidget = nullptr;
	}
	if (HUDWidget) HUDWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (StatsWidget) StatsWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (OrderPanelWidget) OrderPanelWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (TutorialWidget) TutorialWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetControlRotation(LookRot);

	if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
	{
		GI->MarkIntroPlayed();
	}
	if (ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr)
	{
		GM->DayNightCycle->bRunning = true;
		if (GM->AudioManager)
		{
			GM->AudioManager->EndIntroMusicFadeIn(2.f);
			GM->AudioManager->SetZoneAmbience(CameraZone);
			if (GM->DayNightCycle)
			{
				GM->AudioManager->SetPhaseMusic(GM->DayNightCycle->CurrentPhase);
			}
		}
	}
	RefreshTutorial();
}

// ---------------------------------------------------------------- Interaction (key from UDCSettings, E by default)

void ADCPlayerController::TraceTarget(ADCClient*& OutClient, const ADCInteractable*& OutProp) const
{
	OutClient = nullptr;
	OutProp = nullptr;
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config || !PlayerCameraManager)
	{
		return;
	}
	const FVector Start = PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PlayerCameraManager->GetCameraRotation().Vector() * GM->Config->InteractRange;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn()); // the camera sits inside the player's own capsule
	Params.bTraceComplex = true; // props with swapped meshes may have no simple collision
	FHitResult Hit;

	// Clients (pawns): only the one at the front of the queue can be served
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_Pawn), Params))
	{
		ADCClient* Client = Cast<ADCClient>(Hit.GetActor());
		if (Client && Client->IsAtFront() && Client == GM->ClientDirector->GetFrontClient())
		{
			OutClient = Client;
			return;
		}
	}
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		// The back-room props (shelves, VHS player) are scenery now
		const ADCInteractable* Prop = Cast<ADCInteractable>(Hit.GetActor());
		if (Prop && (Prop->InteractType == EInteractType::CashRegister || Prop->InteractType == EInteractType::Computer))
		{
			OutProp = Prop;
		}
	}
}

FText ADCPlayerController::GetInteractPrompt() const
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || CameraZone != ECameraZone::Counter || IsBlockedByUI())
	{
		return FText::GetEmpty();
	}
	ADCClient* Client;
	const ADCInteractable* Prop;
	TraceTarget(Client, Prop);
	if (Client)
	{
		const bool bDeliver = Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0;
		return FText::FromString(bDeliver ? TEXT("Hand over the film") : TEXT("Talk to the customer"));
	}
	return Prop ? Prop->GetLabel() : FText::GetEmpty();
}

void ADCPlayerController::OnInteract()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || CameraZone != ECameraZone::Counter || IsBlockedByUI())
	{
		return;
	}
	ADCClient* Client;
	const ADCInteractable* Prop;
	TraceTarget(Client, Prop);
	if (Client)
	{
		OpenDialogue(Client);
		return;
	}
	if (Prop)
	{
		OpenInteractableScreen(Prop->InteractType);
	}
}

bool ADCPlayerController::OpenInteractableScreen(EInteractType Type)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || CameraZone != ECameraZone::Counter || IsBlockedByUI())
	{
		return false;
	}
	if (Type == EInteractType::CashRegister)
	{
		PushScreen<UDCSupplierWidget>(RootLayout, SupplierWidgetClass);
		return true;
	}
	if (Type == EInteractType::Computer)
	{
		for (TActorIterator<ADCInteractable> It(GetWorld()); It; ++It)
		{
			if (It->InteractType == EInteractType::Computer)
			{
				It->ClearNews();
			}
		}
		PushScreen<UDCComputerWidget>(RootLayout, ComputerWidgetClass);
		return true;
	}
	return false;
}

// ---------------------------------------------------------------- Order dialogue

void ADCPlayerController::OpenDialogue(ADCClient* Client)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!Client || !GM)
	{
		return;
	}
	const bool bDeliver = Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0;
	if (!bDeliver && TutorialStep == ETutorialStep::ApproachClient)
	{
		TutorialStep = ETutorialStep::DialogueOrder;
	}
	const bool bCanReject = !bDeliver && TutorialStep != ETutorialStep::DialogueOrder;
	bDialogueDelivering = bDeliver; // read by HandleScreenChanged while the screen is pushed
	UDCDialogueWidget* Dialogue = PushScreen<UDCDialogueWidget>(RootLayout, DialogueWidgetClass);
	Dialogue->OnButton.AddUniqueDynamic(this, &ADCPlayerController::OnDialogueButton); // pooled screens come back already bound
	Dialogue->OnReject.AddUniqueDynamic(this, &ADCPlayerController::OnDialogueReject);
	Dialogue->Show(bDeliver ? FText::FromString(FString::Printf(TEXT("\"%s\""), *GM->FilmCrafting->HeldFilms[0].Title.ToString())) : Client->Runtime.OrderText,
		FText::FromString(bDeliver ? TEXT("Here you go") : TEXT("Coming right up")), bCanReject, bDeliver ? nullptr : &Client->Runtime);
	DialogueClient = Client;
	RefreshTutorial();
	Cue(TEXT("DialogueOpen"));
	if (!bDeliver)
	{
		Client->PlayTalkSound();
	}
}

void ADCPlayerController::TryOpenDialogue()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!bDialogueDue || !GM || CameraZone != ECameraZone::Counter || IsBlockedByUI())
	{
		return; // stays due: HandleScreenChanged tries again when the screen closes
	}
	bDialogueDue = false;
	ADCClient* Client = GM->ClientDirector->GetFrontClient();
	if (!Client || !Client->IsAtFront())
	{
		return;
	}
	// A client who already ordered and has no film waiting needs nothing from us; one who has a film in hand gets it delivered
	const bool bDeliver = Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0;
	if (bDeliver || !Client->bOrderAcknowledged)
	{
		OpenDialogue(Client);
	}
}

void ADCPlayerController::OnDialogueButton()
{
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	ADCClient* Client = DialogueClient.Get();
	if (GM && Client)
	{
		if (Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0)
		{
			GM->ClientDirector->DeliverFilm(Client);
			Cue(TEXT("Delivered"));
		}
		else
		{
			Cue(TEXT("OrderAccepted"));
			Client->bOrderAcknowledged = true;
			if (TutorialStep == ETutorialStep::DialogueOrder)
			{
				TutorialStep = ETutorialStep::GoToBackRoom;
			}
		}
	}
	CloseScreen();
}

void ADCPlayerController::OnDialogueReject()
{
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (ADCClient* Client = DialogueClient.Get(); GM && Client && !Client->bOrderAcknowledged)
	{
		GM->ClientDirector->ApplyLeavePenalty(); // turning a client away counts as a lost client (LeavePenalty in the config, 0 by default)
		Client->ForceLeave();
		Cue(TEXT("OrderRejected"));
	}
	CloseScreen();
}

// ---------------------------------------------------------------- Game events

void ADCPlayerController::HandleReview(const FReview& Review)
{
	const UPDA_GameConfig* Cfg = GetWorld()->GetAuthGameMode<ADCGameMode>()->Config;
	const int32* Stars = Cfg->ReviewStars.Find(Review.Reaction);
	if (!Cast<UDCFilmCraftingWidget>(RootLayout->Stack->GetActiveWidget())) // the combine screen covers the toast: skip it there
	{
		ReviewToastWidget->ShowReview(Review, Stars ? *Stars : 0, Cfg->ToastDurationSec);
	}
	Cue(*FString::Printf(TEXT("Review_%s"), *StaticEnum<EReaction>()->GetNameStringByValue((int64)Review.Reaction)));
	if (Review.MoneyDelta < 0)
	{
		StartShake(); // fine received
	}
	if (TutorialStep == ETutorialStep::WaitReview)
	{
		if (bTutorialTapeAfterGenreBought)
		{
			CompleteTutorial();
		}
		else
		{
			SetTutorialStep(bTutorialGenreBought ? ETutorialStep::SupplierBuyTape : ETutorialStep::ComputerBuyGenre);
		}
	}
}

void ADCPlayerController::HandleRentCharged(int32 Amount)
{
	Cue(TEXT("RentCharged"));
	StartShake();
}

void ADCPlayerController::HandleMoneyChanged(int32 NewMoney, int32 Delta)
{
	if (Delta != 0)
	{
		Cue(Delta > 0 ? TEXT("MoneyUp") : TEXT("MoneyDown"));
	}
}

void ADCPlayerController::HandlePhaseChanged(EPhase Phase, int32 DayIndex)
{
	Cue(Phase == EPhase::Day ? TEXT("DayStart") : TEXT("NightStart"));
	if (const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr; GM && GM->AudioManager)
	{
		GM->AudioManager->SetPhaseMusic(Phase);
	}
}

void ADCPlayerController::HandleUnlockPurchased(FName UnlockId)
{
	Cue(TEXT("UnlockPurchased"));
	const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
	if (GM)
	{
		GM->SaveProgress();
	}
	const FUnlockData* Row = GM && GM->Unlocks ? GM->Unlocks->GetUnlockData(UnlockId) : nullptr;
	if (Row && Row->Type == EUnlockType::Genre && IsTutorialActive())
	{
		bTutorialGenreBought = true;
		if (TutorialStep == ETutorialStep::ComputerBuyGenre)
		{
			SetTutorialStep(ETutorialStep::SupplierBuyTape);
		}
	}
}

void ADCPlayerController::HandleFilmCreated(const FCreatedFilm& Film)
{
	Cue(TEXT("FilmCreated"));
	HitStop();
	if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
	{
		GM->SaveProgress();
	}
	if (TutorialStep == ETutorialStep::CombinePick || TutorialStep == ETutorialStep::GoToBackRoom)
	{
		TutorialStep = ETutorialStep::DeliverFilm;
	}
	if (CameraZone == ECameraZone::BackRoom)
	{
		CloseScreen(); // the panel closes and the camera goes home (HandleScreenChanged)
	}
	else
	{
		RefreshTutorial();
	}
	// TODO(art): CRT static + rewind effect and the VHS-into-player animation need the TV/player props; play them before closing
}

void ADCPlayerController::HandleClientReached(AActor* Client)
{
	Cue(TEXT("ClientAtCounter"));
	if (TutorialStep == ETutorialStep::WaitForClient)
	{
		if (ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr)
		{
			GM->DayNightCycle->bTutorialPaused = true;
			GM->ClientDirector->bTutorialPaused = true;
		}
		SetTutorialStep(ETutorialStep::ApproachClient);
	}
}

void ADCPlayerController::HandleClientLeft(AActor* Client, bool bServed)
{
	if (Client && Client == DialogueClient.Get())
	{
		CloseScreen(); // ran out of patience mid-conversation: don't leave their dialogue up
	}
	if (bServed && TutorialStep == ETutorialStep::DeliverFilm)
	{
		SetTutorialStep(ETutorialStep::WaitReview);
		return;
	}
	// Safety guard: if the first tutorial customer somehow leaves unserved, lift pause cleanly rather than softlocking
	if (!bServed && (TutorialStep == ETutorialStep::ApproachClient || TutorialStep == ETutorialStep::DialogueOrder
		|| TutorialStep == ETutorialStep::GoToBackRoom || TutorialStep == ETutorialStep::CombinePick || TutorialStep == ETutorialStep::DeliverFilm))
	{
		const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
		if (!GM || !GM->ClientDirector->GetFrontClient())
		{
			EndTutorialPause();
			SetTutorialStep(ETutorialStep::WaitForClient);
		}
	}
}

void ADCPlayerController::HandleTutorialFilmStateChanged()
{
	if (TutorialStep == ETutorialStep::CombinePick)
	{
		RefreshTutorial();
	}
	else if (TutorialStep == ETutorialStep::DeliverFilm && CameraZone == ECameraZone::BackRoom)
	{
		// Only discarding in the back room reverts to CombinePick; DeliverFilm consumes the held film at the counter right before HandleClientLeft advances to WaitReview
		const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
		if (GM && GM->FilmCrafting->HeldFilms.Num() == 0)
		{
			SetTutorialStep(ETutorialStep::CombinePick);
		}
	}
}

void ADCPlayerController::HandleTutorialTapePurchased(FName Row, int32 Count)
{
	if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
	{
		GM->SaveProgress();
	}
	if (bTutorialGenreBought && IsTutorialActive())
	{
		bTutorialTapeAfterGenreBought = true;
	}
	if (TutorialStep == ETutorialStep::SupplierBuyTape)
	{
		CompleteTutorial();
	}
}

void ADCPlayerController::HandleTutorialSkipped()
{
	SkipTutorial();
}

void ADCPlayerController::SkipTutorial()
{
	if (!IsTutorialActive())
	{
		return;
	}
	if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
	{
		GI->MarkTutorialCompleted();
	}
	GetWorldTimerManager().ClearTimer(TutorialCompleteHideTimer);
	EndTutorialPause();
	TutorialStep = ETutorialStep::Complete;
	if (TutorialWidget)
	{
		TutorialWidget->HideHint();
	}
}

void ADCPlayerController::EndTutorialPause()
{
	if (ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr)
	{
		GM->DayNightCycle->bTutorialPaused = false;
		GM->ClientDirector->ResumeFromTutorial();
	}
}

void ADCPlayerController::CompleteTutorial()
{
	if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
	{
		GI->MarkTutorialCompleted();
	}
	EndTutorialPause();
	TutorialStep = ETutorialStep::Complete;
	Cue(TEXT("TutorialComplete"));
	if (TutorialWidget)
	{
		TutorialWidget->SetPlacement(ETutorialPlacement::BottomCenter);
		TutorialWidget->ShowStep(
			FText::FromString(TEXT("TUTORIAL COMPLETE!")),
			FText::FromString(TEXT("You know clues, VHS combinations, genre unlocks, and supplier orders. Watch patience bars and cover tonight's rent!")),
			FText::FromString(TEXT("Close the Supplier (Esc) - your next customer is on the way!")),
			false);
	}
	GetWorldTimerManager().SetTimer(TutorialCompleteHideTimer, this, &ADCPlayerController::DismissCompletedTutorial, 6.f, false);
}

void ADCPlayerController::DismissCompletedTutorial()
{
	TutorialStep = ETutorialStep::Inactive;
	if (TutorialWidget)
	{
		TutorialWidget->HideHint();
	}
}

void ADCPlayerController::SetTutorialStep(ETutorialStep NextStep)
{
	const bool bChanged = TutorialStep != NextStep;
	TutorialStep = NextStep;
	if (bChanged && NextStep != ETutorialStep::Inactive && NextStep != ETutorialStep::WaitForClient && NextStep != ETutorialStep::Complete)
	{
		Cue(TEXT("TutorialStep"));
	}
	RefreshTutorial();
}

void ADCPlayerController::RefreshTutorial()
{
	if (!TutorialWidget)
	{
		return;
	}
	const ADCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADCGameMode>() : nullptr;
	if (!GM || !GM->Config || !GM->Config->bShowTutorial || !IsTutorialActive() || TutorialStep == ETutorialStep::WaitForClient)
	{
		if (TutorialStep != ETutorialStep::Complete)
		{
			TutorialWidget->HideHint();
		}
		return;
	}
	UCommonActivatableWidget* ActiveScreen = RootLayout ? RootLayout->Stack->GetActiveWidget() : nullptr;
	if (Cast<UDCPauseWidget>(ActiveScreen) || Cast<UDCDaySummaryWidget>(ActiveScreen) || Cast<UDCEndScreenWidget>(ActiveScreen))
	{
		TutorialWidget->HideHint();
		return;
	}

	const UDCSettings* Settings = UDCSettings::Get();
	const FString InteractKey = Settings->Interact.GetDisplayName(false).ToString().ToUpper();
	const FString BackRoomKey = Settings->BackRoom.GetDisplayName(false).ToString().ToUpper();

	switch (TutorialStep)
	{
	case ETutorialStep::ApproachClient:
		TutorialWidget->ShowStep(
			FText::FromString(TEXT("TUTORIAL 1/6: FIRST CUSTOMER")),
			FText::FromString(TEXT("Customers give mood clues - never exact skill or genre names. Turn toward them at the counter to talk.")),
			FText::FromString(ActiveScreen
				? FString::Printf(TEXT("Close this screen (Esc), look at the customer and press [%s]"), *InteractKey)
				: FString::Printf(TEXT("Look at the customer and press [%s] to listen"), *InteractKey)));
		break;

	case ETutorialStep::DialogueOrder:
		TutorialWidget->ShowStep(
			FText::FromString(TEXT("TUTORIAL 1/6: INTERPRET CLUES")),
			FText::FromString(TEXT("Clues map to hidden Skills (Funny, Scary, Brave...). Slapstick/laughs = Funny high; 'sleep tonight' / 'no scares' = Scary 0. Next you'll combine 2+ tapes to hit 6 pts.")),
			FText::FromString(TEXT("Read the clues, then click 'Coming right up'")));
		break;

	case ETutorialStep::GoToBackRoom:
		if (CameraZone == ECameraZone::ToBackRoom)
		{
			TutorialWidget->HideHint();
		}
		else
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 2/6: BACK ROOM")),
				FText::FromString(TEXT("The ORDER box (top-left) keeps their clues pinned. Head to the back room to combine 2+ VHS tapes into one 6-point film.")),
				FText::FromString(FString::Printf(TEXT("Press [%s] to go to the back room"), *BackRoomKey)));
		}
		break;

	case ETutorialStep::CombinePick:
	{
		const int32 Filled = GM->FilmCrafting->GetFilledSlots();
		if (Filled == 0)
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 3/6: INSPECT & PICK")),
				FText::FromString(TEXT("Skills show right under each tape title. Pick tapes whose skills match the ORDER clues (here: Funny high, Scary zero).")),
				FText::FromString(TEXT("Click a Comedy tape (Funny 3) to load Slot 1")));
		}
		else if (!GM->FilmCrafting->CanConfirm())
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 3/6: SKILLS ADD UP")),
				FText::FromString(TEXT("Slotted tapes add together in FILM PROFILE - and you can repeat copies of the same tape! Watch Order size & MATCH WITH THE ORDER.")),
				FText::FromString(TEXT("Click a 2nd Comedy tape (repeats work!) so MATCH turns GREAT")));
		}
		else
		{
			const ADCClient* Front = GM->ClientDirector->GetFrontClient();
			EReaction Reaction = EReaction::Loved;
			if (Front)
			{
				const float Known = UBPFL_Scoring::ComputeErrorRatio(GM->FilmCrafting->GetProfile(), Front->Runtime.Target, GM->Config);
				Reaction = UBPFL_Scoring::GetReaction(Known, GM->Config);
			}
			if (Reaction == EReaction::Loved || Reaction == EReaction::Liked)
			{
				TutorialWidget->ShowStep(
					FText::FromString(TEXT("TUTORIAL 3/6: COMBINE FILM")),
					FText::FromString(TEXT("MATCH WITH THE ORDER looks great! Edit the title if you want - combining consumes the slotted tape copies.")),
					FText::FromString(TEXT("Click COMBINE to return to the counter with your film")));
			}
			else
			{
				TutorialWidget->ShowStep(
					FText::FromString(TEXT("TUTORIAL 3/6: CHECK THE MATCH")),
					FText::FromString(TEXT("MATCH WITH THE ORDER is low. Click 'x' next to a slot to swap tapes until Funny is high and Scary stays 0.")),
					FText::FromString(TEXT("Swap tapes with 'x' or click COMBINE when ready")));
			}
		}
		break;
	}

	case ETutorialStep::DeliverFilm:
		if (CameraZone == ECameraZone::ToCounter)
		{
			TutorialWidget->HideHint();
		}
		else if (Cast<UDCDialogueWidget>(ActiveScreen))
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 4/6: DELIVERY & PAY")),
				FText::FromString(FString::Printf(TEXT("Base rental ($%d) pays immediately. Bonus tips (or fines!) arrive shortly after on ReelTalk.net."), GM->Config->BaseRentalPrice)),
				FText::FromString(TEXT("Click 'Here you go' to deliver your film")));
		}
		else
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 4/6: HAND OVER FILM")),
				FText::FromString(TEXT("Your combined VHS is ready! Base rental pays on delivery; tips (or fines) arrive with the customer's review.")),
				FText::FromString(FString::Printf(TEXT("Look at the customer and press [%s] to deliver"), *InteractKey)));
		}
		break;

	case ETutorialStep::WaitReview:
		TutorialWidget->ShowStep(
			FText::FromString(TEXT("TUTORIAL 4/6: REELTALK REVIEW")),
			FText::FromString(TEXT("Base rent is paid! Customers post star ratings, bonus tips (or fines), and missing-skill feedback on ReelTalk.net.")),
			FText::FromString(TEXT("Wait a moment for the customer's review...")));
		break;

	case ETutorialStep::ComputerBuyGenre:
		if (Cast<UDCComputerWidget>(ActiveScreen))
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 5/6: SHOP UPGRADES")),
				FText::FromString(TEXT("GENRES adds genres to your catalog (future customers ask for them!). COMBINE MACHINE adds slots; STAR LICENSES raise tape tier.")),
				FText::FromString(TEXT("On the GENRES tab, buy a new genre (e.g. ROMANCE for $40)")));
		}
		else
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 5/6: OPEN COMPUTER")),
				FText::FromString(TEXT("See ReelTalk.net above - tips boost your cash and Criterio! Now unlock a new genre on the Computer.")),
				FText::FromString(FString::Printf(TEXT("Look at the COMPUTER on the counter and press [%s]"), *InteractKey)));
		}
		break;

	case ETutorialStep::SupplierBuyTape:
		if (Cast<UDCSupplierWidget>(ActiveScreen))
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 6/6: SUPPLIER STOCK")),
				FText::FromString(TEXT("Switch Genre tabs (try your new genre!) and filter by stars. Skills show below each tape - x1 buys one copy, x5 buys five.")),
				FText::FromString(TEXT("Buy any tape (x1 or x5) to stock your shelf and finish!")));
		}
		else
		{
			TutorialWidget->ShowStep(
				FText::FromString(TEXT("TUTORIAL 6/6: BUY FROM SUPPLIER")),
				FText::FromString(TEXT("Unlocking a genre on the Computer does NOT add copies to stock! You must order physical tapes at the Cash Register.")),
				FText::FromString(Cast<UDCComputerWidget>(ActiveScreen)
					? FString::Printf(TEXT("Close Computer (Esc), look at the CASH REGISTER and press [%s]"), *InteractKey)
					: FString::Printf(TEXT("Look at the CASH REGISTER and press [%s] to open Supplier"), *InteractKey)));
		}
		break;

	default:
		TutorialWidget->HideHint();
		break;
	}
}

void ADCPlayerController::HandleDayClosed(const FDaySummary& Summary)
{
	if (IsTutorialActive())
	{
		SkipTutorial();
	}
	if (UDCGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDCGameInstance>() : nullptr)
	{
		GI->MarkTutorialCompleted();
		if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
		{
			GI->CaptureGameModeToActiveSave(GM, Summary.DayIndex + 1);
		}
	}
	Cue(TEXT("DaySummaryOpen"));
	UDCDaySummaryWidget* Screen = PushScreen<UDCDaySummaryWidget>(RootLayout, DaySummaryWidgetClass);
	Screen->OnOpenStore.AddUniqueDynamic(this, &ADCPlayerController::HandleOpenStore);
	Screen->Show(Summary);
}

void ADCPlayerController::HandleOpenStore()
{
	Cue(TEXT("OpenStore"));
	CloseScreen();
	GetWorld()->GetAuthGameMode<ADCGameMode>()->DayNightCycle->StartNextDay();
}

void ADCPlayerController::HandleGameOver()
{
	Cue(TEXT("GameOver"));
	ShowEndScreen(GameOverWidgetClass ? GameOverWidgetClass : TSubclassOf<UDCEndScreenWidget>(UDCEndScreenWidget::StaticClass()));
}

void ADCPlayerController::HandleVictory()
{
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	ULevelSequence* Sequence = GM->Config ? GM->Config->WinSequence.LoadSynchronous() : nullptr;
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true; // hold the last frame until the end screen covers it
	ALevelSequenceActor* Actor = nullptr;
	WinSequencePlayer = Sequence ? ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, Actor) : nullptr;
	if (!WinSequencePlayer)
	{
		FinishWinSequence(); // no sequence: straight to the end screen
		return;
	}

	CloseScreen();
	GM->DayNightCycle->bRunning = false;
	if (HUDWidget) HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (StatsWidget) StatsWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (OrderPanelWidget) OrderPanelWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (TutorialWidget) TutorialWidget->SetVisibility(ESlateVisibility::Collapsed);
	bPlayingWin = true;
	WinSequencePlayer->OnFinished.AddDynamic(this, &ADCPlayerController::FinishWinSequence);
	WinSequencePlayer->Play();
}

void ADCPlayerController::FinishWinSequence()
{
	bPlayingWin = false;
	Cue(TEXT("Victory"));
	ShowEndScreen(VictoryWidgetClass ? VictoryWidgetClass : TSubclassOf<UDCEndScreenWidget>(UDCEndScreenWidget::StaticClass()));
}

void ADCPlayerController::ShowEndScreen(TSubclassOf<UDCEndScreenWidget> Cls)
{
	PushScreen<UDCEndScreenWidget>(RootLayout, Cls)->Show(GetWorld()->GetAuthGameMode<ADCGameMode>()->DayNightCycle->DayIndex);
}

// ---------------------------------------------------------------- Juice and debug

void ADCPlayerController::StartShake()
{
	ShakeLeft = ShakeDuration;
}

void ADCPlayerController::HitStop()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || GM->Config->HitStopSec <= 0.f)
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(this, 0.01f);
	// Real-time ticker: unaffected by the time dilation it undoes
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
		return false;
	}), GM->Config->HitStopSec);
}

void ADCPlayerController::DebugSpawnTutorialClient()
{
	if (ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
	{
		GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow);
	}
}

void ADCPlayerController::ToggleDebugAttributes()
{
	ADCClient::bShowDebugAttributes = !ADCClient::bShowDebugAttributes; // F1: attributes and weights over each client
}

void ADCPlayerController::GoToBackRoomFromScreen()
{
	CloseScreen();
	GoToBackRoom();
}

bool ADCPlayerController::ShowsBackRoomHint() const
{
	if (CameraZone != ECameraZone::Counter || !RootLayout)
	{
		return false;
	}
	const UCommonActivatableWidget* Screen = RootLayout->Stack->GetActiveWidget();
	return !Screen || Cast<UDCComputerWidget>(Screen) || Cast<UDCSupplierWidget>(Screen);
}
