#include "Core/DCMainMenuMode.h"
#include "Core/DCSettings.h"
#include "UI/DCCommonUI.h"
#include "UI/DCMenuWidgets.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

ADCMainMenuGameMode::ADCMainMenuGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ADCMainMenuPlayerController::StaticClass();
}

void ADCMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!Config)
	{
		Config = LoadObject<UPDA_GameConfig>(nullptr, TEXT("/Game/DirectorsCut/Data/DA_GameConfig.DA_GameConfig"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	TSubclassOf<ADCAudioManager> AudioCls = AudioManagerClass;
	if (!AudioCls)
	{
		AudioCls = LoadClass<ADCAudioManager>(nullptr, TEXT("/Game/DirectorsCut/Audio/BP_AudioManager.BP_AudioManager_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!AudioCls)
	{
		AudioCls = ADCAudioManager::StaticClass();
	}
	AudioManager = GetWorld()->SpawnActor<ADCAudioManager>(AudioCls);

	// FMOD bracket: start main menu music/ambience once the audio data asset / BP_AudioManager is wired.
	PlayCue(TEXT("MainMenuMusic"));
}

void ADCMainMenuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UDCSettings::Get()->Apply();
	bShowMouseCursor = true;

	RootLayout = CreateWidget<UDCRootLayout>(this, UDCRootLayout::StaticClass());
	if (!RootLayout)
	{
		return;
	}
	RootLayout->AddToViewport(0);
	RootLayout->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	TSubclassOf<UDCMainMenuWidget> MenuCls = MainMenuWidgetClass;
	if (!MenuCls)
	{
		MenuCls = LoadClass<UDCMainMenuWidget>(nullptr, TEXT("/Game/DirectorsCut/UI/WBP_MainMenu.WBP_MainMenu_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!MenuCls)
	{
		MenuCls = UDCMainMenuWidget::StaticClass();
	}
	RootLayout->Stack->ClearWidgets();
	RootLayout->Stack->AddWidget<UDCMainMenuWidget>(MenuCls);
}

void ADCMainMenuPlayerController::PlayCue(FName Name) const
{
	if (const ADCMainMenuGameMode* GM = GetWorld()->GetAuthGameMode<ADCMainMenuGameMode>())
	{
		GM->PlayCue(Name);
	}
}
