// Store NPC, counter and browse points (SystemsDesigns.md sections 5.5 and 9).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/DCTypes.h"
#include "AITypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "DCClient.generated.h"

class ADCCounterPoint;
class UAC_ClientDirector;

// Shelf spot where a client lingers until the counter is free
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCBrowsePoint : public AActor
{
	GENERATED_BODY()
public:
	ADCBrowsePoint();
	bool bOccupied = false;
};

// Where the served client stands: move this actor in the level. There is no queue; the others keep browsing until it is free.
UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCCounterPoint : public AActor
{
	GENERATED_BODY()
public:
	ADCCounterPoint();

	// True if the client got the spot (or already had it)
	bool TryClaim(class ADCClient* Client);
	void Release(class ADCClient* Client);
	ADCClient* GetClient() const { return Occupant.Get(); }

private:
	TWeakObjectPtr<ADCClient> Occupant;
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

	void Init(const FClientRuntime& InRuntime, UAC_ClientDirector* InDirector, ADCCounterPoint* InCounter, ADCBrowsePoint* InBrowse,
		FVector InExit, float InPatienceSec, float BrowseTimeSec);

	// ABP to use per skeleton (the NPC meshes share one male and one female skeleton); set in BP_Client.
	UPROPERTY(EditDefaultsOnly, Category = "Animation") TMap<TObjectPtr<class USkeleton>, TSubclassOf<class UAnimInstance>> AnimBySkeleton;

	// Swaps the placeholder cube for the archetype's skeletal mesh (DT_ClientArchetypes.Mesh); null keeps the cube.
	void ApplyMesh(class USkeletalMesh* InMesh);

	bool IsAtFront() const { return Runtime.State == EClientState::AtCounter; }
	bool IsFemaleCustomer() const;
	void PlayTalkSound() const;
	// Called by the director after payment: farewell line, then leave as served.
	void MarkServed(const FText& Farewell);
	void ForceLeave(); // night closing: leave with no penalty

	// F1 debug (development builds only)
	static bool bShowDebugAttributes;

	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;

private:
	enum class EGoal : uint8 { None, Browse, Counter, Exit };

	void MoveToGoal(FVector Location, EGoal NewGoal);
	void Arrive();
	void StartLeaving(bool bServed);
	void SetClientState(EClientState S) { Runtime.State = S; }
	UFUNCTION() void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);
	void TryGoToCounter();

	TWeakObjectPtr<UAC_ClientDirector> Director;
	TWeakObjectPtr<ADCCounterPoint> Counter;
	TWeakObjectPtr<ADCBrowsePoint> Browse;
	FVector ExitLocation = FVector::ZeroVector;
	FVector GoalLocation = FVector::ZeroVector;
	float BrowseTime = 3.f;
	float FootstepTimer = 0.f;
	EGoal Goal = EGoal::None;
	FTimerHandle BrowseTimer;
};
