// Automation test: the six cases of SystemsDesigns.md section 6.5, with initial config values, plus the 6.3 feedback pick.
#include "Misc/AutomationTest.h"
#include "Core/BPFL_Scoring.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCScoringExamplesTest, "DirectorsCut.Scoring.Section6Examples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

static FSkillVector SV(int32 Funny, int32 Scary = 0, int32 Brave = 0, int32 Thoughtful = 0, int32 Romantic = 0)
{
	FSkillVector V;
	V.Funny = Funny; V.Scary = Scary; V.Brave = Brave; V.Thoughtful = Thoughtful; V.Romantic = Romantic;
	return V;
}

bool FDCScoringExamplesTest::RunTest(const FString& Parameters)
{
	const UPDA_GameConfig* Cfg = GetDefault<UPDA_GameConfig>();
	auto Run = [&](const TCHAR* Name, float Mult, FSkillVector Target, FSkillVector Profile,
		int32 ExpMiss, float ExpRatio, EReaction ExpReaction, int32 ExpPrice, int32 ExpTip, int32 ExpFine)
	{
		const FString N(Name);
		const float Ratio = UBPFL_Scoring::ComputeErrorRatio(Profile, Target, Cfg);
		const EReaction R = UBPFL_Scoring::GetReaction(Ratio, Cfg);
		TestEqual(N + TEXT(" miss"), UBPFL_Scoring::ComputeMiss(Profile, Target), ExpMiss);
		TestEqual(N + TEXT(" error ratio"), Ratio, ExpRatio, 0.001f);
		TestTrue(N + TEXT(" reaction"), R == ExpReaction);
		TestEqual(N + TEXT(" price"), UBPFL_Scoring::ComputeRentalPrice(Mult, Target, Cfg), ExpPrice);
		TestEqual(N + TEXT(" tip"), UBPFL_Scoring::ComputeTip(R, UBPFL_Scoring::ComputeAccuracy(Ratio), Mult, Cfg), ExpTip);
		TestEqual(N + TEXT(" fine"), UBPFL_Scoring::ComputeFine(R, Mult, Cfg), ExpFine);
	};

	Run(TEXT("Case1"), 1.0f, SV(3), SV(3), 0, 0.f, EReaction::Loved, 10, 12, 0);
	Run(TEXT("Case2"), 1.0f, SV(3, 3), SV(3, 2), 1, 0.167f, EReaction::Liked, 13, 10, 0);
	Run(TEXT("Case3"), 1.0f, SV(3), SV(0, 3), 6, 1.5f, EReaction::Hated, 10, 0, 15);
	Run(TEXT("Case4"), 1.5f, SV(5, 0, 2, 0, 1), SV(5, 0, 1, 0, 1), 1, 0.125f, EReaction::Liked, 23, 16, 0);
	Run(TEXT("Case5"), 1.0f, SV(3), SV(3, 0, 0, 3), 3, 0.75f, EReaction::Hated, 10, 0, 15);

	// Case 6: four Funny 3 tapes cap at SkillCap
	TestEqual(TEXT("Case6 profile cap"), UBPFL_Scoring::ComputeProfile({ SV(3), SV(3), SV(3), SV(3) }, Cfg).Funny, 10);

	// 6.3: case 5 only misses Thoughtful, and by excess
	ESkill Skill = ESkill::Funny;
	EFeedbackDirection Dir = EFeedbackDirection::TooLittle;
	TestTrue(TEXT("Feedback found"), UBPFL_Scoring::GetFeedbackSkill(SV(3, 0, 0, 3), SV(3), Skill, Dir));
	TestTrue(TEXT("Feedback skill"), Skill == ESkill::Thoughtful);
	TestTrue(TEXT("Feedback direction"), Dir == EFeedbackDirection::TooMuch);
	// Case 4 misses Brave by shortage
	TestTrue(TEXT("Feedback found 2"), UBPFL_Scoring::GetFeedbackSkill(SV(5, 0, 1, 0, 1), SV(5, 0, 2, 0, 1), Skill, Dir));
	TestTrue(TEXT("Feedback skill 2"), Skill == ESkill::Brave);
	TestTrue(TEXT("Feedback direction 2"), Dir == EFeedbackDirection::TooLittle);
	TestFalse(TEXT("No feedback on an exact film"), UBPFL_Scoring::GetFeedbackSkill(SV(3), SV(3), Skill, Dir));
	// Error floor: one stray point on a Funny 3 order is 1/4, not 1/3
	TestEqual(TEXT("Floor"), UBPFL_Scoring::ComputeErrorRatio(SV(3, 1), SV(3), Cfg), 0.25f, 0.001f);

	// Match meter: only the hints count, never the Target
	auto Known = [&](FSkillVector Profile, TArray<FHintedSkill> Hints, bool bFull, int32 TargetTotal)
	{
		return UBPFL_Scoring::ComputeKnownErrorRatio(Profile, Hints, bFull, TargetTotal, Cfg);
	};
	FHintedSkill FunnyHigh, ScaryNone, FunnySome;
	FunnyHigh.Skill = ESkill::Funny; FunnyHigh.Level = EHintLevel::High;
	ScaryNone.Skill = ESkill::Scary; ScaryNone.Level = EHintLevel::None;
	FunnySome.Skill = ESkill::Funny; FunnySome.Level = EHintLevel::Some;
	TestEqual(TEXT("Known exact"), Known(SV(3), { FunnyHigh, ScaryNone }, true, 3), 0.f, 0.001f);
	TestEqual(TEXT("Known scary violated"), Known(SV(3, 1), { FunnyHigh, ScaryNone }, true, 3), 0.25f, 0.001f);
	TestEqual(TEXT("Known High short"), Known(SV(2), { FunnyHigh }, true, 3), 0.25f, 0.001f);
	TestEqual(TEXT("Known High above the minimum is fine"), Known(SV(6), { FunnyHigh }, true, 6), 0.f, 0.001f);
	TestEqual(TEXT("Known Some range"), Known(SV(2), { FunnySome }, true, 2), 0.f, 0.001f);
	TestEqual(TEXT("Known Some over"), Known(SV(3), { FunnySome }, true, 2), 0.25f, 0.001f);
	TestEqual(TEXT("Unhinted ignored when not fully hinted"), Known(SV(3, 0, 0, 3), { FunnyHigh }, false, 6), 0.f, 0.001f);
	TestEqual(TEXT("Unhinted is 0 when fully hinted"), Known(SV(3, 0, 0, 3), { FunnyHigh }, true, 3), 0.75f, 0.001f);
	// The bug this fixes: a Funny 6 order with "Funny High" must not read GREAT with one Funny 3 tape
	TestEqual(TEXT("One tape falls short of a 6 point order"), Known(SV(3), { FunnyHigh, ScaryNone }, true, 6), 0.5f, 0.001f);
	TestTrue(TEXT("...and that reads Neutral (Okay), not Loved"), UBPFL_Scoring::GetReaction(0.5f, Cfg) == EReaction::Neutral);
	TestEqual(TEXT("Two tapes reach it"), Known(SV(6), { FunnyHigh, ScaryNone }, true, 6), 0.f, 0.001f);
	return true;
}

#endif
