#include "Core/DCSettings.h"
#include "FMODStudioModule.h"
#include "fmod_studio.hpp"

FKey& UDCSettings::Key(EDCAction Action)
{
	switch (Action)
	{
	case EDCAction::MoveForward: return MoveForward;
	case EDCAction::MoveBack: return MoveBack;
	case EDCAction::MoveLeft: return MoveLeft;
	case EDCAction::MoveRight: return MoveRight;
	case EDCAction::ReturnTape: return ReturnTape;
	default: return Interact;
	}
}

const TCHAR* UDCSettings::ActionLabel(EDCAction Action)
{
	static const TCHAR* Labels[] = {TEXT("Interact"), TEXT("Move forward"), TEXT("Move back"), TEXT("Move left"), TEXT("Move right"), TEXT("Return tape")};
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
