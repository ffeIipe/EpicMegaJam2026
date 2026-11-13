// Input (rebindable movement, free mouse look), key interaction (E by default), menus and game juice.
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
class UDCRootLayout;
class UCommonActivatableWidget;
class ADCClient;
class ADCInteractable;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	// Widget classes: the code-built defaults are used when a WBP_ subclass isn't assigned.
	// Screens (dialogue, supplier, shelf, crafting, menus) are pushed on the CommonUI stack of the root layout.
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCShelfWidget> ShelfWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCTutorialWidget> TutorialWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCMainMenuWidget> MainMenuWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCPauseWidget> PauseWidgetClass;

	// Juice (tunable on BP_DC_PlayerController)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float ShakeDuration = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float ShakeMagnitudeDeg = 1.5f;
	// Entering a level actor with this tag (a trigger box over the back room) shows the "Pick tapes and combine them" hint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial") FName BackRoomZoneTag = "Zone_BackRoom";

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable) void StartShake();
	// What the interact key would do right now ("Talk to the customer"), empty when nothing usable is under the crosshair.
	UFUNCTION(BlueprintPure) FText GetInteractPrompt() const;

private:
	// Input
	void OnInteract();
	void TogglePause();
	void DebugSpawnTutorialClient();
	void ToggleDebugAttributes();

	// A screen is up: CommonUI shows the cursor and blocks game input; this also stops look, clicks and the prompt
	bool IsBlockedByUI() const;
	void HandleScreenChanged(UCommonActivatableWidget* Screen);
	void CloseScreen();

	// Game events
	UFUNCTION() void OnDialogueButton();
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
	UFUNCTION() void HandleZoneOverlap(AActor* Zone, AActor* Other);
	// The front client or the prop under the crosshair within InteractRange (at most one is set)
	void TraceTarget(ADCClient*& OutClient, const ADCInteractable*& OutProp) const;
	void ShowEndScreen(TSubclassOf<UDCEndScreenWidget> Cls);
	void ShowTutorial(const TCHAR* Text, bool& bFlag);
	void HitStop();
	void Cue(const TCHAR* Name) const;

	float ShakeLeft = 0.f;
	FRotator LookRot = FRotator::ZeroRotator; // Mouse look without the shake on top
	TWeakObjectPtr<ADCClient> DialogueClient;
	bool bTutorialClientShown = false, bTutorialBackRoomShown = false, bTutorialFilmShown = false;

	UPROPERTY() TObjectPtr<UDCOrderPanelWidget> OrderPanelWidget;
	UPROPERTY() TObjectPtr<UDCHUDWidget> HUDWidget;
	UPROPERTY() TObjectPtr<UDCReviewToastWidget> ReviewToastWidget;
	UPROPERTY() TObjectPtr<UDCTutorialWidget> TutorialWidget;
	UPROPERTY() TObjectPtr<UDCRootLayout> RootLayout;
};
