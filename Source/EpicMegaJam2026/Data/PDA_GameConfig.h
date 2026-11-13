// Director's Cut: all balance values live here (SystemsDesigns.md section 11.4). Defaults are the initial values.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DCTypes.h"
#include "PDA_GameConfig.generated.h"

UCLASS(BlueprintType)
class EPICMEGAJAM2026_API UPDA_GameConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- Data Tables (assigned in DA_GameConfig)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> GenreTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> VHSTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> CatalogTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> CategoryTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> ArchetypeTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> HintLineTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> GreetingTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> FarewellTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> ReviewLineTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> DayTable;

	// --- Time
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time") float DayDurationSec = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time") float NightDurationSec = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time") float NightTransitionSec = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time") float FirstClientDelaySec = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Time") float SpawnJitterSec = 5.f;

	// --- Clients
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") float ClientPatienceBaseSec = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") float ClientPatienceMaxSec = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") int32 MaxQueueSize = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") float BrowseTimeMinSec = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") float BrowseTimeMaxSec = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") int32 LeavePenalty = 0;

	// --- Criterio
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Criterio") int32 CriterioPerLevel = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Criterio") float CriterioPatienceBonusSec = 10.f;

	// --- Crafting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TMap<EFilmSlot, float> SlotMultipliers;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") int32 MaxFilmTitleLength = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") int32 MaxHeldFilms = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") bool bShowSecondaryGenres = false;

	// --- Scoring
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float AvoidBonus = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float RatingStep = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float LovedThreshold = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float LikedThreshold = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float NeutralThreshold = 0.f;

	// --- Economy
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 StartMoney = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 MovieBudgetGoal = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 BaseRentalPrice = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float MoneyPerPoint = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 ComplaintFine = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 RentIncreaseAfterLastDay = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float ReviewDelaySec = 15.f;

	// --- Camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float LookYawLimit = 90.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float LookPitchLimit = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float MouseSensitivity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ZoneTransitionSec = 1.5f;

	// --- Clock (in-store fictional hours: day 10:00-22:00, night 22:00-04:00)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock") float ClockDayStartHour = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock") float ClockDayEndHour = 22.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clock") float ClockNightEndHour = 4.f;

	// --- Lighting helper: SkyLight intensity as a fraction of the directional light's (TODO(DESIGN): art may want its own presets)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting") float SkyLightIntensityRatio = 0.1f;

	// --- Menus
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") bool bShowMainMenu = true;

	// --- Review toast
	// TODO(DESIGN): the doc says "1 to 5 stars by reaction" without a mapping; these are placeholders
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TMap<EReaction, int32> ReviewStars;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") float ToastDurationSec = 4.f;

	// --- Juice
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Juice") float HitStopSec = 0.08f;

	// --- UI
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") bool bShowTutorial = true;

	// --- Lighting (TODO(DESIGN): final values from art)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting") FLinearColor DayLightColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting") float DayLightIntensity = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting") FLinearColor NightLightColor = FLinearColor(0.1f, 0.15f, 0.4f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting") float NightLightIntensity = 1.f;

	UPDA_GameConfig()
	{
		SlotMultipliers.Add(EFilmSlot::Intro, 1.f);
		SlotMultipliers.Add(EFilmSlot::Middle, 1.f);
		SlotMultipliers.Add(EFilmSlot::Ending, 1.f);
		ReviewStars.Add(EReaction::Loved, 5);
		ReviewStars.Add(EReaction::Liked, 4);
		ReviewStars.Add(EReaction::Neutral, 3);
		ReviewStars.Add(EReaction::Hated, 1);
	}
};
