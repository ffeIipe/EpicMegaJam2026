// Centralized FMOD audio manager: resolves cues from UPDA_AudioConfig (or the legacy Cues map) and plays 2D, 3D and looping events.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/DCTypes.h"
#include "FMODBlueprintStatics.h"
#include "DCAudioManager.generated.h"

class UFMODEvent;
class UPDA_AudioConfig;
struct FDCAudioCue;

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
	void PlayCueWithParameters(FName Name, const TMap<FName, float>& Parameters);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayCueAtLocation(FName Name, FVector Location);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void PlayCueAtLocationWithParameters(FName Name, FVector Location, const TMap<FName, float>& Parameters);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetGlobalParameter(FName ParameterName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetCueParameter(FName CueName, FName ParameterName, float Value);

	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetPauseOrDialogueActive(bool bActive);

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
	static void EnsurePersistentMusicStarted(const UObject* WorldContextObject, UPDA_AudioConfig* OptionalConfig = nullptr);
	static void StopPersistentMusic();
	static void SetGlobalParameterStatic(const char* Name, float Value);

private:
	void EnsureAudioConfig();
	const FDCAudioCue* ResolveCueStruct(FName Name) const;
	void PlayCueInternal(FName Name, bool bAtLocation, FVector Location, const TMap<FName, float>* ExtraParameters);
	void StartOrSwitchLoop(FFMODEventInstance& Instance, TWeakObjectPtr<UFMODEvent>& CurrentEvent, const FDCAudioCue* TargetCue);
	static void ApplyParametersToInstance(FFMODEventInstance Instance, const TMap<FName, float>& Parameters);

	FFMODEventInstance AmbienceInstance;
	FFMODEventInstance LightBuzzInstance;
	FFMODEventInstance ComputerInstance;
	FFMODEventInstance PauseOrDialogueInstance;
	TWeakObjectPtr<UFMODEvent> CurrentAmbienceEvent;
	TWeakObjectPtr<UFMODEvent> CurrentLightBuzzEvent;
	bool bCurrentBackRoom = false;
	EPhase CurrentPhase = EPhase::Day;
};
