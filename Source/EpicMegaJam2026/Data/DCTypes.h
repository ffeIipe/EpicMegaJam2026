// Director's Cut: enums, structs and Data Table row structs (SystemsDesigns.md sections 3, 4, 11).
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DCTypes.generated.h"

class UStaticMesh;
class UTexture2D;
class USkeletalMesh;

// ---------------------------------------------------------------- Enums (11.1)

// v2 (section 3). Paranormal is DEPRECATED (removed in step 8 once nothing references it); the new genres are appended
// so the stored values of the existing rows don't shift.
UENUM(BlueprintType)
enum class EGenre : uint8 { Horror, Comedy, Romance, Action, SciFi, Drama, Paranormal, Adventure, Thriller, Experimental, BMovie };

// v2 (section 3)
UENUM(BlueprintType)
enum class ESkill : uint8 { Funny, Scary, Brave, Thoughtful, Romantic };
constexpr int32 DCNumSkills = 5;

UENUM(BlueprintType)
enum class EHintLevel : uint8 { None, Some, High };

UENUM(BlueprintType)
enum class EFeedbackDirection : uint8 { TooLittle, TooMuch };

UENUM(BlueprintType)
enum class EUnlockType : uint8 { Genre, Slot, StarTier };

// DEPRECATED v1 (step 6 removes it with the preference system)
UENUM(BlueprintType)
enum class EPolarity : uint8 { Likes, Dislikes };

UENUM(BlueprintType)
enum class EPhase : uint8 { Day, Night };

UENUM(BlueprintType)
enum class ESpawnPhase : uint8 { Day, Night, Any };

UENUM(BlueprintType)
enum class EClientState : uint8 { Entering, Browsing, Queued, AtCounter, Served, Leaving };

UENUM(BlueprintType)
enum class EReaction : uint8 { Loved, Liked, Neutral, Hated };

UENUM(BlueprintType)
enum class EInteractType : uint8 { CashRegister, BackRoomDoor, Shelf, VHSPlayer, Computer }; // v2.1: BackRoomDoor, Shelf and VHSPlayer are unused, kept so saved values don't shift

// Where the camera is (section 9): the player never walks, a sequence carries the camera to the back room and back
UENUM(BlueprintType)
enum class ECameraZone : uint8 { Counter, ToBackRoom, BackRoom, ToCounter };

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

// One integer per skill: a tape's skills (0..TapeSkillMax), a film's profile and an order's target (0..SkillCap).
USTRUCT(BlueprintType)
struct FSkillVector
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Funny = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Scary = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Brave = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Thoughtful = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Romantic = 0;

	int32 Get(ESkill Skill) const
	{
		switch (Skill)
		{
		case ESkill::Funny: return Funny;
		case ESkill::Scary: return Scary;
		case ESkill::Brave: return Brave;
		case ESkill::Thoughtful: return Thoughtful;
		default: return Romantic;
		}
	}

	void Set(ESkill Skill, int32 Value)
	{
		switch (Skill)
		{
		case ESkill::Funny: Funny = Value; break;
		case ESkill::Scary: Scary = Value; break;
		case ESkill::Brave: Brave = Value; break;
		case ESkill::Thoughtful: Thoughtful = Value; break;
		default: Romantic = Value; break;
		}
	}
};

USTRUCT(BlueprintType)
struct FHintedSkill
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESkill Skill = ESkill::Funny;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EHintLevel Level = EHintLevel::High;
};

USTRUCT(BlueprintType)
struct FCreatedFilm
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	// v2: one DT_VHS row per slot (NAME_None = empty slot), sized to UnlockedSlots
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> SlotRows;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSkillVector SkillProfile;
};

USTRUCT(BlueprintType)
struct FClientRuntime
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArchetypeRow;
	// Persona name from DT_Customers (section 5.6); empty when no persona was picked
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	// v2: client level 1 to 5, the hidden skill target and the skills the dialogue hints at
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Tier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSkillVector Target;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHintedSkill> HintedSkills;
	// Multiplies the tip: the persona's TipMult (section 5.6), 1 without a persona
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TipMult = 1.f;
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
	// v2 (section 6): Miss / total target, and 1 - that clamped to 0..1
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ErrorRatio = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Accuracy = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MoneyDelta = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Line;
	// Empty when the film was Loved
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText FeedbackLine;
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
	// v2 (sections 3 and 4.2): weights (0 to 3) the VHS generator uses to spread a tape's points
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSkillVector SkillBias;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUnlockedAtStart = false;
};

USTRUCT(BlueprintType)
struct FVHSData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre PrimaryGenre = EGenre::Horror;
	// v2 (section 4.1): 0 to TapeSkillMax each; the sum is StarPoints[Rating]
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSkillVector Skills;
	// v2: the stars of the tape
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1, ClampMax = 5)) int32 Rating = 3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> MeshOverride;
	// Shown on shelves and in the VHS player; empty = genre-colored placeholder
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UTexture2D> Cover;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (MultiLine = true)) FText Synopsis;
};

// DT_ClientTiers (section 5.1)
USTRUCT(BlueprintType)
struct FClientTierData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = 1, ClampMax = 5)) int32 Tier = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DebugName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RecipeTapesMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RecipeTapesMax = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RecipeStarMin = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RecipeStarMax = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxHintedSkills = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PayMultiplier = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpawnWeight = 1.f;
};

// DT_ScriptedOrders (section 5.4): the first clients of a run
USTRUCT(BlueprintType)
struct FScriptedOrderData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Index = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSkillVector Target;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FHintedSkill> HintedSkills;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ArchetypeRow;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTutorialOnly = false;
};

USTRUCT(BlueprintType)
struct FHintLineData : public FTableRowBase
{
	GENERATED_BODY()

	// v2 (section 5.4)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESkill Skill = ESkill::Funny;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EHintLevel Level = EHintLevel::High;
	// Rows of SciFi and Paranormal have no skill: kept in the table but never picked
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUnused = false;
	// DEPRECATED v1: Genre, Polarity (only to tell which rows were migrated)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre Genre = EGenre::Horror;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EPolarity Polarity = EPolarity::Likes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
};

// DT_FeedbackLines (section 6.3)
USTRUCT(BlueprintType)
struct FFeedbackLineData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESkill Skill = ESkill::Funny;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFeedbackDirection Direction = EFeedbackDirection::TooLittle;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Text;
};

// DT_Unlocks (section 7.1)
USTRUCT(BlueprintType)
struct FUnlockData : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName UnlockId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EUnlockType Type = EUnlockType::Genre;
	// Only for Type = Genre
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EGenre Genre = EGenre::Romance;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Description;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Price = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> RequiresUnlocks;
	// 0 = no requirement
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinGenresOwned = 0;
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
};
