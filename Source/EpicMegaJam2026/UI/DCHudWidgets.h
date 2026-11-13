// HUD, review toast, end screens and the floating prop label (code-built, styled by UI/DCCommonUI).
#pragma once

#include "CoreMinimal.h"
#include "UI/DCCommonUI.h"
#include "Data/DCTypes.h"
#include "DCHudWidgets.generated.h"

class UProgressBar;
class UImage;
class UBorder;

// WBP_HUD: money, goal, day/phase, rent, held film, crosshair, the interaction prompt and the tape hotbar.
// Always active: it is the CommonUI root that puts input back in game mode when no screen is up.
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCHUDWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()
public:
	UDCHUDWidget(const FObjectInitializer& ObjectInitializer);
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void UpdateHotbar(const class ADCGameMode& GM);
	UPROPERTY() TObjectPtr<UCommonTextBlock> MoneyText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> GoalText;
	UPROPERTY() TObjectPtr<UProgressBar> GoalBar;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DayText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> ClockText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> RentText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> HeldText;
	UPROPERTY() TObjectPtr<UWidget> Prompt;
	UPROPERTY() TObjectPtr<UCommonTextBlock> PromptText;
	// Bottom-center hotbar: the tapes the player carries (UAC_Inventory::Hotbar)
	UPROPERTY() TArray<TObjectPtr<UBorder>> HotbarFrames;
	UPROPERTY() TArray<TObjectPtr<UImage>> HotbarCovers;
	UPROPERTY() TArray<TObjectPtr<UCommonTextBlock>> HotbarTexts;
	TArray<FName> ShownHotbar;
	float DisplayedMoney = -1.f;
};

// WBP_ReviewToast: ReelTalk.net notification
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCReviewToastWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void ShowReview(const FReview& Review, int32 Stars, float DurationSec);

protected:
	virtual void NativeOnInitialized() override;

private:
	void Hide();
	UPROPERTY() TObjectPtr<UWidget> Panel;
	UPROPERTY() TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> StarsText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> LineText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> MoneyText;
	FTimerHandle HideTimer;
};

// WBP_GameOver / WBP_Victory: final screen with day reached and restart
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCEndScreenWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UDCEndScreenWidget(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title = FText::FromString(TEXT("Eviction notice"));
	UFUNCTION(BlueprintCallable) void Show(int32 DayReached);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnRestart(FName Unused);
	UPROPERTY() TObjectPtr<UCommonTextBlock> TitleBlock;
	UPROPERTY() TObjectPtr<UCommonTextBlock> DayBlock;
};

// Floating label over a prop (shelf name, cash register money), drawn by ADCInteractable's widget component
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCWorldLabelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void SetLabel(const FText& Text);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UWidget> Plate;
	UPROPERTY() TObjectPtr<UCommonTextBlock> LabelText;
};
