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
		const float Ratio = UBPFL_Scoring::ComputeErrorRatio(Profile, Target);
		const EReaction R = UBPFL_Scoring::GetReaction(Ratio, Cfg);
		TestEqual(N + TEXT(" miss"), UBPFL_Scoring::ComputeMiss(Profile, Target), ExpMiss);
		TestEqual(N + TEXT(" error ratio"), Ratio, ExpRatio, 0.001f);
		TestTrue(N + TEXT(" reaction"), R == ExpReaction);
		TestEqual(N + TEXT(" price"), UBPFL_Scoring::ComputeRentalPrice(Mult, Cfg), ExpPrice);
		TestEqual(N + TEXT(" tip"), UBPFL_Scoring::ComputeTip(R, UBPFL_Scoring::ComputeAccuracy(Ratio), Mult, Cfg), ExpTip);
		TestEqual(N + TEXT(" fine"), UBPFL_Scoring::ComputeFine(R, Mult, Cfg), ExpFine);
	};

	Run(TEXT("Case1"), 1.0f, SV(3), SV(3), 0, 0.f, EReaction::Loved, 10, 12, 0);
	Run(TEXT("Case2"), 1.0f, SV(3, 3), SV(3, 2), 1, 0.167f, EReaction::Liked, 10, 10, 0);
	Run(TEXT("Case3"), 1.0f, SV(3), SV(0, 3), 6, 2.f, EReaction::Hated, 10, 0, 15);
	Run(TEXT("Case4"), 1.5f, SV(5, 0, 2, 0, 1), SV(5, 0, 1, 0, 1), 1, 0.125f, EReaction::Liked, 15, 16, 0);
	Run(TEXT("Case5"), 1.0f, SV(3), SV(3, 0, 0, 3), 3, 1.f, EReaction::Hated, 10, 0, 15);

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
	return true;
}

#endif
