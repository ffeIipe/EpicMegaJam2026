// Screens: day summary, supplier, shelf popup, main menu, pause; plus the tutorial sign (code-built, styled by UI/DCCommonUI).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UI/DCCommonUI.h"
#include "Data/DCTypes.h"
#include "DCMenuWidgets.generated.h"

class UVerticalBox;
class UHorizontalBox;
class UScrollBox;
class UAC_Unlocks;
class UAC_Inventory;
class UAC_Economy;
class UAC_DayNightCycle;
class UPDA_GameConfig;
class UDCInventoryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuButton);

// WBP_DaySummary
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCDaySummaryWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UDCDaySummaryWidget(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(BlueprintAssignable) FOnMenuButton OnOpenStore;
	UFUNCTION(BlueprintCallable) void Show(const FDaySummary& Summary);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleOpen(FName Unused) { OnOpenStore.Broadcast(); }
	UPROPERTY() TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> BodyText;
};

// WBP_Supplier: the tapes on sale one copy at a time (genre unlocked, stars within the license), genre tabs, star filter, x1 / x5
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCSupplierWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;

private:
	UFUNCTION() void OnTabClicked(FName Genre) { SelectedGenre = Genre; Refresh(); }
	UFUNCTION() void OnStarClicked(FName Stars) { SelectedStars = FCString::Atoi(*Stars.ToString()); Refresh(); }
	UFUNCTION() void HandleBuyOne(FName Row);
	UFUNCTION() void HandleBuyFive(FName Row);
	UFUNCTION() void OnMoneyChanged(int32 NewMoney, int32 Delta) { UpdateAffordability(); }
	UFUNCTION() void OnUnlockPurchased(FName UnlockId) { Refresh(); }
	void CycleTab(int32 Dir);
	// Buy buttons grey out without the money, without rebuilding the list (that would reset the scroll)
	void UpdateAffordability();

	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UAC_Unlocks> Unlocks;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UHorizontalBox> Tabs;
	UPROPERTY() TObjectPtr<UHorizontalBox> StarFilter;
	UPROPERTY() TObjectPtr<UVerticalBox> List;
	struct FBuyButton { TWeakObjectPtr<UDCButton> Button; FName Row; int32 Count = 1; };
	TArray<FBuyButton> BuyButtons;
	FName SelectedGenre;
	int32 SelectedStars = 0; // 0 = all
};

// WBP_Computer: every unlock (genres, VHS player slots, star licenses) with price, requirements and a buy button (section 7.1)
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCComputerWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;

private:
	UFUNCTION() void HandleBuy(FName UnlockId);
	UFUNCTION() void OnMoneyChanged(int32 NewMoney, int32 Delta) { UpdateAffordability(); }
	UFUNCTION() void OnUnlockPurchased(FName UnlockId) { Refresh(); }
	void UpdateAffordability();

	UPROPERTY() TObjectPtr<UAC_Unlocks> Unlocks;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UVerticalBox> List;
	UPROPERTY() TObjectPtr<UScrollBox> Scroll;
	struct FUnlockButton { TWeakObjectPtr<UDCButton> Button; FName Id; };
	TArray<FUnlockButton> Buttons;
};

// Shelf popup ("VHS Inventory"): rows + detail panel, opened on the clicked shelf's genre
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCShelfWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void ShowGenre(EGenre Genre);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UDCInventoryWidget> List;
};

// Contextual tutorial sign, one at a time
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCTutorialWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void ShowHint(const FText& Text);
	UFUNCTION(BlueprintCallable) void HideHint();

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UWidget> Panel;
	UPROPERTY() TObjectPtr<UCommonTextBlock> HintText;
};

// WBP_MainMenu: Play closes it
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCMainMenuWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UDCMainMenuWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandlePlay(FName Unused) { DeactivateWidget(); }
	UFUNCTION() void HandleQuit(FName Unused);
};

class FDCKeyCapture;

// Options piece used inside the pause screen: key rebinding, master volume, mouse sensitivity. Edits UDCSettings and saves on every change.
UCLASS()
class EPICMEGAJAM2026_API UDCSettingsWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	// Stops a pending "press a key" prompt (screen closed or left the options)
	void CancelCapture();

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleRebind(FName ActionIndex);
	UFUNCTION() void HandleStep(FName Id);
	void FinishCapture(const FKey& Key);
	void Refresh();

	UPROPERTY() TArray<TObjectPtr<UDCButton>> KeyButtons; // indexed by EDCAction
	UPROPERTY() TObjectPtr<UCommonTextBlock> VolumeText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> SensitivityText;
	TSharedPtr<FDCKeyCapture> Capture;
	int32 CapturingAction = INDEX_NONE;
};

// WBP_Pause: Continue (or Esc) closes it; Options swaps the buttons for the settings and Back returns to them
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCPauseWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UDCPauseWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnDeactivated() override;
	virtual bool NativeOnHandleBackAction() override;

private:
	void ShowOptions(bool bShow);
	UFUNCTION() void HandleOptions(FName Unused) { ShowOptions(true); }
	UFUNCTION() void HandleOptionsBack(FName Unused) { ShowOptions(false); }
	UPROPERTY() TObjectPtr<UVerticalBox> MainButtons;
	UPROPERTY() TObjectPtr<UDCSettingsWidget> Settings;
	UPROPERTY() TObjectPtr<UDCButton> OptionsBackButton;
	UFUNCTION() void HandleContinue(FName Unused) { DeactivateWidget(); }
	UFUNCTION() void HandleRestart(FName Unused);
	UFUNCTION() void HandleQuit(FName Unused);
};
