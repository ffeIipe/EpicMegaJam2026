// Pure scoring functions (SystemsDesigns.md section 6). No state; every tunable comes from the config.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/DCTypes.h"
#include "Data/PDA_GameConfig.h"
#include "BPFL_Scoring.generated.h"

UCLASS()
class EPICMEGAJAM2026_API UBPFL_Scoring : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	// ---- v2 (section 6)

	// 4.4: Profile[s] = min(SkillCap, sum of the tapes' Skills[s]). Empty slots simply aren't in the array.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static FSkillVector ComputeProfile(const TArray<FSkillVector>& TapeSkills, const UPDA_GameConfig* Config);

	// 6.1: sum over skills of |Profile - Target|
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static int32 ComputeMiss(const FSkillVector& Profile, const FSkillVector& Target);

	// 6.1: Miss / max(1, sum of Target)
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static float ComputeErrorRatio(const FSkillVector& Profile, const FSkillVector& Target);

	// 6.1: clamp(1 - ErrorRatio, 0, 1)
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static float ComputeAccuracy(float ErrorRatio);

	// 6.2: Loved / Liked / Neutral / Hated from the *MaxError thresholds
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static EReaction GetReaction(float ErrorRatio, const UPDA_GameConfig* Config);

	// 6.2: price paid on delivery = BaseRentalPrice x level multiplier
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static int32 ComputeRentalPrice(float PayMultiplier, const UPDA_GameConfig* Config);

	// 6.2: Loved/Liked tip = round(Accuracy x TipAtPerfect x level multiplier). 0 for other reactions.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static int32 ComputeTip(EReaction Reaction, float Accuracy, float PayMultiplier, const UPDA_GameConfig* Config);

	// 6.2: Hated fine = round(ComplaintFine x level multiplier). 0 for other reactions.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static int32 ComputeFine(EReaction Reaction, float PayMultiplier, const UPDA_GameConfig* Config);

	// 6.3: the skill with the largest |Profile - Target| (ties at random) and which way it missed.
	// False when the film is exact (nothing to give feedback on).
	UFUNCTION(BlueprintCallable, Category = "Scoring")
	static bool GetFeedbackSkill(const FSkillVector& Profile, const FSkillVector& Target, ESkill& OutSkill, EFeedbackDirection& OutDirection);

	// 6.3: a random DT_FeedbackLines row for that skill and direction
	UFUNCTION(BlueprintCallable, Category = "Scoring")
	static FText PickFeedbackLine(const UDataTable* FeedbackTable, ESkill Skill, EFeedbackDirection Direction);

};
