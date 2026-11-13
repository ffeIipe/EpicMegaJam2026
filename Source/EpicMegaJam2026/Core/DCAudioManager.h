// Audio hook (SystemsDesigns.md section 10). PlayCue is intentionally empty for now: the team will map each cue name
// to an FMOD event in the Cues map and play it (override PlayCue in BP_AudioManager, or fill this function in).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCAudioManager.generated.h"

class UFMODEvent;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCAudioManager : public AActor
{
	GENERATED_BODY()
public:
	// Cue name -> FMOD event. Assigned in the editor; no paths are hardcoded.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio") TMap<FName, TObjectPtr<UFMODEvent>> Cues;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Audio") void PlayCue(FName Name);
	virtual void PlayCue_Implementation(FName Name) {}
};
