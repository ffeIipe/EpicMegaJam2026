#include "Core/BPFL_Scoring.h"

// ---------------------------------------------------------------- v2 (section 6)

FSkillVector UBPFL_Scoring::ComputeProfile(const TArray<FSkillVector>& TapeSkills, const UPDA_GameConfig* Config)
{
	FSkillVector Profile;
	for (int32 i = 0; i < DCNumSkills; ++i)
	{
		const ESkill Skill = (ESkill)i;
		int32 Sum = 0;
		for (const FSkillVector& Tape : TapeSkills)
		{
			Sum += Tape.Get(Skill);
		}
		Profile.Set(Skill, FMath::Min(Sum, Config->SkillCap));
	}
	return Profile;
}

int32 UBPFL_Scoring::ComputeMiss(const FSkillVector& Profile, const FSkillVector& Target)
{
	int32 Miss = 0;
	for (int32 i = 0; i < DCNumSkills; ++i)
	{
		Miss += FMath::Abs(Profile.Get((ESkill)i) - Target.Get((ESkill)i));
	}
	return Miss;
}

float UBPFL_Scoring::ComputeErrorRatio(const FSkillVector& Profile, const FSkillVector& Target, const UPDA_GameConfig* Config)
{
	int32 TargetTotal = 0;
	for (int32 i = 0; i < DCNumSkills; ++i)
	{
		TargetTotal += Target.Get((ESkill)i);
	}
	return (float)ComputeMiss(Profile, Target) / FMath::Max(FMath::Max(1, Config->ErrorFloor), TargetTotal);
}

float UBPFL_Scoring::ComputeKnownErrorRatio(const FSkillVector& Profile, const TArray<FHintedSkill>& Hints, bool bFullyHinted, const UPDA_GameConfig* Config)
{
	int32 Miss = 0, Expected = 0;
	bool bHinted[DCNumSkills] = {};
	for (const FHintedSkill& H : Hints)
	{
		const int32 Lo = H.Level == EHintLevel::High ? Config->HintHighMin : H.Level == EHintLevel::Some ? 1 : 0;
		const int32 Hi = H.Level == EHintLevel::High ? Config->SkillCap : H.Level == EHintLevel::Some ? FMath::Max(1, Config->HintHighMin - 1) : 0;
		const int32 V = Profile.Get(H.Skill);
		Miss += V < Lo ? Lo - V : V > Hi ? V - Hi : 0;
		Expected += Lo;
		bHinted[(int32)H.Skill] = true;
	}
	if (bFullyHinted)
	{
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			Miss += bHinted[i] ? 0 : Profile.Get((ESkill)i);
		}
	}
	return (float)Miss / FMath::Max(FMath::Max(1, Config->ErrorFloor), Expected);
}

float UBPFL_Scoring::ComputeAccuracy(float ErrorRatio)
{
	return FMath::Clamp(1.f - ErrorRatio, 0.f, 1.f);
}

EReaction UBPFL_Scoring::GetReaction(float ErrorRatio, const UPDA_GameConfig* Config)
{
	if (ErrorRatio <= Config->LovedMaxError) return EReaction::Loved;
	if (ErrorRatio <= Config->LikedMaxError) return EReaction::Liked;
	if (ErrorRatio <= Config->NeutralMaxError) return EReaction::Neutral;
	return EReaction::Hated;
}

int32 UBPFL_Scoring::ComputeRentalPrice(float PayMultiplier, const UPDA_GameConfig* Config)
{
	return FMath::RoundToInt(Config->BaseRentalPrice * PayMultiplier);
}

int32 UBPFL_Scoring::ComputeTip(EReaction Reaction, float Accuracy, float PayMultiplier, const UPDA_GameConfig* Config)
{
	if (Reaction != EReaction::Loved && Reaction != EReaction::Liked)
	{
		return 0;
	}
	return FMath::RoundToInt(Accuracy * Config->TipAtPerfect * PayMultiplier);
}

int32 UBPFL_Scoring::ComputeFine(EReaction Reaction, float PayMultiplier, const UPDA_GameConfig* Config)
{
	return Reaction == EReaction::Hated ? FMath::RoundToInt(Config->ComplaintFine * PayMultiplier) : 0;
}

bool UBPFL_Scoring::GetFeedbackSkill(const FSkillVector& Profile, const FSkillVector& Target, ESkill& OutSkill, EFeedbackDirection& OutDirection)
{
	int32 Worst = 0;
	TArray<ESkill, TInlineAllocator<DCNumSkills>> Ties;
	for (int32 i = 0; i < DCNumSkills; ++i)
	{
		const int32 Diff = FMath::Abs(Profile.Get((ESkill)i) - Target.Get((ESkill)i));
		if (Diff > Worst)
		{
			Worst = Diff;
			Ties.Reset();
		}
		if (Diff == Worst && Diff > 0)
		{
			Ties.Add((ESkill)i);
		}
	}
	if (Ties.IsEmpty())
	{
		return false;
	}
	OutSkill = Ties[FMath::RandRange(0, Ties.Num() - 1)];
	OutDirection = Profile.Get(OutSkill) < Target.Get(OutSkill) ? EFeedbackDirection::TooLittle : EFeedbackDirection::TooMuch;
	return true;
}

FText UBPFL_Scoring::PickFeedbackLine(const UDataTable* FeedbackTable, ESkill Skill, EFeedbackDirection Direction)
{
	TArray<const FFeedbackLineData*> Candidates;
	if (FeedbackTable)
	{
		for (const TPair<FName, uint8*>& P : FeedbackTable->GetRowMap())
		{
			const FFeedbackLineData* Row = reinterpret_cast<const FFeedbackLineData*>(P.Value);
			if (Row->Skill == Skill && Row->Direction == Direction)
			{
				Candidates.Add(Row);
			}
		}
	}
	// TODO(DESIGN): feedback lines pending from the team
	return Candidates.Num() ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)]->Text : FText::FromString(TEXT("TODO(DESIGN): feedback line"));
}
