#include "Core/DCMainMenuMode.h"
#include "Core/DCGameInstance.h"
#include "Core/DCSettings.h"
#include "UI/DCCommonUI.h"
#include "UI/DCIntroWidget.h"
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

	// Start looping main menu music from AudioConfig (stopped automatically on EndPlay when travelling).
	// While the startup intro is pending the player controller starts it afterwards.
	const UDCGameInstance* GI = GetGameInstance<UDCGameInstance>();
	if (AudioManager && (!GI || GI->bStartupIntroPlayed))
	{
		AudioManager->StartMainMenuAudio();
	}
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

	UDCGameInstance* GI = GetGameInstance<UDCGameInstance>();
	if (GI && !GI->bStartupIntroPlayed)
	{
		TSubclassOf<UDCIntroWidget> IntroCls = IntroWidgetClass;
		if (!IntroCls)
		{
			IntroCls = LoadClass<UDCIntroWidget>(nullptr, TEXT("/Game/DirectorsCut/UI/WBP_Intro.WBP_Intro_C"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		if (!IntroCls)
		{
			IntroCls = UDCIntroWidget::StaticClass();
		}
		RootLayout->Stack->AddWidget<UDCIntroWidget>(IntroCls)->OnFinished.AddDynamic(this, &ADCMainMenuPlayerController::HandleIntroFinished);
		return;
	}
	ShowMainMenu();
}

void ADCMainMenuPlayerController::HandleIntroFinished()
{
	if (UDCGameInstance* GI = GetGameInstance<UDCGameInstance>())
	{
		GI->bStartupIntroPlayed = true;
	}
	ShowMainMenu();
	// The game mode held the menu music back while the intro was up
	if (const ADCMainMenuGameMode* GM = GetWorld()->GetAuthGameMode<ADCMainMenuGameMode>())
	{
		if (GM->AudioManager)
		{
			GM->AudioManager->StartMainMenuAudio();
		}
	}
}

void ADCMainMenuPlayerController::ShowMainMenu()
{
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
