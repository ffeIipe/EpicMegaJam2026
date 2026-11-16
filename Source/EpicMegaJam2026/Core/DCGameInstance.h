// Persistent game instance: tracks the active save slot, reads/writes slot metadata, and routes level transitions.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/DCSaveGame.h"
#include "DCGameInstance.generated.h"

class ADCGameMode;

UCLASS()
class EPICMEGAJAM2026_API UDCGameInstance : public UGameInstance
{
	GENERATED_BODY()
public:
	static constexpr int32 MaxSaveSlots = 3;
	static constexpr int32 SaveUserIndex = 0;

	// Startup logos + opening cinematic play once per launch, not when returning to the menu from gameplay
	bool bStartupIntroPlayed = false;

	virtual void Init() override;
	virtual void Shutdown() override;

	UPROPERTY(BlueprintReadOnly, Category = "Save") FString ActiveSlotName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels") FName MainMenuLevelName = FName(TEXT("/Game/_MainGame/Level/MAP_MainMenu"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels") FName GameplayLevelName = FName(TEXT("/Game/_MainGame/Level/MAP_Rewind"));

	UFUNCTION(BlueprintPure, Category = "Save") static FString SlotNameForIndex(int32 SlotIndex);
	UFUNCTION(BlueprintPure, Category = "Save") TArray<FDCSaveSlotSummary> GetSaveSlots() const;
	UFUNCTION(BlueprintPure, Category = "Save") bool HasAnySaveSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Save") UDCSaveGame* CreateNewSaveSlot(const FString& SlotName, int32 StartingMoney = 50);
	UFUNCTION(BlueprintCallable, Category = "Save") bool SelectSaveSlot(const FString& SlotName);
	UFUNCTION(BlueprintCallable, Category = "Save") bool DeleteSaveSlot(const FString& SlotName);
	UFUNCTION(BlueprintCallable, Category = "Save") void ResetActiveSaveSlotToDayOne(int32 StartingMoney = 50);
	// Lightweight bracket to persist creation date + account money until the full save system is implemented.
	UFUNCTION(BlueprintCallable, Category = "Save") void SaveActiveSlotMetadata(int32 CurrentMoney, int32 CurrentDay);
	UFUNCTION(BlueprintPure, Category = "Save") bool ShouldPlayIntro() const;
	UFUNCTION(BlueprintCallable, Category = "Save") void MarkIntroPlayed();
	UFUNCTION(BlueprintPure, Category = "Save") bool ShouldRunTutorial() const;
	UFUNCTION(BlueprintCallable, Category = "Save") void MarkTutorialCompleted();

private:
	UDCSaveGame* LoadOrCreateActiveSave() const;

	bool bIntroPlayedWithoutSlot = false;
	bool bTutorialCompletedWithoutSlot = false;

public:

	UFUNCTION(BlueprintCallable, Category = "Save") void ApplyActiveSaveToGameMode(ADCGameMode* GameMode);
	UFUNCTION(BlueprintCallable, Category = "Save") void CaptureGameModeToActiveSave(const ADCGameMode* GameMode, int32 OverrideDayIndex = -1);

	UFUNCTION(BlueprintCallable, Category = "Levels") void TravelToGameplay();
	UFUNCTION(BlueprintCallable, Category = "Levels") void TravelToMainMenu();
};
