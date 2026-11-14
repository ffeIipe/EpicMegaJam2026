// Parent class of the client ABPs (one per skeleton): feeds Speed to the Idle/Walk state machine.
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DCClientAnim.generated.h"

UCLASS()
class EPICMEGAJAM2026_API UDCClientAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	// Ground speed in cm/s
	UPROPERTY(BlueprintReadOnly, Category = "Locomotion") float Speed = 0.f;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
