#include "Core/DCClientAnim.h"
#include "GameFramework/Pawn.h"

void UDCClientAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	const APawn* Owner = TryGetPawnOwner();
	Speed = Owner ? Owner->GetVelocity().Size2D() : 0.f;
}
