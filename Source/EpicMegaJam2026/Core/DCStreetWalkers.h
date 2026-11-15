// Ambient pedestrians: place one in the level, move its two points to the ends of the street.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DCStreetWalkers.generated.h"

class ADCClient;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCStreetWalkerSpawner : public AActor
{
	GENERATED_BODY()
public:
	ADCStreetWalkerSpawner();

	// Pedestrian class: assign BP_Client so the meshes get their animation blueprints (plain ADCClient has none).
	UPROPERTY(EditAnywhere, Category = "Street") TSubclassOf<ADCClient> WalkerClass;
	// Ends of the sidewalk, relative to this actor (drag the gizmos)
	UPROPERTY(EditAnywhere, Category = "Street", meta = (MakeEditWidget)) FVector PathStart = FVector(0.f, -1500.f, 0.f);
	UPROPERTY(EditAnywhere, Category = "Street", meta = (MakeEditWidget)) FVector PathEnd = FVector(0.f, 1500.f, 0.f);
	// Random sideways offset so they do not all follow one line
	UPROPERTY(EditAnywhere, Category = "Street") float LaneHalfWidth = 120.f;
	UPROPERTY(EditAnywhere, Category = "Street") int32 MaxWalkers = 4;
	UPROPERTY(EditAnywhere, Category = "Street") float SpawnIntervalMin = 4.f;
	UPROPERTY(EditAnywhere, Category = "Street") float SpawnIntervalMax = 10.f;
	UPROPERTY(EditAnywhere, Category = "Street") float SpeedMin = 90.f;
	UPROPERTY(EditAnywhere, Category = "Street") float SpeedMax = 140.f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	struct FWalker
	{
		TWeakObjectPtr<ADCClient> Actor;
		FVector Target;
	};

	void SpawnWalker();

	TArray<FWalker> Walkers;
	float SpawnTimer = 0.f;
};
