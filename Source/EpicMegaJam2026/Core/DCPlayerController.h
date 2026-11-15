// Input (mouse look within a limited range, rebindable keys), the camera zone (counter <-> back room), the order dialogue
// that opens by itself, menus and game juice. The player never walks (SystemsDesigns.md section 9).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Data/DCTypes.h"
#include "DCPlayerController.generated.h"

class UDCFilmCraftingWidget;
class UDCDialogueWidget;
class UDCOrderPanelWidget;
class UDCHUDWidget;
class UDCReviewToastWidget;
class UDCEndScreenWidget;
class UDCDaySummaryWidget;
class UDCSupplierWidget;
class UDCComputerWidget;
class UDCShelfWidget;
class UDCTutorialWidget;
class UDCMainMenuWidget;
class UDCPauseWidget;
class UDCIntroSkipWidget;
class UDCRootLayout;
class UCommonActivatableWidget;
class ULevelSequencePlayer;
class ADCClient;
class ADCInteractable;

UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	Inactive,
	WaitForClient,
	ApproachClient,
	DialogueOrder,
	GoToBackRoom,
	CombinePick,
	DeliverFilm,
	WaitReview,
	ComputerBuyGenre,
	SupplierBuyTape,
	Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCameraZoneChanged, ECameraZone, Zone);

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	// Widget classes: the code-built defaults are used when a WBP_ subclass isn't assigned.
	// Screens (dialogue, supplier, crafting, menus) are pushed on the CommonUI stack of the root layout.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCFilmCraftingWidget> FilmCraftingWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCDialogueWidget> DialogueWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCOrderPanelWidget> OrderPanelWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCHUDWidget> HUDWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCReviewToastWidget> ReviewToastWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCEndScreenWidget> GameOverWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCEndScreenWidget> VictoryWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCDaySummaryWidget> DaySummaryWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCSupplierWidget> SupplierWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCComputerWidget> ComputerWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCShelfWidget> ShelfWidgetClass; // unused since v2.1 (shelves aren't interactable)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCTutorialWidget> TutorialWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCMainMenuWidget> MainMenuWidgetClass; // unused since MainMenu moved to MAP_MainMenu
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCPauseWidget> PauseWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCIntroSkipWidget> IntroSkipWidgetClass;

	// Juice (tunable on BP_DC_PlayerController)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float ShakeDuration = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float ShakeMagnitudeDeg = 1.5f;

	UPROPERTY(BlueprintAssignable) FOnCameraZoneChanged OnCameraZoneChanged;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable) void StartShake();
	// What the interact key would do right now ("Talk to the customer"), empty when nothing usable is under the crosshair.
	UFUNCTION(BlueprintPure) FText GetInteractPrompt() const;
	UFUNCTION(BlueprintPure) ECameraZone GetCameraZone() const { return CameraZone; }
	// Only at the counter, with no screen up
	UFUNCTION(BlueprintPure) bool CanGoToBackRoom() const;
	// Plays the camera sequence to the back room; the crafting panel opens when it ends. Closing the panel brings the camera back.
	UFUNCTION(BlueprintCallable) void GoToBackRoom();
	UFUNCTION(BlueprintPure) ETutorialStep GetTutorialStep() const { return TutorialStep; }
	UFUNCTION(BlueprintPure) bool IsTutorialActive() const { return TutorialStep != ETutorialStep::Inactive && TutorialStep != ETutorialStep::Complete; }
	UFUNCTION(BlueprintPure) bool IsPlayingIntro() const { return bPlayingIntro; }
	UFUNCTION(BlueprintCallable) void SkipTutorial();
	UFUNCTION(BlueprintCallable) bool OpenInteractableScreen(EInteractType Type);
	UFUNCTION(BlueprintCallable) void CloseScreen();
	UFUNCTION(BlueprintCallable) void OpenDialogue(ADCClient* Client);
	UFUNCTION() void OnDialogueButton();

private:
	// Input
	void OnInteract();
	void TogglePause();
	void DebugSpawnTutorialClient();
	void ToggleDebugAttributes();

	// A screen is up: CommonUI shows the cursor and blocks game input; this also stops look, clicks and the prompt
	bool IsBlockedByUI() const;
	void HandleScreenChanged(UCommonActivatableWidget* Screen);

	// Camera zone: Counter -> ToBackRoom -> BackRoom -> ToCounter -> Counter
	void SetCameraZone(ECameraZone Zone);
	void BeginTransition(bool bForward);
	void FinishTransition();
	void TickFallbackTransition(float DeltaTime);
	void LookAtDialogueClient(float DeltaTime);
	ULevelSequencePlayer* GetSequencePlayer();
	UFUNCTION() void HandleSequenceFinished();

	void RequestDialogue();
	void TryOpenDialogue();

	// Game events
	UFUNCTION() void OnDialogueReject();
	UFUNCTION() void HandleReview(const FReview& Review);
	UFUNCTION() void HandleGameOver();
	UFUNCTION() void HandleVictory();
	UFUNCTION() void HandleDayClosed(const FDaySummary& Summary);
	UFUNCTION() void HandleOpenStore();
	UFUNCTION() void HandleClientReached(AActor* Client);
	UFUNCTION() void HandleClientLeft(AActor* Client, bool bServed);
	UFUNCTION() void HandleFilmCreated(const FCreatedFilm& Film);
	UFUNCTION() void HandleUnlockPurchased(FName UnlockId);
	UFUNCTION() void HandleRentCharged(int32 Amount);
	UFUNCTION() void HandlePhaseChanged(EPhase Phase, int32 DayIndex);
	UFUNCTION() void HandleMoneyChanged(int32 NewMoney, int32 Delta);
	UFUNCTION() void HandleTutorialFilmStateChanged();
	UFUNCTION() void HandleTutorialTapePurchased(FName Row, int32 Count);
	UFUNCTION() void HandleTutorialSkipped();
	// The front client or the prop under the crosshair within InteractRange (at most one is set)
	void TraceTarget(ADCClient*& OutClient, const ADCInteractable*& OutProp) const;
	void ShowEndScreen(TSubclassOf<UDCEndScreenWidget> Cls);
	void SetTutorialStep(ETutorialStep NextStep);
	void RefreshTutorial();
	void EndTutorialPause();
	void CompleteTutorial();
	void DismissCompletedTutorial();
	void HitStop();
	void Cue(const TCHAR* Name) const;
	void TryStartIntro();
	void TickIntro(float DeltaTime);
	bool IsAnyDigitalKeyDown() const;
	UFUNCTION() void FinishIntro();

	bool bPlayingIntro = false;
	float IntroSkipHoldElapsed = 0.f;
	float ShakeLeft = 0.f;
	FRotator CounterRot = FRotator::ZeroRotator; // Starting direction: the look range is centred on it
	FRotator LookRot = FRotator::ZeroRotator; // Mouse look without the shake on top
	FVector2D LookRaw = FVector2D::ZeroVector; // Accumulated mouse offset (yaw, pitch) from CounterRot, before the soft limit
	TWeakObjectPtr<ADCClient> DialogueClient;
	ETutorialStep TutorialStep = ETutorialStep::Inactive;
	bool bTutorialGenreBought = false;
	bool bTutorialTapeAfterGenreBought = false;
	FTimerHandle TutorialCompleteHideTimer;

	ECameraZone CameraZone = ECameraZone::Counter;
	bool bDialogueDue = false; // a dialogue should open as soon as nothing blocks it
	// Placeholder transition (no BackRoomSequence assigned): glide between two poses
	bool bFallbackTransition = false;
	bool bFallbackForward = true;
	float TransitionTime = 0.f;
	FVector CounterLoc = FVector::ZeroVector;
	FVector BackRoomLoc = FVector::ZeroVector;
	FRotator BackRoomRot = FRotator::ZeroRotator;

	UPROPERTY() TObjectPtr<ULevelSequencePlayer> SequencePlayer;
	UPROPERTY() TObjectPtr<ULevelSequencePlayer> IntroSequencePlayer;
	UPROPERTY() TObjectPtr<UDCOrderPanelWidget> OrderPanelWidget;
	UPROPERTY() TObjectPtr<UDCHUDWidget> HUDWidget;
	UPROPERTY() TObjectPtr<UDCReviewToastWidget> ReviewToastWidget;
	UPROPERTY() TObjectPtr<UDCTutorialWidget> TutorialWidget;
	UPROPERTY() TObjectPtr<UDCIntroSkipWidget> IntroSkipWidget;
	UPROPERTY() TObjectPtr<UDCRootLayout> RootLayout;
};
