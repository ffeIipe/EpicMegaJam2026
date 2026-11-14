// Camera zone cycle (SystemsDesigns.md section 9): Counter -> ToBackRoom -> BackRoom (crafting panel up) -> combine -> ToCounter -> Counter.
// Launch: UnrealEditor-Cmd <uproject> /Game/DirectorsCut/Levels/L_VideoStore -game -nullrhi -unattended -ExecCmds="Automation RunTests DirectorsCut.Camera; Quit"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DCGameMode.h"
#include "Core/DCPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

class FDCCameraZoneCommand : public IAutomationLatentCommand
{
public:
	explicit FDCCameraZoneCommand(FAutomationTestBase* InTest) : Test(InTest) {}

	virtual bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (Start == 0.0) Start = Now;
		if (Now - Start > 30.0)
		{
			Test->AddError(FString::Printf(TEXT("Timed out in stage %d (zone %d)"), Stage, PC ? (int32)PC->GetCameraZone() : -1));
			return true;
		}
		UWorld* World = nullptr;
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.WorldType == EWorldType::Game) World = C.World();
		}
		ADCGameMode* GM = World ? World->GetAuthGameMode<ADCGameMode>() : nullptr;
		PC = World ? Cast<ADCPlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (!GM || !GM->Config || !PC || !PC->GetPawn()) return false; // world not ready

		switch (Stage)
		{
		case 0:
			if (!PC->CanGoToBackRoom()) return false; // HUD not ready yet
			GM->DayNightCycle->bRunning = false; // no scheduled client: its dialogue would block the last check
			GM->ClientDirector->CloseStore();
			Test->TestEqual(TEXT("look yaw range"), GM->Config->LookYawLimit, 60.f);
			Test->TestEqual(TEXT("look pitch range"), GM->Config->LookPitchLimit, 30.f);
			Test->TestEqual(TEXT("starts at the counter"), PC->GetCameraZone(), ECameraZone::Counter);
			StartLocation = PC->GetPawn()->GetActorLocation();
			PC->GoToBackRoom();
			Test->TestEqual(TEXT("sequence started"), PC->GetCameraZone(), ECameraZone::ToBackRoom);
			Test->TestFalse(TEXT("can't leave again mid-sequence"), PC->CanGoToBackRoom());
			Stage = 1;
			return false;
		case 1: // the crafting panel opens by itself when the sequence ends
			if (PC->GetCameraZone() != ECameraZone::BackRoom) return false;
			Test->TestFalse(TEXT("back room has a panel up"), PC->CanGoToBackRoom());
			Test->TestTrue(TEXT("tapes to combine"), GM->FilmCrafting->SetSlot(0, "VHS_Comedy_1_01"));
			Test->TestTrue(TEXT("confirm"), GM->FilmCrafting->ConfirmFilm(FText::GetEmpty())); // closes the panel
			Stage = 2;
			return false;
		case 2: // the sequence runs in reverse and leaves the camera where it was
			if (PC->GetCameraZone() != ECameraZone::Counter) return false;
			Test->TestTrue(TEXT("back at the same spot"), PC->GetPawn()->GetActorLocation().Equals(StartLocation, 1.f));
			Test->TestTrue(TEXT("can go again"), PC->CanGoToBackRoom());
			return true;
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
	ADCPlayerController* PC = nullptr;
	double Start = 0.0;
	int32 Stage = 0;
	FVector StartLocation = FVector::ZeroVector;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCCameraZoneTest, "DirectorsCut.Camera.BackRoomCycle",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCCameraZoneTest::RunTest(const FString& Parameters)
{
	ADD_LATENT_AUTOMATION_COMMAND(FDCCameraZoneCommand(this));
	return true;
}

#endif
