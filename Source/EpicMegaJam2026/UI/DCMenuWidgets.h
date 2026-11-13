// Screens: day summary, supplier, shelf popup, main menu, pause; plus the tutorial sign (code-built, styled by UI/DCCommonUI).
#pragma once

#include "CoreMinimal.h"
#include "UI/DCCommonUI.h"
#include "Data/DCTypes.h"
#include "DCMenuWidgets.generated.h"

class UVerticalBox;
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

// WBP_Supplier: catalogs with price and buy button
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
	UFUNCTION() void HandleBuy(FName CatalogRow);
	UFUNCTION() void OnMoneyChanged(int32 NewMoney, int32 Delta) { Refresh(); }
	UFUNCTION() void OnInventoryChanged() { Refresh(); }
	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UAC_DayNightCycle> DayNight;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UVerticalBox> List;
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
	UFUNCTION() void OnTapePicked(FName VHSRow);
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

// WBP_Pause: Continue (or Esc) closes it
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCPauseWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UDCPauseWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleContinue(FName Unused) { DeactivateWidget(); }
	UFUNCTION() void HandleRestart(FName Unused);
	UFUNCTION() void HandleQuit(FName Unused);
};
