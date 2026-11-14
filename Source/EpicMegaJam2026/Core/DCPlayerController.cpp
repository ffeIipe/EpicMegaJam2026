#include "Core/DCPlayerController.h"
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
	OrderPanelWidget = MakeWidget<UDCOrderPanelWidget>(this, OrderPanelWidgetClass, 1, ESlateVisibility::HitTestInvisible);
	OrderPanelWidget->Setup(GM->ClientDirector);
	TutorialWidget = MakeWidget<UDCTutorialWidget>(this, TutorialWidgetClass, 2, PassThrough);
	ReviewToastWidget = MakeWidget<UDCReviewToastWidget>(this, ReviewToastWidgetClass, 3, PassThrough);
	RootLayout = MakeWidget<UDCRootLayout>(this, nullptr, 4, PassThrough);
	RootLayout->Stack->OnDisplayedWidgetChanged().AddUObject(this, &ADCPlayerController::HandleScreenChanged);

	GM->ClientDirector->OnReviewReceived.AddDynamic(this, &ADCPlayerController::HandleReview);
	GM->ClientDirector->OnClientReachedCounter.AddDynamic(this, &ADCPlayerController::HandleClientReached);
	GM->ClientDirector->OnClientLeft.AddDynamic(this, &ADCPlayerController::HandleClientLeft);
	GM->FilmCrafting->OnFilmCreated.AddDynamic(this, &ADCPlayerController::HandleFilmCreated);
	GM->Unlocks->OnUnlockPurchased.AddDynamic(this, &ADCPlayerController::HandleUnlockPurchased);
	GM->Economy->OnGameOver.AddDynamic(this, &ADCPlayerController::HandleGameOver);
	GM->Economy->OnVictory.AddDynamic(this, &ADCPlayerController::HandleVictory);
	GM->Economy->OnRentCharged.AddDynamic(this, &ADCPlayerController::HandleRentCharged);
	GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCPlayerController::HandleMoneyChanged);
	GM->DayNightCycle->OnDayClosed.AddDynamic(this, &ADCPlayerController::HandleDayClosed);
	GM->DayNightCycle->OnPhaseChanged.AddDynamic(this, &ADCPlayerController::HandlePhaseChanged);

	// Unattended runs (automated tests) skip the menu so the game starts by itself
	if (GM->Config->bShowMainMenu && !FApp::IsUnattended())
	{
		PushScreen<UDCMainMenuWidget>(RootLayout, MainMenuWidgetClass);
	}
}

void ADCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// Interact and back-room keys are rebindable (UDCSettings): PlayerTick polls them
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADCPlayerController::TogglePause).bExecuteWhenPaused = true;
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
	if (!Cast<UDCDialogueWidget>(Screen))
	{
		DialogueClient.Reset();
	}
	// The crafting panel was closed (Back, Combine) or replaced (day summary): the camera goes home
	if (CameraZone == ECameraZone::BackRoom && !Cast<UDCFilmCraftingWidget>(Screen))
	{
		BeginTransition(false);
	}
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
		Screen->DeactivateWidget();
	}
}

void ADCPlayerController::TogglePause()
{
	// Only reached in game mode: with a screen up, Esc is the CommonUI back action
	if (RootLayout && !IsBlockedByUI())
	{
		PushScreen<UDCPauseWidget>(RootLayout, PauseWidgetClass);
	}
}

// ---------------------------------------------------------------- Look, keys and camera zone

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

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
	}
}

void ADCPlayerController::SetCameraZone(ECameraZone Zone)
{
	if (CameraZone != Zone)
	{
		CameraZone = Zone;
		OnCameraZoneChanged.Broadcast(Zone);
	}
}

ULevelSequencePlayer* ADCPlayerController::GetSequencePlayer()
{
	if (!SequencePlayer)
	{
		ULevelSequence* Sequence = GetWorld()->GetAuthGameMode<ADCGameMode>()->Config->BackRoomSequence.LoadSynchronous();
		if (!Sequence)
		{
			return nullptr; // art hasn't delivered LS_ToBackRoom: BeginTransition glides the camera instead
		}
		FMovieSceneSequencePlaybackSettings Settings;
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
		ShowTutorial(TEXT("Pick tapes and combine them"), bTutorialBackRoomShown);
	}
	else if (CameraZone == ECameraZone::ToCounter)
	{
		SetControlRotation(LookRot); // aim where the player was looking before leaving
		SetCameraZone(ECameraZone::Counter);
		bDialogueDue = true;
		TryOpenDialogue();
	}
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
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || CameraZone != ECameraZone::Counter || IsBlockedByUI())
	{
		return;
	}
	ADCClient* Client;
	const ADCInteractable* Prop;
	TraceTarget(Client, Prop);
	if (Client)
	{
		OpenDialogue(Client); // normally automatic; this reopens it after Back
		return;
	}
	if (!Prop)
	{
		return;
	}
	Cue(TEXT("Click"));
	if (Prop->InteractType == EInteractType::CashRegister)
	{
		PushScreen<UDCSupplierWidget>(RootLayout, SupplierWidgetClass);
	}
	else if (Prop->InteractType == EInteractType::Computer)
	{
		PushScreen<UDCComputerWidget>(RootLayout, ComputerWidgetClass);
	}
}

// ---------------------------------------------------------------- Order dialogue

void ADCPlayerController::OpenDialogue(ADCClient* Client)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	const bool bDeliver = Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0;
	UDCDialogueWidget* Dialogue = PushScreen<UDCDialogueWidget>(RootLayout, DialogueWidgetClass);
	Dialogue->OnButton.AddUniqueDynamic(this, &ADCPlayerController::OnDialogueButton); // pooled screens come back already bound
	Dialogue->Show(bDeliver ? FText::FromString(FString::Printf(TEXT("\"%s\""), *GM->FilmCrafting->HeldFilms[0].Title.ToString())) : Client->Runtime.OrderText,
		FText::FromString(bDeliver ? TEXT("Here you go") : TEXT("Coming right up")));
	DialogueClient = Client;
	Cue(TEXT("DialogueOpen"));
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
			Client->bOrderAcknowledged = true;
			ShowTutorial(*FString::Printf(TEXT("Press %s to go to the back room"), *UDCSettings::Get()->BackRoom.GetDisplayName(false).ToString().ToUpper()), bTutorialAcceptShown);
		}
	}
	CloseScreen();
}

// ---------------------------------------------------------------- Game events

void ADCPlayerController::HandleReview(const FReview& Review)
{
	const UPDA_GameConfig* Cfg = GetWorld()->GetAuthGameMode<ADCGameMode>()->Config;
	const int32* Stars = Cfg->ReviewStars.Find(Review.Reaction);
	ReviewToastWidget->ShowReview(Review, Stars ? *Stars : 0, Cfg->ToastDurationSec);
	Cue(*FString::Printf(TEXT("Review_%s"), *StaticEnum<EReaction>()->GetNameStringByValue((int64)Review.Reaction)));
	if (Review.MoneyDelta < 0)
	{
		StartShake(); // fine received
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
}

void ADCPlayerController::HandleUnlockPurchased(FName UnlockId)
{
	Cue(TEXT("UnlockPurchased"));
}

void ADCPlayerController::HandleFilmCreated(const FCreatedFilm& Film)
{
	Cue(TEXT("FilmCreated"));
	HitStop();
	if (CameraZone == ECameraZone::BackRoom)
	{
		CloseScreen(); // the panel closes and the camera goes home (HandleScreenChanged)
	}
	// TODO(art): CRT static + rewind effect and the VHS-into-player animation need the TV/player props; play them before closing
}

void ADCPlayerController::HandleClientReached(AActor* Client)
{
	Cue(TEXT("ClientAtCounter"));
	ShowTutorial(TEXT("Listen to what they want"), bTutorialClientShown);
	bDialogueDue = true;
	GetWorldTimerManager().SetTimerForNextTick(this, &ADCPlayerController::TryOpenDialogue);
}

void ADCPlayerController::HandleClientLeft(AActor* Client, bool bServed)
{
	if (Client && Client == DialogueClient.Get())
	{
		CloseScreen(); // ran out of patience mid-conversation: don't leave their dialogue up
	}
	if (bServed && TutorialWidget)
	{
		TutorialWidget->HideHint(); // the loop was understood: hide whatever sign is up
	}
}

void ADCPlayerController::ShowTutorial(const TCHAR* Text, bool& bFlag)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (bFlag || !GM || !GM->Config->bShowTutorial || !TutorialWidget)
	{
		return;
	}
	bFlag = true;
	TutorialWidget->ShowHint(FText::FromString(Text)); // replaces the previous sign: one at a time
}

void ADCPlayerController::HandleDayClosed(const FDaySummary& Summary)
{
	UDCDaySummaryWidget* Screen = PushScreen<UDCDaySummaryWidget>(RootLayout, DaySummaryWidgetClass);
	Screen->OnOpenStore.AddUniqueDynamic(this, &ADCPlayerController::HandleOpenStore);
	Screen->Show(Summary);
}

void ADCPlayerController::HandleOpenStore()
{
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
