// Startup intro screen: Unreal, FMOD and studio cards, then the opening cinematic (code-built, styled by UI/DCCommonUI).
#pragma once

#include "CoreMinimal.h"
#include "UI/DCCommonUI.h"
#include "DCIntroWidget.generated.h"

class UImage;
class UMediaPlayer;
class UMediaTexture;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnIntroFinished);

// WBP_Intro: cards (Unreal, FMOD, Ampersand Games) then Content/Movies/Intro.mp4. Any key or click skips.
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API UDCIntroWidget : public UDCScreen
{
	GENERATED_BODY()
public:
	UDCIntroWidget(const FObjectInitializer& ObjectInitializer);
	UPROPERTY(BlueprintAssignable) FOnIntroFinished OnFinished;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void ShowSlide(int32 Index);
	void StartVideo();
	void Skip();
	UFUNCTION() void Finish();
	UFUNCTION() void HandleVideoFailed(FString Url) { Finish(); }

	UPROPERTY() TArray<TObjectPtr<UWidget>> Cards;
	UPROPERTY() TObjectPtr<UWidget> VideoLayer;
	UPROPERTY() TObjectPtr<UImage> VideoImage;
	UPROPERTY() TObjectPtr<UMediaPlayer> MediaPlayer;
	UPROPERTY() TObjectPtr<UMediaTexture> MediaTexture;
	int32 Slide = INDEX_NONE;
	float SlideTime = 0.f;
	bool bVideoStarted = false;
	bool bFinished = false;
};
