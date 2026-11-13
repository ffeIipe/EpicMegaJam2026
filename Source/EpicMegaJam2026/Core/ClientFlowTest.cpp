// Runs in a game process loading L_VideoStore: the three scripted orders (section 5.4) are served with the starting
// inventory and every one is Loved, then the next client gets a generated order (5.2).
// Launch: UnrealEditor-Cmd <uproject> /Game/DirectorsCut/Levels/L_VideoStore -game -nullrhi -ExecCmds="Automation RunTests DirectorsCut.Flow; Quit"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DCGameMode.h"
#include "Core/DCClient.h"
#include "Core/BPFL_Scoring.h"
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
		if (Now - Start > 90.0)
		{
			Test->AddError(FString::Printf(TEXT("Timed out in stage %d (order %d)"), Stage, Order));
			return true;
		}
		UWorld* World = nullptr;
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.WorldType == EWorldType::Game) World = C.World();
		}
		ADCGameMode* GM = World ? World->GetAuthGameMode<ADCGameMode>() : nullptr;
		if (!GM || !GM->Config) return false; // world not ready

		// The three scripted orders and the starting tapes that satisfy each one
		struct FScript { FSkillVector Target; TArray<FName> Tapes; };
		auto SV = [](int32 Funny, int32 Scary) { FSkillVector V; V.Funny = Funny; V.Scary = Scary; return V; };
		static const TArray<FScript> Scripts = {
			{ SV(3, 0), { "VHS_Comedy_1_01" } },
			{ SV(3, 3), { "VHS_Comedy_1_01", "VHS_Horror_1_01" } },
			{ SV(0, 3), { "VHS_Horror_1_01" } } };

		switch (Stage)
		{
		case 0: // spawn the next client (first stop the automatic schedule and clear any client it already sent)
			if (Order == 0)
			{
				GM->DayNightCycle->bRunning = false;
				GM->ClientDirector->CloseStore();
				GM->ClientDirector->ResetScriptedOrders(); // the schedule may already have sent client 1
				GM->Config->ReviewDelaySec = 0.5f;
			}
			Client = GM->ClientDirector->SpawnClient(GM->ClientDirector->TutorialArchetypeRow);
			if (!Client) { Test->AddError(TEXT("SpawnClient returned null (level set up? tables assigned?)")); return true; }
			Stage = Order < Scripts.Num() ? 1 : 3;
			return false;
		case 1: // scripted client: wait at the counter, check the order, craft the answer, deliver
		{
			if (Client->Runtime.State != EClientState::AtCounter) return false;
			const FScript& Script = Scripts[Order];
			const FString N = FString::Printf(TEXT("Order%d "), Order + 1);
			const FSkillVector& T = Client->Runtime.Target;
			Test->TestEqual(N + TEXT("is level 1"), Client->Runtime.Tier, 1);
			Test->TestEqual(N + TEXT("Funny target"), T.Funny, Script.Target.Funny);
			Test->TestEqual(N + TEXT("Scary target"), T.Scary, Script.Target.Scary);
			Test->TestEqual(N + TEXT("rest of the target is 0"), T.Brave + T.Thoughtful + T.Romantic, 0);
			Test->TestEqual(N + TEXT("two hints"), Client->Runtime.HintedSkills.Num(), 2);
			Test->TestTrue(N + TEXT("order text built"), !Client->Runtime.OrderText.IsEmpty());
			Test->TestTrue(N + TEXT("front client"), GM->ClientDirector->GetFrontClient() == Client);
			Test->TestFalse(N + TEXT("deliver without film rejected"), GM->ClientDirector->DeliverFilm(Client));
			for (int32 i = 0; i < Script.Tapes.Num(); ++i)
			{
				Test->TestTrue(N + TEXT("slot"), GM->FilmCrafting->SetSlot(i, Script.Tapes[i]));
			}
			Test->TestTrue(N + TEXT("confirm"), GM->FilmCrafting->ConfirmFilm(FText::GetEmpty()));
			MoneyBefore = GM->Economy->Money;
			Test->TestTrue(N + TEXT("deliver"), GM->ClientDirector->DeliverFilm(Client));
			Test->TestEqual(N + TEXT("base price paid"), GM->Economy->Money, MoneyBefore + GM->Config->BaseRentalPrice);
			Test->TestEqual(N + TEXT("film out of the hand"), GM->FilmCrafting->HeldFilms.Num(), 0);
			// Exact match: Loved, tip = TipAtPerfect x persona TipMult, arriving with the review
			ExpectedTip = UBPFL_Scoring::ComputeTip(EReaction::Loved, 1.f, Client->Runtime.TipMult, GM->Config);
			Stage = 2;
			return false;
		}
		case 2: // wait for the delayed review to pay the tip
			if (ExpectedTip > 0 && GM->Economy->Money == MoneyBefore + GM->Config->BaseRentalPrice) return false;
			Test->TestEqual(TEXT("money after review"), GM->Economy->Money, MoneyBefore + GM->Config->BaseRentalPrice + ExpectedTip);
			++Order;
			Stage = 0;
			return false;
		case 3: // fourth client: generated order (5.2), level 1 with the starting licence
		{
			const FClientRuntime& Rt = Client->Runtime;
			const int32 Total = Rt.Target.Funny + Rt.Target.Scary + Rt.Target.Brave + Rt.Target.Thoughtful + Rt.Target.Romantic;
			Test->TestEqual(TEXT("generated client is level 1"), Rt.Tier, 1);
			Test->TestTrue(TEXT("recipe of 1 to 2 one-star tapes: 3 to 6 points"), Total >= 3 && Total <= 6);
			Test->TestEqual(TEXT("level 1 hints 2 skills"), Rt.HintedSkills.Num(), 2);
			return true;
		}
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
	double Start = 0.0;
	int32 Stage = 0;
	int32 Order = 0;
	ADCClient* Client = nullptr;
	int32 MoneyBefore = 0;
	int32 ExpectedTip = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCClientFlowTest, "DirectorsCut.Flow.ClientDelivery",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCClientFlowTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCFlowCommand(this));
	return true;
}

#endif
