// GameMode and PlayerController dedicated to MAP_MainMenu (runs without gameplay systems, clients, or day cycle).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Core/DCAudioManager.h"
#include "Data/PDA_GameConfig.h"
#include "DCMainMenuMode.generated.h"

class UDCMainMenuWidget;
class UDCIntroWidget;
class UDCRootLayout;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCMainMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADCMainMenuGameMode();
	virtual void BeginPlay() override;

	// Spawned at BeginPlay; falls back to /Game/DirectorsCut/Audio/BP_AudioManager when unset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") TSubclassOf<ADCAudioManager> AudioManagerClass;
	UPROPERTY(BlueprintReadOnly, Category = "Audio") TObjectPtr<ADCAudioManager> AudioManager;

	// FMOD bracket: team maps "MainMenuMusic", "Click", etc. in BP_AudioManager / DataAsset.
	UFUNCTION(BlueprintCallable, Category = "Audio") void PlayCue(FName Name) const { if (AudioManager) AudioManager->PlayCue(Name); }

	// Used to read StartMoney for new save slots; falls back to /Game/DirectorsCut/Data/DA_GameConfig when unset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config") TObjectPtr<UPDA_GameConfig> Config;
};

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCMainMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCMainMenuWidget> MainMenuWidgetClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI") TSubclassOf<UDCIntroWidget> IntroWidgetClass;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Audio") void PlayCue(FName Name) const;

private:
	void ShowMainMenu();
	UFUNCTION() void HandleIntroFinished();

	UPROPERTY() TObjectPtr<UDCRootLayout> RootLayout;
};
