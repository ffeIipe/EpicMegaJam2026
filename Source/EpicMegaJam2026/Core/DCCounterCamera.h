// First-person player: bodiless (camera only), walks with WASD; the controller drives look and movement input.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DCCounterCamera.generated.h"

class UCameraComponent;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCCounterCamera : public ACharacter
{
	GENERATED_BODY()
public:
	ADCCounterCamera();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
};
