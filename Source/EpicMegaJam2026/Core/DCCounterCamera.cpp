#include "Core/DCCounterCamera.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADCCounterCamera::ADCCounterCamera()
{
	bUseControllerRotationYaw = true; // the body follows the mouse yaw
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision); // the player doesn't take up space: clients walk through the spot
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // eye height with the default capsule
	Camera->bUsePawnControlRotation = true;
}

void ADCCounterCamera::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->DisableMovement(); // no gravity, no walking
}
