#include "Core/BPFL_Scoring.h"

float UBPFL_Scoring::AttributeScore(const FGenrePreference& Pref, const TMap<EGenre, float>& Profile, const UPDA_GameConfig* Config)
{
	const float* Found = Profile.Find(Pref.Genre);
	const float Value = Found ? *Found : 0.f;
	if (Pref.Weight > 0 || Value > 0.f)
	{
		return Pref.Weight * Value;
	}
	return Config->AvoidBonus; // dislikes a genre the film doesn't have
}

float UBPFL_Scoring::ComputeScore(const TArray<FGenrePreference>& Preferences, const FCreatedFilm& Film, const UPDA_GameConfig* Config)
{
	if (Preferences.IsEmpty())
	{
		return 0.f;
	}
	float Raw = 0.f;
	for (const FGenrePreference& Pref : Preferences)
	{
		Raw += AttributeScore(Pref, Film.GenreProfile, Config);
	}
	float Score = Raw / Preferences.Num();
	if (Raw > 0.f)
	{
		Score *= 1.f + (Film.AverageRating - 3.f) * Config->RatingStep;
	}
	return Score;
}

EReaction UBPFL_Scoring::GetReaction(float Score, const UPDA_GameConfig* Config)
{
	if (Score >= Config->LovedThreshold) return EReaction::Loved;
	if (Score >= Config->LikedThreshold) return EReaction::Liked;
	if (Score >= Config->NeutralThreshold) return EReaction::Neutral;
	return EReaction::Hated;
}

int32 UBPFL_Scoring::ComputeTip(EReaction Reaction, float Score, float PayMultiplier, const UPDA_GameConfig* Config)
{
	if (Reaction != EReaction::Loved && Reaction != EReaction::Liked)
	{
		return 0;
	}
	return FMath::RoundToInt(Score * Config->MoneyPerPoint * PayMultiplier);
}

int32 UBPFL_Scoring::ComputeFine(EReaction Reaction, float PayMultiplier, const UPDA_GameConfig* Config)
{
	return Reaction == EReaction::Hated ? FMath::RoundToInt(Config->ComplaintFine * PayMultiplier) : 0;
}
