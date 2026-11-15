// Save-game slot payload and summary metadata.
// TODO(SAVE): Add full demo progression fields (Criterio, Inventory, CraftedFilms, Unlocks, ReviewHistory, TutorialFlags)
// once the demo gameplay systems are finalized.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Data/DCTypes.h"
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
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") bool bHasSavedGameplayState = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") bool bTutorialCompleted = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 Criterio = 0;

	// Unlocks (UAC_Unlocks)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 UnlockedStarTier = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TArray<EGenre> UnlockedGenres;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TArray<FName> OwnedUnlocks;

	// Inventory (UAC_Inventory)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TMap<FName, int32> InventoryStock;

	// Film Crafting (UAC_FilmCrafting)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 UnlockedSlots = 2;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TArray<FCreatedFilm> CraftedFilms;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TArray<FCreatedFilm> HeldFilms;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 FilmsCreated = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TArray<FString> UsedTitles;

	// Client Director (UAC_ClientDirector)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") TArray<FReview> ReviewHistory;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 ReviewStreak = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Save") int32 ScriptedUsed = 0;
};
