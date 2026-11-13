// Store NPC, queue and browse points (SystemsDesigns.md sections 5.5 and 9).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/DCTypes.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "DCClient.generated.h"

class ADCClientQueue;
class UAC_ClientDirector;

// Shelf spot where a client lingers before queueing
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCBrowsePoint : public AActor
{
	GENERATED_BODY()
public:
	ADCBrowsePoint();
	bool bOccupied = false;
};

// Ordered queue points in front of the counter; point 0 is the front.
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCClientQueue : public AActor
{
	GENERATED_BODY()
public:
	ADCClientQueue();

	// Queue point positions relative to this actor. Index 0 = front (at the counter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Queue", meta = (MakeEditWidget = true))
	TArray<FVector> Points = { FVector(0, 0, 0), FVector(150, 0, 0), FVector(300, 0, 0) };

	// Returns the joined index, or -1 if the queue is full (size = min(points, MaxQueueSize)).
	int32 TryJoin(class ADCClient* Client);
	void Remove(class ADCClient* Client);
	ADCClient* GetFront() const { return Slots.Num() > 0 ? Slots[0].Get() : nullptr; }
	FVector GetPointLocation(int32 Index) const;
	int32 MaxSize() const;

private:
	TArray<TWeakObjectPtr<ADCClient>> Slots;
};

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCClient : public ACharacter
{
	GENERATED_BODY()
public:
	ADCClient();

	UPROPERTY(BlueprintReadOnly) FClientRuntime Runtime;
	UPROPERTY(BlueprintReadOnly) float PatienceMax = 60.f;
	// Player already heard the order and said "Coming right up"
	UPROPERTY(BlueprintReadWrite) bool bOrderAcknowledged = false;

	void Init(const FClientRuntime& InRuntime, UAC_ClientDirector* InDirector, ADCClientQueue* InQueue, ADCBrowsePoint* InBrowse,
		FVector InExit, float InPatienceSec, float BrowseTimeSec);

	// Swaps the placeholder cube for the archetype's skeletal mesh (DT_ClientArchetypes.Mesh); null keeps the cube.
	void ApplyMesh(class USkeletalMesh* InMesh);

	bool IsAtFront() const { return Runtime.State == EClientState::AtCounter; }
	void OnQueueIndex(int32 Index);
	// Called by the director after payment: farewell line, then leave as served.
	void MarkServed(const FText& Farewell);
	void ForceLeave(); // night closing: leave with no penalty

	// F1 debug (development builds only)
	static bool bShowDebugAttributes;

	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;

private:
	enum class EGoal : uint8 { None, Browse, Queue, Exit };

	void MoveToGoal(FVector Location, EGoal NewGoal);
	void Arrive();
	void StartLeaving(bool bServed);
	void SetClientState(EClientState S) { Runtime.State = S; }
	UFUNCTION() void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
	void JoinQueue();

	TWeakObjectPtr<UAC_ClientDirector> Director;
	TWeakObjectPtr<ADCClientQueue> Queue;
	TWeakObjectPtr<ADCBrowsePoint> Browse;
	FVector ExitLocation = FVector::ZeroVector;
	FVector GoalLocation = FVector::ZeroVector;
	float BrowseTime = 3.f;
	int32 QueueIndex = -1;
	EGoal Goal = EGoal::None;
	FTimerHandle BrowseTimer;
};
