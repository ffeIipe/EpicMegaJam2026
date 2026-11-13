#include "Core/DCPlayerController.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/DCInteractable.h"
#include "UI/DCWidgets.h"
#include "UI/DCHudWidgets.h"
#include "UI/DCMenuWidgets.h"
#include "UI/DCCommonUI.h"
#include "Widgets/CommonActivatableWidgetContainer.h"
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
	LookRot = GetPawn() ? GetPawn()->GetActorRotation() : FRotator::ZeroRotator;
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
	GM->Economy->OnGameOver.AddDynamic(this, &ADCPlayerController::HandleGameOver);
	GM->Economy->OnVictory.AddDynamic(this, &ADCPlayerController::HandleVictory);
	GM->Economy->OnRentCharged.AddDynamic(this, &ADCPlayerController::HandleRentCharged);
	GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCPlayerController::HandleMoneyChanged);
	GM->DayNightCycle->OnDayClosed.AddDynamic(this, &ADCPlayerController::HandleDayClosed);
	GM->DayNightCycle->OnPhaseChanged.AddDynamic(this, &ADCPlayerController::HandlePhaseChanged);
	TArray<AActor*> Zones;
	UGameplayStatics::GetAllActorsWithTag(this, BackRoomZoneTag, Zones);
	for (AActor* Zone : Zones)
	{
		Zone->OnActorBeginOverlap.AddDynamic(this, &ADCPlayerController::HandleZoneOverlap);
	}

	// Unattended runs (automated tests) skip the menu so the game starts by itself
	if (GM->Config->bShowMainMenu && !FApp::IsUnattended())
	{
		PushScreen<UDCMainMenuWidget>(RootLayout, MainMenuWidgetClass);
	}
}

void ADCPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ADCPlayerController::OnClick);
	for (const FKey& Key : {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine})
	{
		InputComponent->BindKey(Key, IE_Pressed, this, &ADCPlayerController::OnHotbarKey);
	}
	InputComponent->BindKey(EKeys::Q, IE_Pressed, this, &ADCPlayerController::ReturnSelectedTape);
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

// ---------------------------------------------------------------- Movement and look

void ADCPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config || !GetPawn())
	{
		return; // BP_DC_GameMode must have DA_GameConfig assigned
	}
	if (IsBlockedByUI())
	{
		return;
	}

	float DX = 0.f, DY = 0.f;
	GetInputMouseDelta(DX, DY);
	const float Sens = GM->Config->MouseSensitivity;
	LookRot.Yaw = FRotator::NormalizeAxis(LookRot.Yaw + DX * Sens);
	LookRot.Pitch = FMath::Clamp(LookRot.Pitch + DY * Sens, -GM->Config->LookPitchLimit, GM->Config->LookPitchLimit);
	FRotator Rot = LookRot;
	if (ShakeLeft > 0.f)
	{
		ShakeLeft -= DeltaTime;
		const float Strength = ShakeMagnitudeDeg * FMath::Clamp(ShakeLeft / FMath::Max(ShakeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
		Rot += FRotator(FMath::FRandRange(-Strength, Strength), FMath::FRandRange(-Strength, Strength), 0.f);
	}
	SetControlRotation(Rot);

	const FRotationMatrix YawMatrix(FRotator(0.f, LookRot.Yaw, 0.f));
	const FVector Move = YawMatrix.GetScaledAxis(EAxis::X) * ((IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f))
		+ YawMatrix.GetScaledAxis(EAxis::Y) * ((IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f));
	if (!Move.IsNearlyZero())
	{
		GetPawn()->AddMovementInput(Move.GetSafeNormal());
	}
}

// ---------------------------------------------------------------- Click interaction

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
		OutProp = Cast<ADCInteractable>(Hit.GetActor());
	}
}

FText ADCPlayerController::GetInteractPrompt() const
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || IsBlockedByUI())
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

void ADCPlayerController::OnClick()
{
	ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || IsBlockedByUI())
	{
		return;
	}
	ADCClient* Client;
	const ADCInteractable* Prop;
	TraceTarget(Client, Prop);
	if (Client)
	{
		const bool bDeliver = Client->bOrderAcknowledged && GM->FilmCrafting->HeldFilms.Num() > 0;
		UDCDialogueWidget* Dialogue = PushScreen<UDCDialogueWidget>(RootLayout, DialogueWidgetClass);
		Dialogue->OnButton.AddUniqueDynamic(this, &ADCPlayerController::OnDialogueButton); // pooled screens come back already bound
		Dialogue->Show(bDeliver ? FText::FromString(FString::Printf(TEXT("\"%s\""), *GM->FilmCrafting->HeldFilms[0].Title.ToString())) : Client->Runtime.OrderText,
			FText::FromString(bDeliver ? TEXT("Here you go") : TEXT("Coming right up")));
		DialogueClient = Client;
		Cue(TEXT("DialogueOpen"));
		return;
	}
	if (!Prop)
	{
		return;
	}
	Cue(TEXT("Click"));
	switch (Prop->InteractType)
	{
	case EInteractType::CashRegister:
		PushScreen<UDCSupplierWidget>(RootLayout, SupplierWidgetClass);
		break;
	case EInteractType::BackRoomDoor: // legacy value, kept so saved InteractType values don't shift: the door is walked through now
		break;
	case EInteractType::Shelf:
		PushScreen<UDCShelfWidget>(RootLayout, ShelfWidgetClass)->ShowGenre(Prop->ShelfGenre);
		break;
	case EInteractType::VHSPlayer:
		PushScreen<UDCFilmCraftingWidget>(RootLayout, FilmCraftingWidgetClass);
		break;
	}
}

void ADCPlayerController::OnHotbarKey(FKey Key)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || IsBlockedByUI())
	{
		return;
	}
	static const TArray<FKey> Keys = {EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
	const int32 Index = Keys.IndexOfByKey(Key);
	if (GM->Inventory->Hotbar.IsValidIndex(Index))
	{
		GM->Inventory->SelectedHotbar = Index;
	}
}

void ADCPlayerController::ReturnSelectedTape()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (GM && !IsBlockedByUI() && GM->Inventory->Hotbar.IsValidIndex(GM->Inventory->SelectedHotbar))
	{
		GM->Inventory->ReturnTape(GM->Inventory->Hotbar[GM->Inventory->SelectedHotbar]);
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
	if (Client && Client == DialogueClient.Get())
	{
		CloseScreen(); // ran out of patience mid-conversation: don't leave their dialogue up
	}
	if (bServed && TutorialWidget)
	{
		TutorialWidget->HideHint(); // the loop was understood: hide whatever sign is up
	}
}

void ADCPlayerController::HandleZoneOverlap(AActor* Zone, AActor* Other)
{
	// Only once a customer has asked for something, so the hint lands when it's useful
	if (Other == GetPawn() && bTutorialClientShown)
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
		GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow, EClientCategory::Casual);
	}
}

void ADCPlayerController::ToggleDebugAttributes()
{
	ADCClient::bShowDebugAttributes = !ADCClient::bShowDebugAttributes; // F1: attributes and weights over each client
}
