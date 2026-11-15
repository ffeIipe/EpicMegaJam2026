#include "Core/DCSettings.h"
#include "FMODStudioModule.h"
#include "fmod_studio.hpp"
#include "Scalability.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"

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
	// Graphics go through cvars so nothing is written to the engine's own scalability ini
	if (Scalability::GetQualityLevels().GetSingleQualityLevel() != Quality)
	{
		Scalability::FQualityLevels Levels;
		Levels.SetFromSingleQualityLevel(FMath::Clamp(Quality, 0, 3));
		Scalability::SetQualityLevels(Levels, true);
	}
	auto SetCVar = [](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	};
	SetCVar(TEXT("r.DynamicGlobalIlluminationMethod"), bLumen ? 1 : 0); // 0 = none
	SetCVar(TEXT("r.ReflectionMethod"), bLumen ? 1 : 2);                 // 2 = screen-space
	SetCVar(TEXT("t.MaxFPS"), FpsCap);
	SetCVar(TEXT("r.VSync"), bVSync ? 1 : 0);
	// Set after the quality preset: game-setting priority beats the scalability value for the same cvar
	SetCVar(TEXT("r.ScreenPercentage"), FMath::Clamp(ResolutionScale, 50, 100));

	// Window mode is left alone in the editor, where PIE shares the editor's window
	if (!GIsEditor && GEngine)
	{
		UGameUserSettings* GUS = GEngine->GetGameUserSettings();
		const EWindowMode::Type Wanted = bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
		if (GUS && GUS->GetFullscreenMode() != Wanted)
		{
			GUS->SetFullscreenMode(Wanted);
			GUS->ApplySettings(false);
		}
	}

	if (!IFMODStudioModule::IsAvailable())
	{
		return;
	}
	if (FMOD::Studio::System* System = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime))
	{
		// Buses missing from the banks are skipped
		const TPair<const char*, float> Buses[] = {{"bus:/", MasterVolume}, {"bus:/SFX", SfxVolume}, {"bus:/Music", MusicVolume}};
		for (const TPair<const char*, float>& B : Buses)
		{
			FMOD::Studio::Bus* Bus = nullptr;
			if (System->getBus(B.Key, &Bus) == FMOD_OK)
			{
				Bus->setVolume(B.Value);
			}
		}
	}
}

void UDCSettings::Save()
{
	SaveConfig();
	Apply();
}
