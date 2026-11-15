// Satirical title generator for films mixed with an empty title field. Words follow the two strongest skills of the profile.
#pragma once

#include "CoreMinimal.h"
#include "Data/DCTypes.h"

namespace DCFilmTitles
{
	// A title of at most MaxLength characters that is not in Used. Empty when no fresh one was found in the tries (caller falls back).
	EPICMEGAJAM2026_API FString Generate(const FSkillVector& Profile, const TSet<FString>& Used, int32 MaxLength);
}
