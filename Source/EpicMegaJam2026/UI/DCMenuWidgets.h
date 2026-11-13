// Step 7-9 widgets: day summary, supplier, shelf popup, tutorial hint, main menu, pause (code-built placeholders).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DCTypes.h"
#include "DCMenuWidgets.generated.h"

class UTextBlock;
class UVerticalBox;
class UAC_Inventory;
class UAC_Economy;
class UAC_DayNightCycle;
class UPDA_GameConfig;
class UDCInventoryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMenuButton);

// WBP_DaySummary
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCDaySummaryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnMenuButton OnOpenStore;
	UFUNCTION(BlueprintCallable) void Show(const FDaySummary& Summary);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleOpen(FName Unused) { OnOpenStore.Broadcast(); }
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> BodyText;
};

// WBP_Supplier: catalogs with price and buy button
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCSupplierWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnMenuButton OnClose;
	UFUNCTION(BlueprintCallable) void Setup(UAC_Inventory* InInventory, UAC_Economy* InEconomy, UAC_DayNightCycle* InDayNight, UPDA_GameConfig* InConfig);
	UFUNCTION(BlueprintCallable) void Refresh();

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleBuy(FName CatalogRow);
	UFUNCTION() void HandleClose(FName Unused) { OnClose.Broadcast(); }
	UFUNCTION() void OnMoneyChanged(int32 NewMoney, int32 Delta) { Refresh(); }
	UFUNCTION() void OnInventoryChanged() { Refresh(); }
	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UAC_Economy> Economy;
	UPROPERTY() TObjectPtr<UAC_DayNightCycle> DayNight;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UVerticalBox> List;
};

// Shelf popup: inventory list filtered by the clicked shelf's genre
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCShelfWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnMenuButton OnClose;
	UFUNCTION(BlueprintCallable) void Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig);
	UFUNCTION(BlueprintCallable) void ShowGenre(EGenre Genre);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleClose(FName Unused) { OnClose.Broadcast(); }
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
	UPROPERTY() TObjectPtr<UTextBlock> HintText;
};

// WBP_MainMenu
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnMenuButton OnPlay;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandlePlay(FName Unused) { OnPlay.Broadcast(); }
	UFUNCTION() void HandleQuit(FName Unused);
};

// WBP_Pause
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCPauseWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnMenuButton OnContinue;

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleContinue(FName Unused) { OnContinue.Broadcast(); }
	UFUNCTION() void HandleRestart(FName Unused);
	UFUNCTION() void HandleQuit(FName Unused);
};
