#include "Core/DCSkeletonTools.h"
#include "Animation/Skeleton.h"

void UDCSkeletonTools::UseSkeletonProportions(USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		return;
	}
	const int32 NumBones = Skeleton->GetReferenceSkeleton().GetNum();
	for (int32 Bone = 0; Bone < NumBones; ++Bone)
	{
		Skeleton->SetBoneTranslationRetargetingMode(Bone,
			Bone == 0 ? EBoneTranslationRetargetingMode::AnimationScaled : EBoneTranslationRetargetingMode::Skeleton, false);
	}
	Skeleton->MarkPackageDirty();
}
