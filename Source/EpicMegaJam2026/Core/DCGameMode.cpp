#include "Core/DCGameMode.h"
#include "Core/DCCounterCamera.h"
#include "Core/DCPlayerController.h"
#include "Core/DCGameInstance.h"

ADCGameMode::ADCGameMode()
{
	Economy = CreateDefaultSubobject<UAC_Economy>(TEXT("Economy"));
	DayNightCycle = CreateDefaultSubobject<UAC_DayNightCycle>(TEXT("DayNightCycle"));
	ClientDirector = CreateDefaultSubobject<UAC_ClientDirector>(TEXT("ClientDirector"));
	Inventory = CreateDefaultSubobject<UAC_Inventory>(TEXT("Inventory"));
	FilmCrafting = CreateDefaultSubobject<UAC_FilmCrafting>(TEXT("FilmCrafting"));
	Unlocks = CreateDefaultSubobject<UAC_Unlocks>(TEXT("Unlocks"));
	DefaultPawnClass = ADCCounterCamera::StaticClass();
	PlayerControllerClass = ADCPlayerController::StaticClass();
}

void ADCGameMode::BeginPlay()
{
	Super::BeginPlay();
	TSubclassOf<ADCAudioManager> Cls = AudioManagerClass;
	if (!Cls)
	{
		Cls = LoadClass<ADCAudioManager>(nullptr, TEXT("/Game/DirectorsCut/Audio/BP_AudioManager.BP_AudioManager_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!Cls)
	{
		Cls = ADCAudioManager::StaticClass();
	}
	AudioManager = GetWorld()->SpawnActor<ADCAudioManager>(Cls);
}

void ADCGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Covers returning to the menu and quitting; components are still valid before Super.
	SaveProgress();
	Super::EndPlay(EndPlayReason);
}

void ADCGameMode::SaveProgress() const
{
	// After game over/victory the slot is reset to day one by the end screen; saving here would undo that.
	if (Economy && (Economy->bGameIsOver || Economy->bWon))
	{
		return;
	}
	if (UDCGameInstance* GI = GetGameInstance<UDCGameInstance>())
	{
		GI->CaptureGameModeToActiveSave(this);
	}
}
