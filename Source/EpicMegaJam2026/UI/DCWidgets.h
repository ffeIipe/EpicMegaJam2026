// Code-built CommonUI widgets for the tapes: inventory (tabs + rows/tiles), VHS player crafting, dialogue, order panel.
// Style lives in UI/DCCommonUI; WBP_ subclasses can still be assigned on the player controller.
#pragma once

#include "CoreMinimal.h"
#include "UI/DCCommonUI.h"
#include "Data/DCTypes.h"
#include "DCWidgets.generated.h"

class UAC_Inventory;
class UAC_FilmCrafting;
class UAC_ClientDirector;
class UPDA_GameConfig;
class UHorizontalBox;
class UTextBlock;
class UEditableTextBox;
class UImage;
class UProgressBar;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHSPicked, FName, VHSRow);

// WBP_Inventory: genre tabs (ALL + DT_Genres, cycled with the tab actions) and the owned tapes with their copies and
// their 5 skills, as cover tiles (VHS player: copies not yet in a slot) or as rows with a detail panel (shelves: read-only stock)
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCInventoryWidget : public UCommonUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnVHSPicked OnVHSPicked;

	// InCrafting (VHS player only): copies already in a slot are not offered again
	UFUNCTION(BlueprintCallable) void Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig, bool bInGrid, UAC_FilmCrafting* InCrafting);
	UFUNCTION(BlueprintCallable) void Refresh();
	UFUNCTION(BlueprintCallable) void SetGenre(EGenre Genre);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnTabClicked(FName Tab) { SelectedTab = Tab; Refresh(); }
	UFUNCTION() void OnTapeClicked(FName Row);
	void CycleTab(int32 Dir);
	void ShowDetail(FName Row);

	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UAC_FilmCrafting> Crafting;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UHorizontalBox> Tabs;
	UPROPERTY() TObjectPtr<UPanelWidget> List;
	UPROPERTY() TObjectPtr<UWidget> Detail;
	UPROPERTY() TObjectPtr<UImage> DetailCover;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DetailTitle;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DetailStars;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DetailGenre;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DetailSkills;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DetailSynopsis;
	FName SelectedTab = "All"; // "All" or an EGenre name
	FName SelectedTape;
	bool bGrid = false;
};

// WBP_FilmCrafting ("Combine VHS"): a click on a tape fills the first empty slot (3 to 5 slots, empty and repeated allowed),
// the skill sum updates live; name it and combine
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCFilmCraftingWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void Refresh();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;

private:
	UFUNCTION() void OnClearClicked(FName SlotName);
	UFUNCTION() void OnPicked(FName VHSRow);
	UFUNCTION() void OnConfirm(FName Unused);
	UFUNCTION() void OnDiscard(FName Unused);
	UFUNCTION() void OnFilmStateChanged() { Refresh(); }

	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UAC_FilmCrafting> Crafting;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UDCInventoryWidget> Picker;
	UPROPERTY() TObjectPtr<UEditableTextBox> TitleBox;
	UPROPERTY() TObjectPtr<UDCButton> ConfirmButton;
	UPROPERTY() TObjectPtr<UDCButton> DiscardButton;
	UPROPERTY() TObjectPtr<UCommonTextBlock> SlotsHeader;
	UPROPERTY() TObjectPtr<UCommonTextBlock> StatusText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> HeldText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> ProfileText;
	// One entry per possible slot (MaxSlots); the ones past UnlockedSlots are collapsed
	UPROPERTY() TArray<TObjectPtr<UWidget>> SlotColumns;
	UPROPERTY() TArray<TObjectPtr<UCommonTextBlock>> SlotTexts;
	UPROPERTY() TArray<TObjectPtr<UImage>> SlotCovers;
	UPROPERTY() TArray<TObjectPtr<UDCButton>> SlotClears;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueButton);

// WBP_Dialogue: client phrase + one button ("Coming right up" / "Here you go")
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCDialogueWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnDialogueButton OnButton;
	UFUNCTION(BlueprintCallable) void Show(const FText& Line, const FText& ButtonLabel);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleButton(FName Unused) { OnButton.Broadcast(); }
	UPROPERTY() TObjectPtr<UCommonTextBlock> LineText;
	UPROPERTY() TObjectPtr<UDCButton> Button;
};

// WBP_OrderPanel: the front client's order and patience bar, visible in both zones
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCOrderPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void Setup(UAC_ClientDirector* InDirector) { Director = InDirector; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY() TObjectPtr<UAC_ClientDirector> Director;
	UPROPERTY() TObjectPtr<UCommonTextBlock> OrderText;
	UPROPERTY() TObjectPtr<UProgressBar> PatienceBar;
	UPROPERTY() TObjectPtr<UWidget> PanelRoot;
};
