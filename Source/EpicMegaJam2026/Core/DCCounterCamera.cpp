#include "Core/DCCounterCamera.h"
#include "Core/DCGameMode.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADCCounterCamera::ADCCounterCamera()
{
	bUseControllerRotationYaw = true; // the body follows the mouse yaw
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f)); // eye height with the default capsule
	Camera->bUsePawnControlRotation = true;
}

void ADCCounterCamera::BeginPlay()
{
	Super::BeginPlay();
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (GM && GM->Config)
	{
		GetCharacterMovement()->MaxWalkSpeed = GM->Config->WalkSpeed;
	}
}
