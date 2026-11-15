// Swings the store's glass door open when a client comes in or leaves, then closes it. Added at runtime to the level's BP_GlassDoor,
// so the Blueprint itself needs no changes.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DCDoorSwing.generated.h"

UCLASS()
class EPICMEGAJAM2026_API UDCDoorSwing : public UActorComponent
{
	GENERATED_BODY()
public:
	UDCDoorSwing();

	// Opens the door of the level (the BP_GlassDoor nearest to NearLocation) and closes it again after HoldSeconds. No door found: does nothing.
	static void Pulse(UObject* WorldContext, const FVector& NearLocation, float HoldSeconds = 5.f);

	// Degrees the door pane turns around its own pivot; flip the sign if it opens outwards
	UPROPERTY(EditAnywhere, Category = "Door") float OpenYaw = -90.f;
	UPROPERTY(EditAnywhere, Category = "Door") float SwingSpeed = 6.f;

	void Open(float HoldSeconds);

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick) override;

private:
	UPROPERTY() TObjectPtr<class USceneComponent> Pane;
	FRotator ClosedRotation = FRotator::ZeroRotator;
	float HoldLeft = 0.f;
	float Alpha = 0.f;
};
