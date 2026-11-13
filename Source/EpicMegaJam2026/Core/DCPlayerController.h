// Input, limited mouse look, zone switching, click interaction, menus and game juice.
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
class UDCShelfWidget;
class UDCTutorialWidget;
class UDCMainMenuWidget;
class UDCPauseWidget;
class ADCClient;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnZoneChanged, EZone, NewZone);

UENUM()
enum class EDCModal : uint8 { None, Dialogue, Supplier, Shelf, Crafting };

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	// Widget classes: the code-built defaults are used when a WBP_ subclass isn't assigned.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCFilmCraftingWidget> FilmCraftingWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCDialogueWidget> DialogueWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCOrderPanelWidget> OrderPanelWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCHUDWidget> HUDWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCReviewToastWidget> ReviewToastWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCEndScreenWidget> GameOverWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCEndScreenWidget> VictoryWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCDaySummaryWidget> DaySummaryWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCSupplierWidget> SupplierWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCShelfWidget> ShelfWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCTutorialWidget> TutorialWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCMainMenuWidget> MainMenuWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCPauseWidget> PauseWidgetClass;

	// Level actors carrying these tags define where the camera sits in each zone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zones") FName CounterAnchorTag = "Zone_Counter";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zones") FName BackRoomAnchorTag = "Zone_BackRoom";

	// Juice (tunable on BP_DC_PlayerController)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float ShakeDuration = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float ShakeMagnitudeDeg = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction") float InteractRange = 3000.f;

	UPROPERTY(BlueprintAssignable) FOnZoneChanged OnZoneChanged;
	UPROPERTY(BlueprintReadOnly) EZone CurrentZone = EZone::Counter;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// Re-anchor the look limits to the pawn's current rotation (call after a zone change).
	UFUNCTION(BlueprintCallable) void ResetLook();
	// Moves the camera to the other zone over ZoneTransitionSec; look input is blocked meanwhile.
	UFUNCTION(BlueprintCallable) void SwitchZone(EZone Target);
	UFUNCTION(BlueprintCallable) void ToggleZone();
	UFUNCTION(BlueprintCallable) void StartShake();

private:
	// Input
	void OnClick();
	void TogglePause();
	void DebugSpawnTutorialClient();
	void ToggleDebugAttributes();

	// Modal panels (dialogue, supplier, shelf, crafting): cursor shown, look blocked
	void OpenModal(EDCModal Modal, UUserWidget* Widget);
	UFUNCTION() void CloseModal();
	void ApplyInputMode();
	bool IsBlockedByUI() const { return Modal != EDCModal::None || bFullScreenUI || bTransitioning; }
	UUserWidget* GetModalWidget() const;

	// Game events
	UFUNCTION() void OnDialogueButton();
	UFUNCTION() void HandleReview(const FReview& Review);
	UFUNCTION() void HandleGameOver();
	UFUNCTION() void HandleVictory();
	UFUNCTION() void HandleDayClosed(const FDaySummary& Summary);
	UFUNCTION() void HandleOpenStore();
	UFUNCTION() void HandlePlay();
	UFUNCTION() void HandleContinue();
	UFUNCTION() void HandleClientReached(AActor* Client);
	UFUNCTION() void HandleClientLeft(AActor* Client, bool bServed);
	UFUNCTION() void HandleFilmCreated(const FCreatedFilm& Film);
	UFUNCTION() void HandleRentCharged(int32 Amount);
	UFUNCTION() void HandlePhaseChanged(EPhase Phase, int32 DayIndex);
	UFUNCTION() void HandleMoneyChanged(int32 NewMoney, int32 Delta);
	UFUNCTION() void HandleZoneChangedForTutorial(EZone NewZone);
	void ShowEndScreen(TSubclassOf<UDCEndScreenWidget> Cls);
	void OpenFullScreen(UUserWidget* Widget);
	void ShowTutorial(const TCHAR* Text, bool& bFlag);
	void OnArrived();
	void HitStop();
	void Cue(const TCHAR* Name) const;

	EDCModal Modal = EDCModal::None;
	bool bFullScreenUI = false; // main menu, pause, summary or end screen is up
	bool bTransitioning = false;
	float TransitionTime = 0.f;
	float ShakeLeft = 0.f;
	EZone TargetZone = EZone::Counter;
	FRotator LookOffset = FRotator::ZeroRotator; // Pitch/Yaw offset from the base rotation
	FTransform FromTransform, ToTransform;
	TWeakObjectPtr<ADCClient> DialogueClient;
	bool bTutorialClientShown = false, bTutorialBackRoomShown = false, bTutorialFilmShown = false;

	UPROPERTY() TObjectPtr<UDCFilmCraftingWidget> FilmCraftingWidget;
	UPROPERTY() TObjectPtr<UDCDialogueWidget> DialogueWidget;
	UPROPERTY() TObjectPtr<UDCOrderPanelWidget> OrderPanelWidget;
	UPROPERTY() TObjectPtr<UDCHUDWidget> HUDWidget;
	UPROPERTY() TObjectPtr<UDCReviewToastWidget> ReviewToastWidget;
	UPROPERTY() TObjectPtr<UDCDaySummaryWidget> DaySummaryWidget;
	UPROPERTY() TObjectPtr<UDCSupplierWidget> SupplierWidget;
	UPROPERTY() TObjectPtr<UDCShelfWidget> ShelfWidget;
	UPROPERTY() TObjectPtr<UDCTutorialWidget> TutorialWidget;
	UPROPERTY() TObjectPtr<UDCMainMenuWidget> MainMenuWidget;
	UPROPERTY() TObjectPtr<UDCPauseWidget> PauseWidget;
	UPROPERTY() TObjectPtr<UUserWidget> ActiveFullScreen;
};
