// Editor-time helper called from Tools/NpcSkeletonFix (Python): USkeleton's retargeting setter isn't exposed to scripts.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DCSkeletonTools.generated.h"

UCLASS()
class EPICMEGAJAM2026_API UDCSkeletonTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Mixamo clips carry another character's bone lengths: keep this skeleton's own, and scale Hips so the height follows the clip.
	UFUNCTION(BlueprintCallable, Category = "DirectorsCut|Editor")
	static void UseSkeletonProportions(USkeleton* Skeleton);
};
