// Bodiless pawn: fixed first-person camera. Its actor rotation is the base orientation the look limits are relative to.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DCCounterCamera.generated.h"

class UCameraComponent;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCCounterCamera : public APawn
{
	GENERATED_BODY()
public:
	ADCCounterCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
};
