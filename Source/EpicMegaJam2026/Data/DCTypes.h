// Director's Cut: enums, structs and Data Table row structs (SystemsDesigns.md sections 3, 4, 11).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCTypes.generated.h"

class UStaticMesh;
class UTexture2D;
class USkeletalMesh;

// ---------------------------------------------------------------- Enums (11.1)

UENUM(BlueprintType)
enum class EGenre : uint8 { Horror, Comedy, Romance, Action, SciFi, Drama, Paranormal };

UENUM(BlueprintType)
enum class EPolarity : uint8 { Likes, Dislikes };

UENUM(BlueprintType)
enum class EClientCategory : uint8 { Casual, Enthusiast, Cinephile };

UENUM(BlueprintType)
enum class EPhase : uint8 { Day, Night };

UENUM(BlueprintType)
enum class ESpawnPhase : uint8 { Day, Night, Any };

UENUM(BlueprintType)
enum class EClientState : uint8 { Entering, Browsing, Queued, AtCounter, Served, Leaving };

UENUM(BlueprintType)
enum class EReaction : uint8 { Loved, Liked, Neutral, Hated };

UENUM(BlueprintType)
enum class EFilmSlot : uint8 { Intro, Middle, Ending };

UENUM(BlueprintType)
enum class EInteractType : uint8 { CashRegister, BackRoomDoor, Shelf, VHSPlayer };

// Numbers shown in WBP_DaySummary (section 8)
USTRUCT(BlueprintType)
struct FDaySummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) int32 DayIndex = 1;
	UPROPERTY(BlueprintReadWrite) int32 ClientsServed = 0;
	UPROPERTY(BlueprintReadWrite) int32 ClientsLost = 0;
	UPROPERTY(BlueprintReadWrite) int32 Loved = 0;
	UPROPERTY(BlueprintReadWrite) int32 Liked = 0;
	UPROPERTY(BlueprintReadWrite) int32 Neutral = 0;
	UPROPERTY(BlueprintReadWrite) int32 Hated = 0;
	UPROPERTY(BlueprintReadWrite) int32 MoneyEarned = 0;
	UPROPERTY(BlueprintReadWrite) int32 RentPaid = 0;
	UPROPERTY(BlueprintReadWrite) int32 MoneyNow = 0;
	UPROPERTY(BlueprintReadWrite) int32 Goal = 0;
};

// ---------------------------------------------------------------- Structs (11.2)

USTRUCT(BlueprintType)
struct FGenreValue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre Genre = EGenre::Horror;
	// 1 to 3
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1, ClampMax = 3)) int32 Value = 1;
};

USTRUCT(BlueprintType)
struct FGenrePreference
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre Genre = EGenre::Horror;
	// Negative = dislikes the genre
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Weight = 0;
};

USTRUCT(BlueprintType)
struct FCreatedFilm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName IntroRow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MiddleRow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EndingRow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TMap<EGenre, float> GenreProfile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AverageRating = 0.f;
};

USTRUCT(BlueprintType)
struct FClientRuntime
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArchetypeRow;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EClientCategory Category = EClientCategory::Casual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGenrePreference> Preferences;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText OrderText;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PatienceLeft = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EClientState State = EClientState::Entering;
};

USTRUCT(BlueprintType)
struct FReview
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText FilmTitle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EReaction Reaction = EReaction::Neutral;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Score = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MoneyDelta = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Line;
};

// ---------------------------------------------------------------- Data Table rows (11.3)

USTRUCT(BlueprintType)
struct FGenreData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre Genre = EGenre::Horror;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FLinearColor Color = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ShelfLabel;
};

USTRUCT(BlueprintType)
struct FVHSData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre PrimaryGenre = EGenre::Horror;
	// 1 to 3 entries; must include PrimaryGenre with the highest value
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGenreValue> GenreValues;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1, ClampMax = 5)) int32 Rating = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CatalogId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> MeshOverride;
	// Shown on shelves and in the VHS player; empty = genre-colored placeholder
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Cover;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true)) FText Synopsis;
};

USTRUCT(BlueprintType)
struct FCatalogData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Price = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> VHSRows;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 UnlockDay = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOwnedAtStart = false;
};

USTRUCT(BlueprintType)
struct FClientCategoryData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EClientCategory Category = EClientCategory::Casual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NumAttributes = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WeightMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 WeightMax = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 0, ClampMax = 1)) float PositiveChance = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PayMultiplier = 1.f;
};

USTRUCT(BlueprintType)
struct FClientArchetypeData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DebugName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESpawnPhase SpawnPhase = ESpawnPhase::Any;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpawnWeight = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EGenre> AllowedGenres;
	// Empty = generate random preferences
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGenrePreference> FixedPreferences;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTutorialOnly = false;
};

USTRUCT(BlueprintType)
struct FHintLineData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre Genre = EGenre::Horror;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EPolarity Polarity = EPolarity::Likes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
};

// Shared by DT_Greetings and DT_FarewellLines
USTRUCT(BlueprintType)
struct FLineData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESpawnPhase SpawnPhase = ESpawnPhase::Any;
};

USTRUCT(BlueprintType)
struct FReviewLineData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EReaction Reaction = EReaction::Neutral;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
};

USTRUCT(BlueprintType)
struct FDayData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DayIndex = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DayClients = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NightClients = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RentAmount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CasualWeight = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float EnthusiastWeight = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CinephileWeight = 0.f;
};
