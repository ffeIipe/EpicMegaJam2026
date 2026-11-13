#include "Core/DCPlayerController.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/DCInteractable.h"
#include "UI/DCWidgets.h"
#include "UI/DCHudWidgets.h"
#include "UI/DCMenuWidgets.h"
#include "Kismet/GameplayStatics.h"
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
	ResetLook();
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config)
	{
		return;
	}
	const ESlateVisibility Hidden = ESlateVisibility::Collapsed;
	const ESlateVisibility PassThrough = ESlateVisibility::SelfHitTestInvisible;

	HUDWidget = MakeWidget<UDCHUDWidget>(this, HUDWidgetClass, 0, PassThrough);
	OrderPanelWidget = MakeWidget<UDCOrderPanelWidget>(this, OrderPanelWidgetClass, 1, ESlateVisibility::HitTestInvisible);
	OrderPanelWidget->Setup(GM->ClientDirector);
	TutorialWidget = MakeWidget<UDCTutorialWidget>(this, TutorialWidgetClass, 2, PassThrough);
	ReviewToastWidget = MakeWidget<UDCReviewToastWidget>(this, ReviewToastWidgetClass, 3, PassThrough);

	DialogueWidget = MakeWidget<UDCDialogueWidget>(this, DialogueWidgetClass, 4, Hidden);
	DialogueWidget->OnButton.AddDynamic(this, &ADCPlayerController::OnDialogueButton);
	FilmCraftingWidget = MakeWidget<UDCFilmCraftingWidget>(this, FilmCraftingWidgetClass, 4, Hidden);
	FilmCraftingWidget->Setup(GM->Inventory, GM->FilmCrafting, GM->Config);
	FilmCraftingWidget->OnBackToCounter.AddDynamic(this, &ADCPlayerController::CloseModal);
	SupplierWidget = MakeWidget<UDCSupplierWidget>(this, SupplierWidgetClass, 4, Hidden);
	SupplierWidget->Setup(GM->Inventory, GM->Economy, GM->DayNightCycle, GM->Config);
	SupplierWidget->OnClose.AddDynamic(this, &ADCPlayerController::CloseModal);
	ShelfWidget = MakeWidget<UDCShelfWidget>(this, ShelfWidgetClass, 4, Hidden);
	ShelfWidget->Setup(GM->Inventory, GM->Config);
	ShelfWidget->OnClose.AddDynamic(this, &ADCPlayerController::CloseModal);

	DaySummaryWidget = MakeWidget<UDCDaySummaryWidget>(this, DaySummaryWidgetClass, 10, Hidden);
	DaySummaryWidget->OnOpenStore.AddDynamic(this, &ADCPlayerController::HandleOpenStore);
	PauseWidget = MakeWidget<UDCPauseWidget>(this, PauseWidgetClass, 11, Hidden);
	PauseWidget->OnContinue.AddDynamic(this, &ADCPlayerController::HandleContinue);
	MainMenuWidget = MakeWidget<UDCMainMenuWidget>(this, MainMenuWidgetClass, 12, Hidden);
	MainMenuWidget->OnPlay.AddDynamic(this, &ADCPlayerController::HandlePlay);

	GM->ClientDirector->OnReviewReceived.AddDynamic(this, &ADCPlayerController::HandleReview);
	GM->ClientDirector->OnClientReachedCounter.AddDynamic(this, &ADCPlayerController::HandleClientReached);
	GM->ClientDirector->OnClientLeft.AddDynamic(this, &ADCPlayerController::HandleClientLeft);
	GM->FilmCrafting->OnFilmCreated.AddDynamic(this, &ADCPlayerController::HandleFilmCreated);
	GM->Economy->OnGameOver.AddDynamic(this, &ADCPlayerController::HandleGameOver);
	GM->Economy->OnVictory.AddDynamic(this, &ADCPlayerController::HandleVictory);
	GM->Economy->OnRentCharged.AddDynamic(this, &ADCPlayerController::HandleRentCharged);
	GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCPlayerController::HandleMoneyChanged);
	GM->DayNightCycle->OnDayClosed.AddDynamic(this, &ADCPlayerController::HandleDayClosed);
	GM->DayNightCycle->OnPhaseChanged.AddDynamic(this, &ADCPlayerController::HandlePhaseChanged);
	OnZoneChanged.AddDynamic(this, &ADCPlayerController::HandleZoneChangedForTutorial);

	ApplyInputMode();
	// Unattended runs (automated tests) skip the menu so the game starts by itself
	if (GM->Config->bShowMainMenu && !FApp::IsUnattended())
	{
		OpenFullScreen(MainMenuWidget);
	}
}

void ADCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ADCPlayerController::ToggleZone);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADCPlayerController::OnClick);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ADCPlayerController::TogglePause).bExecuteWhenPaused = true;
#if !UE_BUILD_SHIPPING
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ADCPlayerController::ToggleDebugAttributes);
	InputComponent->BindKey(EKeys::F2, IE_Pressed, this, &ADCPlayerController::DebugSpawnTutorialClient);
#endif
}

// ---------------------------------------------------------------- Input mode and modals

void ADCPlayerController::ApplyInputMode()
{
	const bool bUI = Modal != EDCModal::None || bFullScreenUI;
	SetShowMouseCursor(bUI);
	if (bUI)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

UUserWidget* ADCPlayerController::GetModalWidget() const
{
	switch (Modal)
	{
	case EDCModal::Dialogue: return DialogueWidget;
	case EDCModal::Supplier: return SupplierWidget;
	case EDCModal::Shelf: return ShelfWidget;
	case EDCModal::Crafting: return FilmCraftingWidget;
	default: return nullptr;
	}
}

void ADCPlayerController::OpenModal(EDCModal NewModal, UUserWidget* Widget)
{
	CloseModal();
	Modal = NewModal;
	Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ApplyInputMode();
}

void ADCPlayerController::CloseModal()
{
	if (UUserWidget* W = GetModalWidget())
	{
		W->SetVisibility(ESlateVisibility::Collapsed);
	}
	DialogueClient.Reset();
	Modal = EDCModal::None;
	ApplyInputMode();
}

void ADCPlayerController::OpenFullScreen(UUserWidget* Widget)
{
	CloseModal();
	if (ActiveFullScreen)
	{
		ActiveFullScreen->SetVisibility(ESlateVisibility::Collapsed);
	}
	ActiveFullScreen = Widget;
	bFullScreenUI = true;
	Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ApplyInputMode();
	SetPause(true);
}

void ADCPlayerController::HandleContinue()
{
	if (ActiveFullScreen)
	{
		ActiveFullScreen->SetVisibility(ESlateVisibility::Collapsed);
		ActiveFullScreen = nullptr;
	}
	bFullScreenUI = false;
	SetPause(false);
	ApplyInputMode();
}

void ADCPlayerController::HandlePlay() { HandleContinue(); }

void ADCPlayerController::TogglePause()
{
	if (bFullScreenUI)
	{
		if (ActiveFullScreen == PauseWidget)
		{
			HandleContinue();
		}
		return; // menu, summary and end screens aren't dismissed with Esc
	}
	if (PauseWidget)
	{
		OpenFullScreen(PauseWidget);
	}
}

// ---------------------------------------------------------------- Zones

void ADCPlayerController::ResetLook()
{
	LookOffset = FRotator::ZeroRotator;
	if (GetPawn())
	{
		SetControlRotation(GetPawn()->GetActorRotation());
	}
}

void ADCPlayerController::ToggleZone()
{
	if (bFullScreenUI)
	{
		return;
	}
	CloseModal();
	SwitchZone(CurrentZone == EZone::Counter ? EZone::BackRoom : EZone::Counter);
}

void ADCPlayerController::SwitchZone(EZone Target)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (bTransitioning || Target == CurrentZone || !GM || !GM->Config || !GetPawn())
	{
		return;
	}
	TArray<AActor*> Anchors;
	UGameplayStatics::GetAllActorsWithTag(this, Target == EZone::Counter ? CounterAnchorTag : BackRoomAnchorTag, Anchors);
	if (Anchors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SwitchZone: no anchor actor tagged for the target zone"));
		return;
	}
	FromTransform = GetPawn()->GetActorTransform();
	ToTransform = Anchors[0]->GetActorTransform();
	TargetZone = Target;
	TransitionTime = 0.f;
	bTransitioning = true;
	// TODO(DESIGN): the doc calls for LS_ToBackRoom / LS_ToCounter Level Sequences; this is a plain lerp until art provides them.
}

void ADCPlayerController::OnArrived()
{
	bTransitioning = false;
	CurrentZone = TargetZone;
	ResetLook();
	Cue(TEXT("ZoneChange"));
	OnZoneChanged.Broadcast(CurrentZone);
}

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config || !GetPawn())
	{
		return; // BP_DC_GameMode must have DA_GameConfig assigned
	}

	if (bTransitioning)
	{
		TransitionTime += DeltaTime;
		const float Duration = FMath::Max(GM->Config->ZoneTransitionSec, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::SmoothStep(0.f, 1.f, TransitionTime / Duration);
		FTransform T;
		T.Blend(FromTransform, ToTransform, Alpha);
		GetPawn()->SetActorTransform(T);
		SetControlRotation(T.Rotator());
		if (TransitionTime >= Duration)
		{
			OnArrived();
		}
		return;
	}
	if (Modal != EDCModal::None || bFullScreenUI)
	{
		return;
	}

	float DX = 0.f, DY = 0.f;
	GetInputMouseDelta(DX, DY);
	const float Sens = GM->Config->MouseSensitivity;
	LookOffset.Yaw = FMath::Clamp(LookOffset.Yaw + DX * Sens, -GM->Config->LookYawLimit, GM->Config->LookYawLimit);
	LookOffset.Pitch = FMath::Clamp(LookOffset.Pitch + DY * Sens, -GM->Config->LookPitchLimit, GM->Config->LookPitchLimit);
	FRotator Rot = GetPawn()->GetActorRotation() + LookOffset;
	if (ShakeLeft > 0.f)
	{
		ShakeLeft -= DeltaTime;
		const float Strength = ShakeMagnitudeDeg * FMath::Clamp(ShakeLeft / FMath::Max(ShakeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		Rot += FRotator(FMath::FRandRange(-Strength, Strength), FMath::FRandRange(-Strength, Strength), 0.f);
	}
	SetControlRotation(Rot);
}

// ---------------------------------------------------------------- Click interaction

void ADCPlayerController::OnClick()
{
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || IsBlockedByUI() || !PlayerCameraManager)
	{
		return;
	}
	const FVector Start = PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + PlayerCameraManager->GetCameraRotation().Vector() * InteractRange;
	FHitResult Hit;

	// Clients (pawns): only the one at the front of the queue can be served
	if (CurrentZone == EZone::Counter && GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_Pawn)))
	{
		ADCClient* Client = Cast<ADCClient>(Hit.GetActor());
		if (Client && Client->IsAtFront() && Client == GM->ClientDirector->GetFrontClient())
		{
			const bool bDeliver = Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0;
			DialogueClient = Client;
			DialogueWidget->Show(bDeliver ? FText::FromString(FString::Printf(TEXT("\"%s\""), *GM->FilmCrafting->HeldFilms[0].Title.ToString())) : Client->Runtime.OrderText,
				FText::FromString(bDeliver ? TEXT("Here you go") : TEXT("Coming right up")));
			OpenModal(EDCModal::Dialogue, DialogueWidget);
			Cue(TEXT("DialogueOpen"));
			return;
		}
	}

	// Props
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
	{
		return;
	}
	const ADCInteractable* Prop = Cast<ADCInteractable>(Hit.GetActor());
	if (!Prop)
	{
		return;
	}
	Cue(TEXT("Click"));
	switch (Prop->InteractType)
	{
	case EInteractType::CashRegister:
		OpenModal(EDCModal::Supplier, SupplierWidget);
		SupplierWidget->Refresh();
		break;
	case EInteractType::BackRoomDoor:
		ToggleZone();
		break;
	case EInteractType::Shelf:
		ShelfWidget->ShowGenre(Prop->ShelfGenre);
		OpenModal(EDCModal::Shelf, ShelfWidget);
		break;
	case EInteractType::VHSPlayer:
		OpenModal(EDCModal::Crafting, FilmCraftingWidget);
		FilmCraftingWidget->Refresh();
		break;
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
		}
	}
	CloseModal();
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

void ADCPlayerController::HandleFilmCreated(const FCreatedFilm& Film)
{
	Cue(TEXT("FilmCreated"));
	HitStop();
	ShowTutorial(TEXT("Bring it back to the counter"), bTutorialFilmShown);
	// TODO(art): CRT static + rewind effect and the VHS-into-player animation need the TV/player props
}

void ADCPlayerController::HandleClientReached(AActor* Client)
{
	Cue(TEXT("ClientAtCounter"));
	ShowTutorial(TEXT("Listen to what they want"), bTutorialClientShown);
}

void ADCPlayerController::HandleClientLeft(AActor* Client, bool bServed)
{
	if (bServed && TutorialWidget)
	{
		TutorialWidget->HideHint(); // the loop was understood: hide whatever sign is up
	}
}

void ADCPlayerController::HandleZoneChangedForTutorial(EZone NewZone)
{
	if (NewZone == EZone::BackRoom)
	{
		ShowTutorial(TEXT("Pick 3 tapes: Intro, Middle, Ending"), bTutorialBackRoomShown);
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
	DaySummaryWidget->Show(Summary);
	OpenFullScreen(DaySummaryWidget);
}

void ADCPlayerController::HandleOpenStore()
{
	HandleContinue();
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
	UDCEndScreenWidget* Screen = CreateWidget<UDCEndScreenWidget>(this, Cls);
	Screen->AddToViewport(20);
	Screen->Show(GetWorld()->GetAuthGameMode<ADCGameMode>()->DayNightCycle->DayIndex);
	OpenFullScreen(Screen);
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
		GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow, EClientCategory::Casual);
	}
}

void ADCPlayerController::ToggleDebugAttributes()
{
	ADCClient::bShowDebugAttributes = !ADCClient::bShowDebugAttributes; // F1: attributes and weights over each client
}
