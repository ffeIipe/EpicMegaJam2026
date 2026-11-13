#include "Core/DCGameMode.h"
#include "Core/DCCounterCamera.h"
#include "Core/DCPlayerController.h"

ADCGameMode::ADCGameMode()
{
	Economy = CreateDefaultSubobject<UAC_Economy>(TEXT("Economy"));
	DayNightCycle = CreateDefaultSubobject<UAC_DayNightCycle>(TEXT("DayNightCycle"));
	ClientDirector = CreateDefaultSubobject<UAC_ClientDirector>(TEXT("ClientDirector"));
	Inventory = CreateDefaultSubobject<UAC_Inventory>(TEXT("Inventory"));
	FilmCrafting = CreateDefaultSubobject<UAC_FilmCrafting>(TEXT("FilmCrafting"));
	DefaultPawnClass = ADCCounterCamera::StaticClass();
	PlayerControllerClass = ADCPlayerController::StaticClass();
}

void ADCGameMode::BeginPlay()
{
	Super::BeginPlay();
	TSubclassOf<ADCAudioManager> Cls = AudioManagerClass ? AudioManagerClass : TSubclassOf<ADCAudioManager>(ADCAudioManager::StaticClass());
	AudioManager = GetWorld()->SpawnActor<ADCAudioManager>(Cls);
}
