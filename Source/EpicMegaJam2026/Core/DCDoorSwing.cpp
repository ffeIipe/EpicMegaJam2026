#include "Core/DCDoorSwing.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"

UDCDoorSwing::UDCDoorSwing()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDCDoorSwing::Pulse(UObject* WorldContext, const FVector& NearLocation, float HoldSeconds)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return;
	}
	AActor* Best = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("GlassDoor")) // BP_GlassDoor_C
			&& (!Best || FVector::DistSquared(It->GetActorLocation(), NearLocation) < FVector::DistSquared(Best->GetActorLocation(), NearLocation)))
		{
			Best = *It;
		}
	}
	if (!Best)
	{
		return;
	}
	UDCDoorSwing* Swing = Best->FindComponentByClass<UDCDoorSwing>();
	if (!Swing)
	{
		Swing = NewObject<UDCDoorSwing>(Best);
		Swing->RegisterComponent();
	}
	Swing->Open(HoldSeconds);
}

void UDCDoorSwing::Open(float HoldSeconds)
{
	if (!Pane)
	{
		TArray<UStaticMeshComponent*> Meshes;
		GetOwner()->GetComponents(Meshes);
		for (UStaticMeshComponent* M : Meshes)
		{
			if (M->GetName().Contains(TEXT("GlassDoor"))) // the door pane, not the posters or the handle
			{
				Pane = M;
				ClosedRotation = M->GetRelativeRotation();
				break;
			}
		}
	}
	if (Pane)
	{
		HoldLeft = FMath::Max(HoldLeft, HoldSeconds);
		SetComponentTickEnabled(true);
	}
}

void UDCDoorSwing::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTick)
{
	Super::TickComponent(DeltaTime, TickType, ThisTick);
	HoldLeft = FMath::Max(0.f, HoldLeft - DeltaTime);
	Alpha = FMath::FInterpTo(Alpha, HoldLeft > 0.f ? 1.f : 0.f, DeltaTime, SwingSpeed);
	Pane->SetRelativeRotation(ClosedRotation + FRotator(0.f, OpenYaw * Alpha, 0.f));
	if (HoldLeft <= 0.f && Alpha < 0.01f)
	{
		Alpha = 0.f;
		Pane->SetRelativeRotation(ClosedRotation);
		SetComponentTickEnabled(false);
	}
}
