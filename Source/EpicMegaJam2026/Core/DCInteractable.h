// Clickable world props (SystemsDesigns.md section 9): cash register, back room door, shelves and the VHS player.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/DCTypes.h"
#include "DCInteractable.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class EPICMEGAJAM2026_API ADCInteractable : public AActor
{
	GENERATED_BODY()
public:
	ADCInteractable();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction") EInteractType InteractType = EInteractType::CashRegister;
	// For shelves: the genre the inventory list is filtered by
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction") EGenre ShelfGenre = EGenre::Horror;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
	// Only used by the cash register: the 3D money counter
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> MoneyDisplay;

	virtual void BeginPlay() override;

private:
	UFUNCTION() void OnMoneyChanged(int32 NewMoney, int32 Delta);
};
