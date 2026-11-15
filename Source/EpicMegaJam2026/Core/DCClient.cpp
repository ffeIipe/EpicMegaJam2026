#include "Core/DCClient.h"
#include "Core/DCGameMode.h"
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- Browse point / queue

ADCBrowsePoint::ADCBrowsePoint()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ADCCounterPoint::ADCCounterPoint()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

bool ADCCounterPoint::TryClaim(ADCClient* Client)
{
	if (Occupant.IsValid() && Occupant != Client)
	{
		return false;
	}
	Occupant = Client;
	return true;
}

void ADCCounterPoint::Release(ADCClient* Client)
{
	if (Occupant == Client)
	{
		Occupant = nullptr;
	}
}

// ---------------------------------------------------------------- Client

ADCClient::ADCClient()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	// Placeholder body (art replaces the mesh from the archetype row)
	UStaticMeshComponent* Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderBody"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
		Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.7f));
	}
}

void ADCClient::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (AAIController* AI = Cast<AAIController>(NewController))
	{
		AI->ReceiveMoveCompleted.AddDynamic(this, &ADCClient::OnMoveCompleted);
	}
}

void ADCClient::Init(const FClientRuntime& InRuntime, UAC_ClientDirector* InDirector, ADCCounterPoint* InCounter, ADCBrowsePoint* InBrowse,
	FVector InExit, float InPatienceSec, float BrowseTimeSec)
{
	Runtime = InRuntime;
	Director = InDirector;
	Counter = InCounter;
	Browse = InBrowse;
	ExitLocation = InExit;
	PatienceMax = InPatienceSec;
	Runtime.PatienceLeft = InPatienceSec;
	BrowseTime = BrowseTimeSec;
	SetClientState(EClientState::Entering);
	Browse->bOccupied = true;
	MoveToGoal(Browse->GetActorLocation(), EGoal::Browse);
}

void ADCClient::ApplyMesh(USkeletalMesh* InMesh)
{
	if (!InMesh)
	{
		return;
	}
	GetMesh()->SetSkeletalMesh(InMesh);
	// The NPC rigs follow Mixamo (facing +Y), which is what lets Mixamo clips play on them unchanged
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	if (const TSubclassOf<UAnimInstance>* Anim = AnimBySkeleton.Find(InMesh->GetSkeleton()))
	{
		GetMesh()->SetAnimInstanceClass(*Anim);
	}
	if (UStaticMeshComponent* Body = FindComponentByClass<UStaticMeshComponent>())
	{
		Body->SetVisibility(false);
	}
}

void ADCClient::MoveToGoal(FVector Location, EGoal NewGoal)
{
	Goal = NewGoal;
	GoalLocation = Location;
	AAIController* AI = Cast<AAIController>(GetController());
	const EPathFollowingRequestResult::Type Request = AI ? AI->MoveToLocation(Location, 20.f) : EPathFollowingRequestResult::Failed;
	if (Request == EPathFollowingRequestResult::Failed)
	{
		// No nav mesh / no path: snap so the flow never stalls
		SetActorLocation(Location + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		Arrive();
	}
	else if (Request == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		Arrive();
	}
}

void ADCClient::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
	if (Result != EPathFollowingResult::Success)
	{
		SetActorLocation(GoalLocation + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	}
	Arrive();
}

void ADCClient::Arrive()
{
	const EGoal Done = Goal;
	Goal = EGoal::None;
	switch (Done)
	{
	case EGoal::Browse:
		SetClientState(EClientState::Browsing);
		GetWorldTimerManager().SetTimer(BrowseTimer, this, &ADCClient::TryGoToCounter, BrowseTime);
		break;
	case EGoal::Counter:
		if (Runtime.State == EClientState::Queued)
		{
			SetClientState(EClientState::AtCounter); // patience starts now
			if (Director.IsValid())
			{
				Director->NotifyReachedCounter(this);
			}
		}
		break;
	case EGoal::Exit:
		Destroy();
		break;
	default:
		break;
	}
}

void ADCClient::TryGoToCounter()
{
	if (!Counter.IsValid())
	{
		StartLeaving(false);
		return;
	}
	if (!Counter->TryClaim(this))
	{
		// Counter busy: keep looking at the tapes and ask again shortly
		GetWorldTimerManager().SetTimer(BrowseTimer, this, &ADCClient::TryGoToCounter, 1.f);
		return;
	}
	if (Browse.IsValid())
	{
		Browse->bOccupied = false;
	}
	SetClientState(EClientState::Queued); // walking to the counter
	MoveToGoal(Counter->GetActorLocation(), EGoal::Counter);
}

void ADCClient::MarkServed(const FText& Farewell)
{
	SetClientState(EClientState::Served);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::White, Farewell.ToString()); // placeholder speech bubble
	}
	StartLeaving(true);
}

void ADCClient::ForceLeave()
{
	if (Runtime.State != EClientState::Leaving)
	{
		StartLeaving(false);
	}
}

void ADCClient::StartLeaving(bool bServed)
{
	GetWorldTimerManager().ClearTimer(BrowseTimer);
	if (Browse.IsValid())
	{
		Browse->bOccupied = false;
	}
	if (Counter.IsValid())
	{
		Counter->Release(this);
	}
	SetClientState(EClientState::Leaving);
	if (Director.IsValid())
	{
		Director->NotifyLeft(this, bServed);
	}
	MoveToGoal(ExitLocation, EGoal::Exit);
}

bool ADCClient::bShowDebugAttributes = false;

void ADCClient::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
	if (bShowDebugAttributes)
	{
		FString Text = FString::Printf(TEXT("Tier %d\n"), Runtime.Tier);
		for (int32 i = 0; i < DCNumSkills; ++i)
		{
			Text += FString::Printf(TEXT("%s %d\n"), *StaticEnum<ESkill>()->GetNameStringByValue(i), Runtime.Target.Get((ESkill)i));
		}
		DrawDebugString(GetWorld(), FVector(0.f, 0.f, 140.f), Text, this, FColor::Yellow, 0.f);
	}
#endif
	// The clock starts once the player says "Coming right up"
	if (Runtime.State == EClientState::AtCounter && bOrderAcknowledged)
	{
		if (Director.IsValid() && Director->bTutorialPaused)
		{
			return;
		}
		Runtime.PatienceLeft -= DeltaSeconds;
		if (Runtime.PatienceLeft <= 0.f)
		{
			if (Director.IsValid())
			{
				Director->ApplyLeavePenalty();
			}
			StartLeaving(false); // leaves unpaid
		}
	}
}
