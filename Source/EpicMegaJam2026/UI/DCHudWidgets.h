// Step 6 widgets: HUD, review toast, end screens (code-built placeholders; art restyles in the WBP_ subclasses).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DCTypes.h"
#include "DCHudWidgets.generated.h"

class UTextBlock;
class UProgressBar;

// WBP_HUD: money, goal, day/phase, rent, held film, zone button
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCHUDWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UFUNCTION() void OnZoneButton(FName Unused);
	UPROPERTY() TObjectPtr<UTextBlock> MoneyText;
	UPROPERTY() TObjectPtr<UTextBlock> GoalText;
	UPROPERTY() TObjectPtr<UProgressBar> GoalBar;
	UPROPERTY() TObjectPtr<UTextBlock> DayText;
	UPROPERTY() TObjectPtr<UTextBlock> ClockText;
	UPROPERTY() TObjectPtr<UTextBlock> RentText;
	UPROPERTY() TObjectPtr<UTextBlock> HeldText;
	UPROPERTY() TObjectPtr<UTextBlock> ZoneText;
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
	UPROPERTY() TObjectPtr<UTextBlock> TitleText;
	UPROPERTY() TObjectPtr<UTextBlock> StarsText;
	UPROPERTY() TObjectPtr<UTextBlock> LineText;
	UPROPERTY() TObjectPtr<UTextBlock> MoneyText;
	FTimerHandle HideTimer;
};

// WBP_GameOver / WBP_Victory: final screen with day reached and restart
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCEndScreenWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title = FText::FromString(TEXT("Eviction notice"));
	UFUNCTION(BlueprintCallable) void Show(int32 DayReached);

protected:
	virtual void NativeOnInitialized() override;

private:
	UFUNCTION() void OnRestart(FName Unused);
	UPROPERTY() TObjectPtr<UTextBlock> TitleBlock;
	UPROPERTY() TObjectPtr<UTextBlock> DayBlock;
};
