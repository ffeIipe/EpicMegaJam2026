// Centralized FMOD audio manager: resolves cues from UPDA_AudioConfig (or the legacy Cues map) and plays 2D, 3D and looping events.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/DCTypes.h"
#include "FMODBlueprintStatics.h"
#include "DCAudioManager.generated.h"

class UFMODEvent;
class UPDA_AudioConfig;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCAudioManager : public AActor
{
	GENERATED_BODY()

public:
	// Primary audio Data Asset; if unset, resolved from GameMode->Config, DA_AudioConfig asset paths, or a transient default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<UPDA_AudioConfig> AudioConfig;

	// Legacy Cue name -> FMOD event override map. Checked before AudioConfig so BP_AudioManager overrides still win.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TMap<FName, TObjectPtr<UFMODEvent>> Cues;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "Audio")
	UFMODEvent* ResolveEvent(FName Name) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Audio")
	void PlayCue(FName Name);
	virtual void PlayCue_Implementation(FName Name);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayCueAtLocation(FName Name, FVector Location);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetZoneAmbience(ECameraZone Zone);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetPhaseMusic(EPhase Phase);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StartMainMenuAudio();

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void StopAllLoops();

	static ADCAudioManager* Get(const UObject* WorldContextObject);
	static void PlayCueStatic(const UObject* WorldContextObject, FName Name);
	static void PlayCueAtLocationStatic(const UObject* WorldContextObject, FName Name, FVector Location);

private:
	void EnsureAudioConfig();
	void StartOrSwitchLoop(FFMODEventInstance& Instance, TWeakObjectPtr<UFMODEvent>& CurrentEvent, UFMODEvent* TargetEvent);
	void SetOptionalGlobalParameter(const char* Name, float Value) const;

	FFMODEventInstance AmbienceInstance;
	FFMODEventInstance LightBuzzInstance;
	FFMODEventInstance MusicInstance;
	FFMODEventInstance ComputerInstance;
	TWeakObjectPtr<UFMODEvent> CurrentAmbienceEvent;
	TWeakObjectPtr<UFMODEvent> CurrentLightBuzzEvent;
	TWeakObjectPtr<UFMODEvent> CurrentMusicEvent;
	bool bCurrentBackRoom = false;
	EPhase CurrentPhase = EPhase::Day;
};
