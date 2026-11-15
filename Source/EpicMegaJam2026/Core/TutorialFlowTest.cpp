// Interactive tutorial flow & skip verification on L_VideoStore.
// Launch: UnrealEditor-Cmd <uproject> /Game/DirectorsCut/Levels/L_VideoStore -game -nullrhi -unattended -nosplash -ExecCmds="Automation RunTests DirectorsCut.Tutorial; Quit"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/DCPlayerController.h"
#include "Data/PDA_GameConfig.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UWorld* FindGameWorld()
	{
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.WorldType == EWorldType::Game)
			{
				return C.World();
			}
		}
		return nullptr;
	}
}

class FDCTutorialFlowCommand : public IAutomationLatentCommand
{
public:
	explicit FDCTutorialFlowCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Start == 0.0) Start = Now;
		if (Now - Start > 60.0)
		{
			Test->AddError(FString::Printf(TEXT("Timed out in stage %d (tutorial step %d)"), Stage, PC ? (int32)PC->GetTutorialStep() : -1));
			return true;
		}
		UWorld* World = FindGameWorld();
		ADCGameMode* GM = World ? World->GetAuthGameMode<ADCGameMode>() : nullptr;
		PC = World ? Cast<ADCPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!GM || !GM->Config || !PC || !PC->GetPawn())
		{
			return false;
		}

		switch (Stage)
		{
		case 0:
			if (!PC->CanGoToBackRoom()) return false;
			if (!GM->Config->bShowTutorial)
			{
				GM->Config->bShowTutorial = true; // guard in case config was toggled
			}
			Test->TestEqual(TEXT("starts waiting for first client"), PC->GetTutorialStep(), ETutorialStep::WaitForClient);
			Test->TestFalse(TEXT("clock not paused before first client"), GM->DayNightCycle->bTutorialPaused);
			GM->ClientDirector->CloseStore();
			GM->ClientDirector->ResetScriptedOrders();
			Client = GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow);
			if (!Client)
			{
				Test->AddError(TEXT("SpawnClient returned null"));
				return true;
			}
			Stage = 1;
			return false;

		case 1:
			if (!Client->IsAtFront()) return false;
			Test->TestEqual(TEXT("step 1 approach client"), PC->GetTutorialStep(), ETutorialStep::ApproachClient);
			Test->TestTrue(TEXT("day clock paused during tutorial"), GM->DayNightCycle->bTutorialPaused);
			Test->TestTrue(TEXT("client director paused during tutorial"), GM->ClientDirector->bTutorialPaused);
			ElapsedAtPause = GM->DayNightCycle->PhaseElapsed;

			PC->OpenDialogue(Client);
			Test->TestEqual(TEXT("step 1 dialogue open"), PC->GetTutorialStep(), ETutorialStep::DialogueOrder);
			PC->OnDialogueButton();
			Test->TestTrue(TEXT("order acknowledged"), Client->bOrderAcknowledged);
			Test->TestEqual(TEXT("step 2 go to back room"), PC->GetTutorialStep(), ETutorialStep::GoToBackRoom);
			Stage = 2;
			return false;

		case 2:
			if (!PC->CanGoToBackRoom()) return false;
			PC->GoToBackRoom();
			Stage = 3;
			return false;

		case 3:
			if (PC->GetCameraZone() != ECameraZone::BackRoom) return false;
			Test->TestEqual(TEXT("step 3 combine pick"), PC->GetTutorialStep(), ETutorialStep::CombinePick);
			Test->TestTrue(TEXT("phase elapsed stayed frozen"), FMath::IsNearlyEqual(GM->DayNightCycle->PhaseElapsed, ElapsedAtPause, 0.001f));
			Test->TestTrue(TEXT("client patience stayed full"), FMath::IsNearlyEqual(Client->Runtime.PatienceLeft, Client->PatienceMax, 0.001f));

			Test->TestTrue(TEXT("slot 0 comedy"), GM->FilmCrafting->SetSlot(0, "VHS_Comedy_1_01"));
			Test->TestEqual(TEXT("still in combine step after 1 tape"), PC->GetTutorialStep(), ETutorialStep::CombinePick);
			Test->TestTrue(TEXT("slot 1 comedy repeat"), GM->FilmCrafting->SetSlot(1, "VHS_Comedy_1_01"));
			Test->TestTrue(TEXT("confirm first film"), GM->FilmCrafting->ConfirmFilm(FText::GetEmpty()));
			Test->TestEqual(TEXT("step 4 deliver film"), PC->GetTutorialStep(), ETutorialStep::DeliverFilm);
			Stage = 4;
			return false;

		case 4:
			if (PC->GetCameraZone() != ECameraZone::Counter) return false;
			if (!bTestedDiscard)
			{
				// Re-enter back room, discard the held film -> must revert DeliverFilm -> CombinePick -> Esc -> GoToBackRoom -> recombine
				bTestedDiscard = true;
				PC->GoToBackRoom();
				Stage = 41;
				return false;
			}
			PC->OpenDialogue(Client);
			Test->TestEqual(TEXT("still in deliver step on dialogue"), PC->GetTutorialStep(), ETutorialStep::DeliverFilm);
			PC->OnDialogueButton();
			Test->TestEqual(TEXT("step 4 waiting for review"), PC->GetTutorialStep(), ETutorialStep::WaitReview);
			Stage = 5;
			return false;

		case 41:
			if (PC->GetCameraZone() != ECameraZone::BackRoom) return false;
			GM->FilmCrafting->DiscardHeldFilm();
			Test->TestEqual(TEXT("discard in back room reverts to combine pick"), PC->GetTutorialStep(), ETutorialStep::CombinePick);
			PC->CloseScreen();
			Stage = 42;
			return false;

		case 42:
			if (PC->GetCameraZone() != ECameraZone::Counter) return false;
			Test->TestEqual(TEXT("leaving back room empty returns to step 2"), PC->GetTutorialStep(), ETutorialStep::GoToBackRoom);
			PC->GoToBackRoom();
			Stage = 3;
			return false;

		case 5:
			if (GM->ClientDirector->ReviewHistory.Num() == 0) return false;
			Test->TestEqual(TEXT("step 5 buy genre on computer"), PC->GetTutorialStep(), ETutorialStep::ComputerBuyGenre);
			Test->TestTrue(TEXT("open computer screen"), PC->OpenInteractableScreen(EInteractType::Computer));
			Test->TestTrue(TEXT("buy Romance genre"), GM->Unlocks->BuyUnlock("Genre_Romance"));
			Test->TestEqual(TEXT("step 6 visit supplier"), PC->GetTutorialStep(), ETutorialStep::SupplierBuyTape);
			PC->CloseScreen();
			Stage = 6;
			return false;

		case 6:
			if (!PC->CanGoToBackRoom()) return false; // waits until Computer finishes deactivating
			Test->TestTrue(TEXT("open supplier screen"), PC->OpenInteractableScreen(EInteractType::CashRegister));
			Test->TestTrue(TEXT("buy Romance VHS from supplier"), GM->Inventory->BuyTape("VHS_Romance_1_01", 1));
			Test->TestEqual(TEXT("tutorial complete"), PC->GetTutorialStep(), ETutorialStep::Complete);
			Test->TestFalse(TEXT("day clock resumed"), GM->DayNightCycle->bTutorialPaused);
			Test->TestFalse(TEXT("client director resumed"), GM->ClientDirector->bTutorialPaused);
			Test->TestEqual(TEXT("romance tape stocked"), GM->Inventory->GetCount("VHS_Romance_1_01"), 1);
			PC->CloseScreen();
			return true;
		}
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	ADCPlayerController* PC = nullptr;
	ADCClient* Client = nullptr;
	double Start = 0.0;
	int32 Stage = 0;
	float ElapsedAtPause = 0.f;
	bool bTestedDiscard = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCTutorialFlowTest, "DirectorsCut.Tutorial.FullFlow",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCTutorialFlowTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCTutorialFlowCommand(this));
	return true;
}

class FDCTutorialSkipCommand : public IAutomationLatentCommand
{
public:
	explicit FDCTutorialSkipCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Start == 0.0) Start = Now;
		if (Now - Start > 30.0)
		{
			Test->AddError(TEXT("Skip test timed out"));
			return true;
		}
		UWorld* World = FindGameWorld();
		ADCGameMode* GM = World ? World->GetAuthGameMode<ADCGameMode>() : nullptr;
		ADCPlayerController* PC = World ? Cast<ADCPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!GM || !GM->Config || !PC || !PC->GetPawn())
		{
			return false;
		}

		if (Stage == 0)
		{
			if (!PC->CanGoToBackRoom()) return false;
			GM->ClientDirector->CloseStore();
			GM->ClientDirector->ResetScriptedOrders();
			Client = GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow);
			if (!Client)
			{
				Test->AddError(TEXT("SpawnClient returned null"));
				return true;
			}
			Stage = 1;
			return false;
		}

		if (!Client->IsAtFront()) return false;
		Test->TestTrue(TEXT("tutorial active and paused"), PC->IsTutorialActive() && GM->DayNightCycle->bTutorialPaused);
		PC->SkipTutorial();
		Test->TestFalse(TEXT("skip stops tutorial"), PC->IsTutorialActive());
		Test->TestEqual(TEXT("skip marks step complete"), PC->GetTutorialStep(), ETutorialStep::Complete);
		Test->TestFalse(TEXT("skip unfreezes day clock"), GM->DayNightCycle->bTutorialPaused);
		Test->TestFalse(TEXT("skip unfreezes client director"), GM->ClientDirector->bTutorialPaused);
		return true;
	}

private:
	FAutomationTestBase* Test = nullptr;
	ADCClient* Client = nullptr;
	double Start = 0.0;
	int32 Stage = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCTutorialSkipTest, "DirectorsCut.Tutorial.SkipUnfreezes",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCTutorialSkipTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCTutorialSkipCommand(this));
	return true;
}

#endif
