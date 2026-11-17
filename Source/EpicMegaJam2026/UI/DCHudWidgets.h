// HUD, review toast, end screens and the floating prop label (code-built, styled by UI/DCCommonUI).
#pragma once

#include "CoreMinimal.h"
#include "UI/DCCommonUI.h"
#include "Data/DCTypes.h"
#include "DCHudWidgets.generated.h"

class UProgressBar;
class UImage;
class UBorder;

// WBP_HUD: money, goal, day/phase, rent, held film, crosshair, the interaction prompt and the back-room key hint.
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
	UPROPERTY() TObjectPtr<UWidget> Prompt;
	UPROPERTY() TObjectPtr<UWidget> Crosshair;
	UPROPERTY() TObjectPtr<UCommonTextBlock> PromptText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> PromptKeyText;
	UPROPERTY() TObjectPtr<UWidget> BackRoomHint; // "[SPACE] Back room", only with the camera at the counter
	UPROPERTY() TObjectPtr<UCommonTextBlock> BackRoomKeyText;
};

// WBP_Stats: money counter, top right. A plain overlay above every screen so the number never disappears
// (it must not be the activatable HUD: that one hands input back to the game and has to stay at the bottom of the stack).
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCStatsWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY() TObjectPtr<UCommonTextBlock> MoneyText;
	float DisplayedMoney = -1.f;
	int32 LastMoney = -1;
	float Punch = 0.f;      // 1 right after the money changes, decays to 0
	bool bPunchGain = true;
};


// WBP_ReviewToast: ReelTalk.net notification
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCReviewToastWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void ShowReview(const FReview& Review, int32 Stars, float DurationSec);
	void Hide();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float Pop = 0.f;
	bool bPopGain = true;
	UPROPERTY() TObjectPtr<UWidget> Panel;
	UPROPERTY() TObjectPtr<UCommonTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> StarsText;
	UPROPERTY() TObjectPtr<UCommonTextBlock> StreakText;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title = NSLOCTEXT("DirectorsCut", "EndScreen_DefaultTitle", "Eviction notice");
	UFUNCTION(BlueprintCallable) void Show(int32 DayReached);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnRestart(FName Unused);
	UFUNCTION() void OnMainMenu(FName Unused);
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
	// Money feedback: the label jumps (up and green on a gain, down, shaking and red on a loss)
	void PunchMoney(int32 Delta);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	float Punch = 0.f;
	bool bPunchGain = true;
	UPROPERTY() TObjectPtr<UWidget> Plate;
	UPROPERTY() TObjectPtr<UCommonTextBlock> LabelText;
};

// Passive overlay shown during the intro sequence: "Hold any key to skip" + progress bar
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCIntroSkipWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable) void SetSkipProgress(float Progress);

protected:
	virtual void NativeOnInitialized() override;

private:
	UPROPERTY() TObjectPtr<UProgressBar> ProgressBar;
};
