#include "Core/DCStreetWalkers.h"
#include "Core/DCClient.h"
#include "Core/DCGameMode.h"
#include "Data/DCCustomers.h"
#include "Data/PDA_GameConfig.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADCStreetWalkerSpawner::ADCStreetWalkerSpawner()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ADCStreetWalkerSpawner::BeginPlay()
{
	Super::BeginPlay();
	SpawnTimer = 0.5f; // first pedestrian almost at once
}

void ADCStreetWalkerSpawner::EndPlay(const EEndPlayReason::Type Reason)
{
	for (const FWalker& W : Walkers)
	{
		if (W.Actor.IsValid())
		{
			W.Actor->Destroy();
		}
	}
	Walkers.Empty();
	Super::EndPlay(Reason);
}

void ADCStreetWalkerSpawner::SpawnWalker()
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	const UDataTable* Table = (GM && GM->Config) ? GM->Config->CustomerTable.Get() : nullptr;
	if (!Table || Table->GetRowMap().IsEmpty())
	{
		return;
	}
	TArray<FName> Rows = Table->GetRowNames();
	const FCustomerData* Persona = Table->FindRow<FCustomerData>(Rows[FMath::RandRange(0, Rows.Num() - 1)], TEXT("StreetWalker"));
	USkeletalMesh* Mesh = Persona ? Persona->Mesh.LoadSynchronous() : nullptr;
	if (!Mesh)
	{
		return;
	}

	const bool bForward = FMath::RandBool();
	const FVector Lane = GetActorRightVector() * FMath::FRandRange(-LaneHalfWidth, LaneHalfWidth);
	const FVector From = GetTransform().TransformPosition(bForward ? PathStart : PathEnd) + Lane;
	const FVector To = GetTransform().TransformPosition(bForward ? PathEnd : PathStart) + Lane;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADCClient* Walker = GetWorld()->SpawnActor<ADCClient>(WalkerClass ? WalkerClass.Get() : ADCClient::StaticClass(),
		From, (To - From).GetSafeNormal2D().Rotation(), Params);
	if (!Walker)
	{
		return;
	}
	Walker->ApplyMesh(Mesh);
	// Pedestrians must never block the shop's own clients or the player
	Walker->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// ...nor hide the computer and cash register from the player's interaction trace (ECC_Visibility)
	Walker->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Walker->GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Walker->GetCharacterMovement()->MaxWalkSpeed = FMath::FRandRange(SpeedMin, SpeedMax);
	Walkers.Add({ Walker, To });
}

void ADCStreetWalkerSpawner::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (int32 i = Walkers.Num() - 1; i >= 0; --i)
	{
		ADCClient* W = Walkers[i].Actor.Get();
		if (!W)
		{
			Walkers.RemoveAtSwap(i);
			continue;
		}
		const FVector ToTarget = (Walkers[i].Target - W->GetActorLocation()).GetSafeNormal2D();
		if (FVector::Dist2D(Walkers[i].Target, W->GetActorLocation()) < 60.f)
		{
			W->Destroy();
			Walkers.RemoveAtSwap(i);
			continue;
		}
		W->AddMovementInput(ToTarget, 1.f);
	}

	SpawnTimer -= DeltaSeconds;
	if (SpawnTimer <= 0.f)
	{
		SpawnTimer = FMath::FRandRange(SpawnIntervalMin, SpawnIntervalMax);
		if (Walkers.Num() < MaxWalkers)
		{
			SpawnWalker();
		}
	}
}
