// Rent, victory and defeat rules (SystemsDesigns.md section 7) with initial config values.
// Launch like the Flow test, one test per process (defeat and victory are exclusive end states):
//   Automation RunTests DirectorsCut.Economy.RentAndDefeat   /   DirectorsCut.Economy.Victory
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DCGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	ADCGameMode* FindGameModeWithConfig()
	{
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.WorldType == EWorldType::Game && C.World())
			{
				ADCGameMode* GM = C.World()->GetAuthGameMode<ADCGameMode>();
				return GM && GM->Config ? GM : nullptr;
			}
		}
		return nullptr;
	}
}

class FDCEconomyCommand : public IAutomationLatentCommand
{
public:
	FDCEconomyCommand(FAutomationTestBase* InTest, bool bInVictory) : Test(InTest), bVictory(bInVictory) {}

	virtual bool Update() override
	{
		ADCGameMode* GM = FindGameModeWithConfig();
		if (!GM)
		{
			if (FPlatformTime::Seconds() - Start > 30.0)
			{
				Test->AddError(TEXT("Game world with config never became ready"));
				return true;
			}
			return false;
		}
		UAC_Economy* E = GM->Economy;
		Test->TestEqual(TEXT("start money"), E->Money, GM->Config->StartMoney);
		if (bVictory)
		{
			E->AddMoney(GM->Config->MovieBudgetGoal - E->Money - 1);
			Test->TestFalse(TEXT("no victory below goal"), E->bWon);
			E->AddMoney(1);
			Test->TestTrue(TEXT("victory at goal"), E->bWon);
			return true;
		}
		Test->TestEqual(TEXT("rent day 1"), E->GetRentForDay(1), 60);
		Test->TestEqual(TEXT("rent day 5"), E->GetRentForDay(5), 200);
		Test->TestEqual(TEXT("rent day 6"), E->GetRentForDay(6), 240);
		Test->TestEqual(TEXT("rent day 7"), E->GetRentForDay(7), 280);
		// 50 < 60: cannot pay -> game over, money untouched
		Test->TestFalse(TEXT("rent unpayable"), E->ChargeRent(1));
		Test->TestTrue(TEXT("game over flag"), E->bGameIsOver);
		Test->TestEqual(TEXT("money kept on defeat"), E->Money, GM->Config->StartMoney);
		return true;
	}

private:
	FAutomationTestBase* Test;
	bool bVictory;
	double Start = FPlatformTime::Seconds();
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCDefeatTest, "DirectorsCut.Economy.RentAndDefeat",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDCDefeatTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCEconomyCommand(this, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCVictoryTest, "DirectorsCut.Economy.Victory",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDCVictoryTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCEconomyCommand(this, true));
	return true;
}

#endif
