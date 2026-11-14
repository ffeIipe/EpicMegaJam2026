// First-person camera fixed at the counter: bodiless and without movement (the controller limits the look range
// and the back-room sequence moves it).
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
