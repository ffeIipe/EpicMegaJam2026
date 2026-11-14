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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> ArchetypeTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> HintLineTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> GreetingTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> FarewellTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> ReviewLineTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> DayTable;
	// v2 (section 11.3)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> ClientTierTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> ScriptedOrderTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> FeedbackLineTable;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> UnlockTable;
	// Customer personas (section 5.6)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tables") TObjectPtr<UDataTable> CustomerTable;

	// --- Skills (section 3, 4.1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills") int32 SkillCap = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills") int32 TapeSkillMax = 3;
	// Skill points of a tape by stars, index 0 = 1 star
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills") TArray<int32> StarPoints;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills") int32 TapesPerStar = 10;
	// Used by the VHS generator (section 4.2) so any skill can show up on a tape
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skills") float GeneratorBiasFloor = 0.25f;

	// --- Hints (section 5.4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hints") int32 HintHighMin = 3;

	// --- Progression and inventory (sections 4.3, 7.1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression") int32 StartUnlockedSlots = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression") int32 MaxSlots = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression") int32 StartUnlockedStarTier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression") int32 PremiumMinGenres = 6;
	// DT_VHS row -> copies. TODO(DESIGN): rows exist after step 4 (pure tapes `_01` of 1 star)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory") TMap<FName, int32> StartingTapes;
	// Price of one tape by stars, index 0 = 1 star
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory") TArray<int32> TapePriceByStar;

	// --- Scoring v2 (section 6)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float LovedMaxError = 0.10f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float LikedMaxError = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scoring") float NeutralMaxError = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 TipAtPerfect = 12;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") int32 MaxFilmTitleLength = 30;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") int32 MaxHeldFilms = 1;

	// --- Economy
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 StartMoney = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 MovieBudgetGoal = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 BaseRentalPrice = 10;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 ComplaintFine = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") int32 RentIncreaseAfterLastDay = 40;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy") float ReviewDelaySec = 15.f;

	// --- Camera
	// The camera is fixed at the counter and looks within +-limit degrees of its starting direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float LookYawLimit = 60.f;
	// How fast the camera turns towards the client while their dialogue is open (higher = quicker)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float DialogueLookSpeed = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float LookPitchLimit = 30.f;
	// Fraction of each limit (from its edge inwards) where the look eases to a stop instead of cutting. 0 = hard cut.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0", ClampMax = "1")) float LookSoftZone = 0.4f;
	// LS_ToBackRoom (art): played forward to reach the back room and in reverse to return. Without it the camera glides
	// to the actor tagged BackRoomCameraTag (or just waits) for BackRoomTransitionSec.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") TSoftObjectPtr<class ULevelSequence> BackRoomSequence;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BackRoomTransitionSec = 1.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") FName BackRoomCameraTag = "BackRoomCamera";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float MouseSensitivity = 1.f;
	// --- Player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player") float InteractRange = 450.f;

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
		StarPoints = { 3, 4, 5, 6, 7 };
		TapePriceByStar = { 4, 6, 9, 14, 20 };
		StartingTapes.Add("VHS_Horror_1_01", 2);
		StartingTapes.Add("VHS_Comedy_1_01", 2);
		StartingTapes.Add("VHS_Action_1_01", 1);
		ReviewStars.Add(EReaction::Loved, 5);
		ReviewStars.Add(EReaction::Liked, 4);
		ReviewStars.Add(EReaction::Neutral, 3);
		ReviewStars.Add(EReaction::Hated, 1);
	}
};
