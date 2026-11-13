// Runs in a game process loading L_VideoStore: spawn the tutorial client, craft a film, deliver it, wait for the review.
// Launch: UnrealEditor-Cmd <uproject> /Game/DirectorsCut/Levels/L_VideoStore -game -nullrhi -ExecCmds="Automation RunTests DirectorsCut.Flow; Quit"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "GameFramework/WorldSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

class FDCFlowCommand : public IAutomationLatentCommand
{
public:
	explicit FDCFlowCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Start == 0.0) Start = Now;
		if (Now - Start > 60.0)
		{
			{ UWorld* W = nullptr; for (const FWorldContext& C : GEngine->GetWorldContexts()) { if (C.WorldType == EWorldType::Game) W = C.World(); } ADCGameMode* G = W ? W->GetAuthGameMode<ADCGameMode>() : nullptr; Test->AddError(FString::Printf(TEXT("Timed out in stage %d money=%d before=%d dilation=%f paused=%d worldtime=%f"), Stage, G ? G->Economy->Money : -1, MoneyBefore, W ? W->GetWorldSettings()->TimeDilation : -1.f, W ? (int32)W->IsPaused() : -1, W ? W->GetTimeSeconds() : -1.0)); }
			return true;
		}
		UWorld* World = nullptr;
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.WorldType == EWorldType::Game) World = C.World();
		}
		ADCGameMode* GM = World ? World->GetAuthGameMode<ADCGameMode>() : nullptr;
		if (!GM || !GM->Config) return false; // world not ready

		switch (Stage)
		{
		case 0: // spawn (first stop the automatic schedule and clear any client it already sent)
			GM->DayNightCycle->bRunning = false;
			GM->ClientDirector->CloseStore();
			Client = GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow, EClientCategory::Casual);
			if (!Client) { Test->AddError(TEXT("SpawnClient returned null (level set up? tables assigned?)")); return true; }
			Test->TestEqual(TEXT("tutorial preference count"), Client->Runtime.Preferences.Num(), 1);
			Test->TestTrue(TEXT("tutorial is Horror -2"), Client->Runtime.Preferences[0].Genre == EGenre::Horror && Client->Runtime.Preferences[0].Weight == -2);
			Stage = 1;
			return false;
		case 1: // wait at counter
			if (Client->Runtime.State != EClientState::AtCounter) return false;
			Test->TestTrue(TEXT("order text built"), !Client->Runtime.OrderText.IsEmpty());
			Test->TestTrue(TEXT("front client"), GM->ClientDirector->GetFrontClient() == Client);
			Test->TestFalse(TEXT("deliver without film rejected"), GM->ClientDirector->DeliverFilm(Client));
			Test->TestTrue(TEXT("slot intro"), GM->FilmCrafting->SetSlot(EFilmSlot::Intro, "VHS_Starter_02"));
			Test->TestFalse(TEXT("duplicate tape rejected"), GM->FilmCrafting->SetSlot(EFilmSlot::Middle, "VHS_Starter_02"));
			Test->TestTrue(TEXT("slot middle"), GM->FilmCrafting->SetSlot(EFilmSlot::Middle, "VHS_Starter_03"));
			Test->TestFalse(TEXT("cannot confirm with 2 slots"), GM->FilmCrafting->CanConfirm());
			Test->TestTrue(TEXT("slot ending"), GM->FilmCrafting->SetSlot(EFilmSlot::Ending, "VHS_Starter_04"));
			Test->TestTrue(TEXT("confirm"), GM->FilmCrafting->ConfirmFilm(FText::GetEmpty()));
			Test->TestEqual(TEXT("untitled name"), GM->FilmCrafting->HeldFilms[0].Title.ToString(), FString(TEXT("Untitled Cut #1")));
			MoneyBefore = GM->Economy->Money;
			Test->TestTrue(TEXT("deliver"), GM->ClientDirector->DeliverFilm(Client));
			Test->TestEqual(TEXT("base price paid"), GM->Economy->Money, MoneyBefore + GM->Config->BaseRentalPrice);
			Test->TestEqual(TEXT("film consumed from hand"), GM->FilmCrafting->HeldFilms.Num(), 0);
			Stage = 2;
			return false;
		case 2: // wait for the delayed review: Horror film vs Horror -2 -> Hated -> fine 15
			if (GM->Economy->Money == MoneyBefore + GM->Config->BaseRentalPrice) return false;
			Test->TestEqual(TEXT("money after review"), GM->Economy->Money, MoneyBefore + GM->Config->BaseRentalPrice - GM->Config->ComplaintFine);
			return true;
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
	double Start = 0.0;
	int32 Stage = 0;
	ADCClient* Client = nullptr;
	int32 MoneyBefore = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCClientFlowTest, "DirectorsCut.Flow.ClientDelivery",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCClientFlowTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCFlowCommand(this));
	return true;
}

#endif
