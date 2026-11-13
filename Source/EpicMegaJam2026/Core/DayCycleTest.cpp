// Day/night cycle, rent at closing and supplier purchases, run in a game process loading L_VideoStore.
// One test per process (the day cycle test mutates shared state and defeat ends the game):
//   Automation RunTests DirectorsCut.DayCycle.CloseAndNextDay / DirectorsCut.DayCycle.RentDefeat / DirectorsCut.Supplier.Buy
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DCGameMode.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	ADCGameMode* FindGM()
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

	enum class EScenario { CloseAndNextDay, RentDefeat, SupplierBuy };

	class FDCCycleCommand : public IAutomationLatentCommand
	{
	public:
		FDCCycleCommand(FAutomationTestBase* InTest, EScenario InScenario) : Test(InTest), Scenario(InScenario) {}

		virtual bool Update() override
		{
			if (FPlatformTime::Seconds() - Start > 30.0)
			{
				Test->AddError(FString::Printf(TEXT("Timed out in stage %d"), Stage));
				return true;
			}
			ADCGameMode* GM = FindGM();
			if (!GM) return false;
			UAC_DayNightCycle* DN = GM->DayNightCycle;
			UAC_Economy* E = GM->Economy;

			if (Scenario == EScenario::SupplierBuy)
			{
				UAC_Inventory* Inv = GM->Inventory;
				GM->Config->MovieBudgetGoal = 1000000;
				Test->TestEqual(TEXT("starting tapes (3 rows, 5 copies)"), Inv->GetOwnedVHS().Num(), 3);
				UAC_Unlocks* U = GM->Unlocks;
				Test->TestEqual(TEXT("starting slots"), GM->FilmCrafting->UnlockedSlots, 3);
				Test->TestEqual(TEXT("starting star level"), U->UnlockedStarTier, 1);
				Test->TestEqual(TEXT("starting genres"), U->GetGenreCount(), 3);

				// Supplier: genre unlocked and stars within the license
				Test->TestTrue(TEXT("1 star Horror on sale"), Inv->IsTapeForSale("VHS_Horror_1_02"));
				Test->TestFalse(TEXT("Romance locked"), Inv->IsTapeForSale("VHS_Romance_1_01"));
				Test->TestFalse(TEXT("2 stars locked"), Inv->IsTapeForSale("VHS_Horror_2_01"));
				Test->TestEqual(TEXT("1 star price"), Inv->GetTapePrice("VHS_Horror_1_02"), 4);
				Test->TestTrue(TEXT("buy x5"), Inv->BuyTape("VHS_Horror_1_02", 5));
				Test->TestEqual(TEXT("copies after purchase"), Inv->GetCount("VHS_Horror_1_02"), 5);
				Test->TestEqual(TEXT("money after purchase"), E->Money, 30);
				Test->TestFalse(TEXT("cannot buy a locked genre"), Inv->BuyTape("VHS_Romance_1_01"));
				Test->TestFalse(TEXT("cannot afford x10"), Inv->BuyTape("VHS_Horror_1_02", 10));
				Test->TestEqual(TEXT("purchase is not earnings"), E->DayEarned, 0);

				// Computer: slots, star levels and genres, each bought once and in order
				E->AddMoney(2000);
				Test->TestFalse(TEXT("Slot_5 needs Slot_4"), U->CanBuyUnlock("Slot_5"));
				Test->TestTrue(TEXT("buy Slot_4"), U->BuyUnlock("Slot_4"));
				Test->TestEqual(TEXT("4 slots in the player"), GM->FilmCrafting->SlotRows.Num(), 4);
				Test->TestTrue(TEXT("buy Slot_5"), U->BuyUnlock("Slot_5"));
				Test->TestTrue(TEXT("fifth slot usable"), GM->FilmCrafting->SetSlot(4, "VHS_Horror_1_02"));
				GM->FilmCrafting->ClearSlot(4);
				Test->TestFalse(TEXT("cannot buy twice"), U->BuyUnlock("Slot_4"));
				Test->TestFalse(TEXT("Stars_3 needs Stars_2"), U->CanBuyUnlock("Stars_3"));
				Test->TestTrue(TEXT("buy Stars_2"), U->BuyUnlock("Stars_2"));
				Test->TestEqual(TEXT("star level 2"), U->UnlockedStarTier, 2);
				Test->TestTrue(TEXT("2 stars on sale now"), Inv->IsTapeForSale("VHS_Horror_2_01"));
				Test->TestTrue(TEXT("buy Genre_Romance"), U->BuyUnlock("Genre_Romance"));
				Test->TestTrue(TEXT("Romance on sale now"), Inv->IsTapeForSale("VHS_Romance_1_01"));
				Test->TestFalse(TEXT("premium genre needs 6 genres"), U->CanBuyUnlock("Genre_Experimental"));
				Test->TestTrue(TEXT("buy Genre_Drama"), U->BuyUnlock("Genre_Drama"));
				Test->TestTrue(TEXT("buy Genre_SciFi"), U->BuyUnlock("Genre_SciFi"));
				Test->TestEqual(TEXT("six genres"), U->GetGenreCount(), 6);
				Test->TestTrue(TEXT("premium genre now buyable"), U->BuyUnlock("Genre_Experimental"));
				return true;
			}

			switch (Stage)
			{
			case 0: // shorten the phases; the day is already running from BeginPlay
				GM->Config->DayDurationSec = 1.f;
				GM->Config->NightDurationSec = 1.f;
				GM->Config->MovieBudgetGoal = 1000000;
				Test->TestTrue(TEXT("day 1 running at start"), DN->bRunning && DN->DayIndex == 1);
				if (Scenario == EScenario::CloseAndNextDay)
				{
					E->AddMoney(200); // enough for the $60 rent
				}
				Stage = 1;
				return false;
			case 1: // night begins
				if (DN->CurrentPhase != EPhase::Night) return false;
				Stage = 2;
				return false;
			case 2: // store closes: rent charged (or game over)
				if (Scenario == EScenario::RentDefeat)
				{
					if (!E->bGameIsOver) return false;
					Test->TestEqual(TEXT("money kept on defeat"), E->Money, GM->Config->StartMoney);
					return true;
				}
				if (DN->bRunning) return false;
				Test->TestFalse(TEXT("no game over when rent is payable"), E->bGameIsOver);
				Test->TestEqual(TEXT("rent charged"), E->Money, GM->Config->StartMoney + 200 - 60);
				DN->StartNextDay();
				Test->TestEqual(TEXT("day 2"), DN->DayIndex, 2);
				Test->TestTrue(TEXT("day phase again"), DN->CurrentPhase == EPhase::Day && DN->bRunning);
				Test->TestEqual(TEXT("earnings reset"), E->DayEarned, 0);
				Test->TestEqual(TEXT("clock at day start"), DN->GetClockText().ToString(), FString(TEXT("10:00")));
				return true;
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		EScenario Scenario;
		double Start = FPlatformTime::Seconds();
		int32 Stage = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCCloseTest, "DirectorsCut.DayCycle.CloseAndNextDay",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDCCloseTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCCycleCommand(this, EScenario::CloseAndNextDay));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCRentDefeatTest, "DirectorsCut.DayCycle.RentDefeat",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDCRentDefeatTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCCycleCommand(this, EScenario::RentDefeat));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCSupplierTest, "DirectorsCut.Supplier.Buy",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FDCSupplierTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCCycleCommand(this, EScenario::SupplierBuy));
	return true;
}

#endif
