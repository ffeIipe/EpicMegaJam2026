#include "Core/DCCounterCamera.h"
#include "Camera/CameraComponent.h"

ADCCounterCamera::ADCCounterCamera()
{
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = true;
}
