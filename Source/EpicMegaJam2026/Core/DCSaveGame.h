// Save-game slot payload and summary metadata.
// TODO(SAVE): Add full demo progression fields (Criterio, Inventory, CraftedFilms, Unlocks, ReviewHistory, TutorialFlags)
// once the demo gameplay systems are finalized.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DCSaveGame.generated.h"

USTRUCT(BlueprintType)
struct EPICMEGAJAM2026_API FDCSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Save") FString SlotName;
	UPROPERTY(BlueprintReadOnly, Category = "Save") int32 SlotIndex = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Save") bool bExists = false;
	UPROPERTY(BlueprintReadOnly, Category = "Save") FDateTime CreatedAt;
	UPROPERTY(BlueprintReadOnly, Category = "Save") FDateTime LastSavedAt;
	UPROPERTY(BlueprintReadOnly, Category = "Save") int32 Money = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Save") int32 DayIndex = 1;
};

UCLASS()
class EPICMEGAJAM2026_API UDCSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") FString SlotName;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") FDateTime CreatedAt;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") FDateTime LastSavedAt;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 Money = 50;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 DayIndex = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") bool bIntroPlayed = false;

	// TODO(SAVE): Full gameplay state fields go here when implementing complete save/load for the demo.
};
