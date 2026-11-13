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
	// 6.1: positive -> Weight x Profile[g]; negative -> Weight x Profile[g] if the film has that genre, else AvoidBonus.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static float AttributeScore(const FGenrePreference& Pref, const TMap<EGenre, float>& Profile, const UPDA_GameConfig* Config);

	// 6.2: Raw / NumAttributes, times RatingFactor only when Raw > 0.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static float ComputeScore(const TArray<FGenrePreference>& Preferences, const FCreatedFilm& Film, const UPDA_GameConfig* Config);

	// 6.3
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static EReaction GetReaction(float Score, const UPDA_GameConfig* Config);

	// Tip for Loved/Liked: round(Score x MoneyPerPoint x PayMultiplier). 0 for other reactions.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static int32 ComputeTip(EReaction Reaction, float Score, float PayMultiplier, const UPDA_GameConfig* Config);

	// Fine for Hated: round(ComplaintFine x PayMultiplier). 0 for other reactions.
	UFUNCTION(BlueprintPure, Category = "Scoring")
	static int32 ComputeFine(EReaction Reaction, float PayMultiplier, const UPDA_GameConfig* Config);
};
