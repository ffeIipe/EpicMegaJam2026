#include "Core/DCClient.h"
#include "Core/DCGameMode.h"
#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------- Browse point / queue

ADCBrowsePoint::ADCBrowsePoint()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

ADCClientQueue::ADCClientQueue()
{
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

int32 ADCClientQueue::MaxSize() const
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	return GM && GM->Config ? FMath::Min(Points.Num(), GM->Config->MaxQueueSize) : Points.Num();
}

FVector ADCClientQueue::GetPointLocation(int32 Index) const
{
	return GetActorTransform().TransformPosition(Points[FMath::Clamp(Index, 0, Points.Num() - 1)]);
}

int32 ADCClientQueue::TryJoin(ADCClient* Client)
{
	if (Slots.Num() >= MaxSize())
	{
		return -1;
	}
	return Slots.Add(Client);
}

void ADCClientQueue::Remove(ADCClient* Client)
{
	if (Slots.Remove(Client) == 0)
	{
		return;
	}
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (ADCClient* C = Slots[i].Get())
		{
			C->OnQueueIndex(i); // everyone behind steps forward
		}
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

void ADCClient::Init(const FClientRuntime& InRuntime, UAC_ClientDirector* InDirector, ADCClientQueue* InQueue, ADCBrowsePoint* InBrowse,
	FVector InExit, float InPatienceSec, float BrowseTimeSec)
{
	Runtime = InRuntime;
	Director = InDirector;
	Queue = InQueue;
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
		GetWorldTimerManager().SetTimer(BrowseTimer, this, &ADCClient::JoinQueue, BrowseTime);
		break;
	case EGoal::Queue:
		if (QueueIndex == 0 && Runtime.State == EClientState::Queued)
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

void ADCClient::JoinQueue()
{
	if (Browse.IsValid())
	{
		Browse->bOccupied = false;
	}
	QueueIndex = Queue.IsValid() ? Queue->TryJoin(this) : -1;
	if (QueueIndex < 0)
	{
		StartLeaving(false); // shouldn't happen: the director gates spawns by queue size
		return;
	}
	SetClientState(EClientState::Queued);
	MoveToGoal(Queue->GetPointLocation(QueueIndex), EGoal::Queue);
}

void ADCClient::OnQueueIndex(int32 Index)
{
	QueueIndex = Index;
	if (Runtime.State == EClientState::Queued)
	{
		MoveToGoal(Queue->GetPointLocation(Index), EGoal::Queue);
	}
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
	if (Queue.IsValid())
	{
		Queue->Remove(this);
	}
	QueueIndex = -1;
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
		FString Text;
		for (const FGenrePreference& P : Runtime.Preferences)
		{
			Text += FString::Printf(TEXT("%s %+d\n"),*StaticEnum<EGenre>()->GetNameStringByValue((int64)P.Genre), P.Weight);
		}
		DrawDebugString(GetWorld(), FVector(0.f, 0.f, 140.f), Text, this, FColor::Yellow, 0.f);
	}
#endif
	if (Runtime.State == EClientState::AtCounter)
	{
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
