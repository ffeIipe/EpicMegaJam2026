// Placeholder UMG widgets (trees built in code). Style/layout is for art to redo in WBP_ subclasses.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Data/DCTypes.h"
#include "DCWidgets.generated.h"

class UAC_Inventory;
class UAC_FilmCrafting;
class UPDA_GameConfig;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UEditableTextBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPayloadClicked, FName, Payload);

// Button that reports a name when clicked
UCLASS()
class EPICMEGAJAM2026_API UDCPayloadButton : public UButton
{
	GENERATED_BODY()
public:
	UPROPERTY() FOnPayloadClicked OnPayloadClicked;
	FName Payload;
	void Init(FName InPayload);
private:
	UFUNCTION() void HandleClicked() { OnPayloadClicked.Broadcast(Payload); }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVHSPicked, FName, VHSRow);

// WBP_Inventory: genre tabs + tape list (title and stars)
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnVHSPicked OnVHSPicked;

	UFUNCTION(BlueprintCallable) void Setup(UAC_Inventory* InInventory, UPDA_GameConfig* InConfig);
	UFUNCTION(BlueprintCallable) void Refresh();
	UFUNCTION(BlueprintCallable) void SetGenre(EGenre Genre) { SelectedGenre = Genre; Refresh(); }

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnTabClicked(FName GenreName);
	UFUNCTION() void OnTapeClicked(FName Row) { OnVHSPicked.Broadcast(Row); }

	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UHorizontalBox> Tabs;
	UPROPERTY() TObjectPtr<UScrollBox> List;
	EGenre SelectedGenre = EGenre::Horror;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBackToCounter);

// WBP_FilmCrafting: 3 slots, title, confirm, discard
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCFilmCraftingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnBackToCounter OnBackToCounter;

	UFUNCTION(BlueprintCallable) void Setup(UAC_Inventory* InInventory, UAC_FilmCrafting* InCrafting, UPDA_GameConfig* InConfig);
	UFUNCTION(BlueprintCallable) void Refresh();

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnSlotClicked(FName SlotName);
	UFUNCTION() void OnClearClicked(FName SlotName);
	UFUNCTION() void OnPicked(FName VHSRow);
	UFUNCTION() void OnConfirm(FName Unused);
	UFUNCTION() void OnDiscard(FName Unused);
	UFUNCTION() void OnBack(FName Unused) { OnBackToCounter.Broadcast(); }
	UFUNCTION() void OnFilmStateChanged() { Refresh(); }

	UDCPayloadButton* MakeButton(const FString& Label, FName Payload, class UPanelWidget* Parent, UTextBlock** OutText = nullptr);

	UPROPERTY() TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY() TObjectPtr<UAC_FilmCrafting> Crafting;
	UPROPERTY() TObjectPtr<UPDA_GameConfig> Config;
	UPROPERTY() TObjectPtr<UDCInventoryWidget> Picker;
	UPROPERTY() TObjectPtr<UEditableTextBox> TitleBox;
	UPROPERTY() TObjectPtr<UDCPayloadButton> ConfirmButton;
	UPROPERTY() TObjectPtr<UDCPayloadButton> DiscardButton;
	UPROPERTY() TObjectPtr<UTextBlock> StatusText;
	UPROPERTY() TObjectPtr<UTextBlock> HeldText;
	UPROPERTY() TMap<EFilmSlot, TObjectPtr<UTextBlock>> SlotTexts;
	EFilmSlot ActiveSlot = EFilmSlot::Intro;
};

class UProgressBar;
class UAC_ClientDirector;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueButton);

// WBP_Dialogue: client phrase + one button ("Coming right up" / "Here you go")
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCDialogueWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnDialogueButton OnButton;
	UFUNCTION(BlueprintCallable) void Show(const FText& Line, const FText& ButtonLabel);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void HandleButton(FName Unused) { OnButton.Broadcast(); }
	UPROPERTY() TObjectPtr<UTextBlock> LineText;
	UPROPERTY() TObjectPtr<UTextBlock> ButtonText;
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
	UPROPERTY() TObjectPtr<UTextBlock> OrderText;
	UPROPERTY() TObjectPtr<UProgressBar> PatienceBar;
	UPROPERTY() TObjectPtr<UWidget> PanelRoot;
};
