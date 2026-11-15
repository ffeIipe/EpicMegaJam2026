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

	UPROPERTY(BlueprintReadOnly, Category = "Save") FString ActiveSlotName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels") FName MainMenuLevelName = FName(TEXT("/Game/_MainGame/Level/MAP_MainMenu"));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Levels") FName GameplayLevelName = FName(TEXT("/Game/_MainGame/Level/MAP_Rewind"));

	UFUNCTION(BlueprintPure, Category = "Save") static FString SlotNameForIndex(int32 SlotIndex);
	UFUNCTION(BlueprintPure, Category = "Save") TArray<FDCSaveSlotSummary> GetSaveSlots() const;
	UFUNCTION(BlueprintPure, Category = "Save") bool HasAnySaveSlot() const;

	UFUNCTION(BlueprintCallable, Category = "Save") UDCSaveGame* CreateNewSaveSlot(const FString& SlotName, int32 StartingMoney = 50);
	UFUNCTION(BlueprintCallable, Category = "Save") bool SelectSaveSlot(const FString& SlotName);
	// Lightweight bracket to persist creation date + account money until the full save system is implemented.
	UFUNCTION(BlueprintCallable, Category = "Save") void SaveActiveSlotMetadata(int32 CurrentMoney, int32 CurrentDay);

	// TODO(SAVE): Implement full world serialization/deserialization once demo gameplay systems are finalized.
	UFUNCTION(BlueprintCallable, Category = "Save") void ApplyActiveSaveToGameMode(ADCGameMode* GameMode) {}
	UFUNCTION(BlueprintCallable, Category = "Save") void CaptureGameModeToActiveSave(const ADCGameMode* GameMode) {}

	UFUNCTION(BlueprintCallable, Category = "Levels") void TravelToGameplay();
	UFUNCTION(BlueprintCallable, Category = "Levels") void TravelToMainMenu();
};
