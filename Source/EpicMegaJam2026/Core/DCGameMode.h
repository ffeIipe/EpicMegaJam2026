// Holds the rule components and the config reference (SystemsDesigns.md section 2).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Data/PDA_GameConfig.h"
#include "Components/DCComponents.h"
#include "Core/DCAudioManager.h"
#include "Core/DCCounterCamera.h"
#include "DCGameMode.generated.h"

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ADCGameMode();
	virtual void BeginPlay() override;
	// Always the C++ player: BP_CounterCamera was reparented to plain Character in the editor and lost this class.
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController*) override { return ADCCounterCamera::StaticClass(); }

	// Spawned at BeginPlay (BP_AudioManager on BP_DC_GameMode); may stay null.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") TSubclassOf<ADCAudioManager> AudioManagerClass;
	UPROPERTY(BlueprintReadOnly, Category = "Audio") TObjectPtr<ADCAudioManager> AudioManager;
	// Single entry point for every audio cue of the game
	UFUNCTION(BlueprintCallable, Category = "Audio") void PlayCue(FName Name) const { if (AudioManager) AudioManager->PlayCue(Name); }

	// Assign DA_GameConfig on BP_DC_GameMode.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Config") TObjectPtr<UPDA_GameConfig> Config;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAC_Economy> Economy;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAC_DayNightCycle> DayNightCycle;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAC_ClientDirector> ClientDirector;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAC_Inventory> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAC_FilmCrafting> FilmCrafting;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UAC_Unlocks> Unlocks;
};
