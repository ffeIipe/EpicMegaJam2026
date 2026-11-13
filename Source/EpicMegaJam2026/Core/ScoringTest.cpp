// Automation test: the three worked examples of SystemsDesigns.md section 6.3, with initial config values.
#include "Misc/AutomationTest.h"
#include "Core/BPFL_Scoring.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCScoringExamplesTest, "DirectorsCut.Scoring.Section6Examples",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCScoringExamplesTest::RunTest(const FString& Parameters)
{
	const UPDA_GameConfig* Cfg = GetDefault<UPDA_GameConfig>();
	auto Run = [&](TArray<FGenrePreference> Prefs, TMap<EGenre, float> Profile, float Rating, float Mult,
		float ExpScore, EReaction ExpReaction, int32 ExpTip, int32 ExpFine, const TCHAR* Name)
	{
		FCreatedFilm Film;
		Film.GenreProfile = Profile;
		Film.AverageRating = Rating;
		const float Score = UBPFL_Scoring::ComputeScore(Prefs, Film, Cfg);
		const EReaction R = UBPFL_Scoring::GetReaction(Score, Cfg);
		TestEqual(FString(Name) + TEXT(" score"), Score, ExpScore, 0.001f);
		TestTrue(FString(Name) + TEXT(" reaction"), R == ExpReaction);
		TestEqual(FString(Name) + TEXT(" tip"), UBPFL_Scoring::ComputeTip(R, Score, Mult, Cfg), ExpTip);
		TestEqual(FString(Name) + TEXT(" fine"), UBPFL_Scoring::ComputeFine(R, Mult, Cfg), ExpFine);
	};

	// Fear client (Horror -2), film with a Horror 3 tape: Raw -6 -> Hated, fine 15
	Run({{EGenre::Horror, -2}}, {{EGenre::Horror, 3.f}}, 3.f, 1.f, -6.f, EReaction::Hated, 0, 15, TEXT("Example1"));
	// Same client, no Horror: Raw +3 -> Liked, tip 6 (rating 3 -> factor 1)
	Run({{EGenre::Horror, -2}}, {{EGenre::Comedy, 3.f}}, 3.f, 1.f, 3.f, EReaction::Liked, 6, 0, TEXT("Example2"));
	// Funny client (Comedy +2), Comedy 5 + Romance 3, rating 4: Raw 10, Score 11 -> Loved, tip 22
	Run({{EGenre::Comedy, 2}}, {{EGenre::Comedy, 5.f}, {EGenre::Romance, 3.f}}, 4.f, 1.f, 11.f, EReaction::Loved, 22, 0, TEXT("Example3"));
	return true;
}

#endif
