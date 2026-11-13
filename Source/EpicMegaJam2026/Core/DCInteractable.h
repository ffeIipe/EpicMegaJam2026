// Clickable world props (SystemsDesigns.md section 9): cash register, back room door, shelves and the VHS player.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/DCTypes.h"
#include "DCInteractable.generated.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UWidgetComponent;

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
	// Hidden: only the anchor of LabelWidget now, kept so the label placement already set per prop in the maps survives
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextRenderComponent> MoneyDisplay;
	// Floating label in the UI style: the prop name, or the money on the cash register
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UWidgetComponent> LabelWidget;

	virtual void BeginPlay() override;

	// What the prop does, shown above it in the world and in the HUD prompt when aimed at. Empty = no label.
	FText GetLabel() const;

private:
	UFUNCTION() void OnMoneyChanged(int32 NewMoney, int32 Delta);
	void SetLabelText(const FText& Text) const;
};
