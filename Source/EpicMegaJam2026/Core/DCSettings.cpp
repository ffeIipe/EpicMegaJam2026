#include "Core/DCSettings.h"
#include "FMODStudioModule.h"
#include "fmod_studio.hpp"

FKey& UDCSettings::Key(EDCAction Action)
{
	return Action == EDCAction::BackRoom ? BackRoom : Interact;
}

const TCHAR* UDCSettings::ActionLabel(EDCAction Action)
{
	static const TCHAR* Labels[] = {TEXT("Interact"), TEXT("Go to the back room")};
	return Labels[(int32)Action];
}

void UDCSettings::Apply() const
{
	if (!IFMODStudioModule::IsAvailable())
	{
		return;
	}
	if (FMOD::Studio::System* System = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime))
	{
		FMOD::Studio::Bus* Master = nullptr;
		if (System->getBus("bus:/", &Master) == FMOD_OK)
		{
			Master->setVolume(MasterVolume);
		}
	}
}

void UDCSettings::Save()
{
	SaveConfig();
	Apply();
}
