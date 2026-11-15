#include "Core/DCAudioManager.h"
#include "Core/DCGameMode.h"
#include "Core/DCMainMenuMode.h"
#include "Data/PDA_AudioConfig.h"
#include "Data/PDA_GameConfig.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "FMODEvent.h"
#include "FMODStudioModule.h"
#include "fmod_studio.hpp"
#include "Kismet/GameplayStatics.h"

namespace
{
	using FAudioEventMember = TObjectPtr<UFMODEvent> UPDA_AudioConfig::*;

	struct FDefaultEventEntry
	{
		FAudioEventMember Member;
		const TCHAR* AssetPath;
	};

	const FDefaultEventEntry DefaultEventEntries[] = {
		{ &UPDA_AudioConfig::CounterAmbience, TEXT("/Game/FMOD/Events/AMB/CounterAmb.CounterAmb") },
		{ &UPDA_AudioConfig::WarehouseAmbience, TEXT("/Game/FMOD/Events/AMB/WarehouseAmb.WarehouseAmb") },
		{ &UPDA_AudioConfig::Footsteps, TEXT("/Game/FMOD/Events/SFX/Footsteps.Footsteps") },
		{ &UPDA_AudioConfig::DoorbellRing, TEXT("/Game/FMOD/Events/SFX/ENV/DoorbellRing.DoorbellRing") },
		{ &UPDA_AudioConfig::LightBuzz, TEXT("/Game/FMOD/Events/SFX/ENV/LightBuzz.LightBuzz") },
		{ &UPDA_AudioConfig::StoreDoorClose, TEXT("/Game/FMOD/Events/SFX/ENV/StoreDoorClose.StoreDoorClose") },
		{ &UPDA_AudioConfig::StoreDoorOpen, TEXT("/Game/FMOD/Events/SFX/ENV/StoreDoorOpen.StoreDoorOpen") },
		{ &UPDA_AudioConfig::WarehouseDoorClose, TEXT("/Game/FMOD/Events/SFX/ENV/WarehouseDoorClose.WarehouseDoorClose") },
		{ &UPDA_AudioConfig::WarehouseDoorOpen, TEXT("/Game/FMOD/Events/SFX/ENV/WarehouseDoorOpen.WarehouseDoorOpen") },
		{ &UPDA_AudioConfig::FilmPlace, TEXT("/Game/FMOD/Events/UI/FilmPlace.FilmPlace") },
		{ &UPDA_AudioConfig::FilmSelect, TEXT("/Game/FMOD/Events/UI/FilmSelect.FilmSelect") },
		{ &UPDA_AudioConfig::MenuClick, TEXT("/Game/FMOD/Events/UI/MENU/MenuClick.MenuClick") },
		{ &UPDA_AudioConfig::MenuHover, TEXT("/Game/FMOD/Events/UI/MENU/MenuHover.MenuHover") },
	};

	void PopulateMissingDefaultEvents(UPDA_AudioConfig& Cfg)
	{
		for (const FDefaultEventEntry& Entry : DefaultEventEntries)
		{
			if (!(Cfg.*Entry.Member))
			{
				Cfg.*Entry.Member = LoadObject<UFMODEvent>(nullptr, Entry.AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
			}
		}
	}

	const TMap<FName, FAudioEventMember>& GetCueLookup()
	{
		static const TMap<FName, FAudioEventMember> Map = []()
		{
			TMap<FName, FAudioEventMember> M;
			// Ambience & Music
			M.Add(TEXT("MainMenuMusic"), &UPDA_AudioConfig::MainMenuMusic);
			M.Add(TEXT("DayMusic"), &UPDA_AudioConfig::DayMusic);
			M.Add(TEXT("NightMusic"), &UPDA_AudioConfig::NightMusic);
			M.Add(TEXT("CounterAmbience"), &UPDA_AudioConfig::CounterAmbience);
			M.Add(TEXT("CounterAmb"), &UPDA_AudioConfig::CounterAmbience);
			M.Add(TEXT("WarehouseAmbience"), &UPDA_AudioConfig::WarehouseAmbience);
			M.Add(TEXT("WarehouseAmb"), &UPDA_AudioConfig::WarehouseAmbience);
			M.Add(TEXT("LightBuzz"), &UPDA_AudioConfig::LightBuzz);
			// Environment & Doors
			M.Add(TEXT("DoorbellRing"), &UPDA_AudioConfig::DoorbellRing);
			M.Add(TEXT("StoreDoorOpen"), &UPDA_AudioConfig::StoreDoorOpen);
			M.Add(TEXT("StoreDoorClose"), &UPDA_AudioConfig::StoreDoorClose);
			M.Add(TEXT("WarehouseDoorOpen"), &UPDA_AudioConfig::WarehouseDoorOpen);
			M.Add(TEXT("WarehouseDoorClose"), &UPDA_AudioConfig::WarehouseDoorClose);
			// Clients
			M.Add(TEXT("Footsteps"), &UPDA_AudioConfig::Footsteps);
			M.Add(TEXT("ClientAtCounter"), &UPDA_AudioConfig::ClientAtCounter);
			M.Add(TEXT("DialogueOpen"), &UPDA_AudioConfig::DialogueOpen);
			M.Add(TEXT("OrderAccepted"), &UPDA_AudioConfig::OrderAccepted);
			M.Add(TEXT("OrderRejected"), &UPDA_AudioConfig::OrderRejected);
			M.Add(TEXT("Delivered"), &UPDA_AudioConfig::Delivered);
			M.Add(TEXT("ClientAngryLeave"), &UPDA_AudioConfig::ClientAngryLeave);
			// Crafting
			M.Add(TEXT("FilmSelect"), &UPDA_AudioConfig::FilmSelect);
			M.Add(TEXT("FilmPlace"), &UPDA_AudioConfig::FilmPlace);
			M.Add(TEXT("FilmRemove"), &UPDA_AudioConfig::FilmRemove);
			M.Add(TEXT("FilmCreated"), &UPDA_AudioConfig::FilmCreated);
			M.Add(TEXT("FilmDiscard"), &UPDA_AudioConfig::FilmDiscard);
			M.Add(TEXT("SlotFull"), &UPDA_AudioConfig::SlotFull);
			// Economy & Computer
			M.Add(TEXT("MoneyUp"), &UPDA_AudioConfig::MoneyUp);
			M.Add(TEXT("MoneyDown"), &UPDA_AudioConfig::MoneyDown);
			M.Add(TEXT("TapePurchased"), &UPDA_AudioConfig::TapePurchased);
			M.Add(TEXT("UnlockPurchased"), &UPDA_AudioConfig::UnlockPurchased);
			M.Add(TEXT("ComputerNotification"), &UPDA_AudioConfig::ComputerNotification);
			M.Add(TEXT("RentCharged"), &UPDA_AudioConfig::RentCharged);
			M.Add(TEXT("ReviewLoved"), &UPDA_AudioConfig::ReviewLoved);
			M.Add(TEXT("Review_Loved"), &UPDA_AudioConfig::ReviewLoved);
			M.Add(TEXT("ReviewLiked"), &UPDA_AudioConfig::ReviewLiked);
			M.Add(TEXT("Review_Liked"), &UPDA_AudioConfig::ReviewLiked);
			M.Add(TEXT("ReviewNeutral"), &UPDA_AudioConfig::ReviewNeutral);
			M.Add(TEXT("Review_Neutral"), &UPDA_AudioConfig::ReviewNeutral);
			M.Add(TEXT("ReviewHated"), &UPDA_AudioConfig::ReviewHated);
			M.Add(TEXT("Review_Hated"), &UPDA_AudioConfig::ReviewHated);
			// UI & Flow
			M.Add(TEXT("MenuClick"), &UPDA_AudioConfig::MenuClick);
			M.Add(TEXT("Click"), &UPDA_AudioConfig::MenuClick);
			M.Add(TEXT("MenuHover"), &UPDA_AudioConfig::MenuHover);
			M.Add(TEXT("Hover"), &UPDA_AudioConfig::MenuHover);
			M.Add(TEXT("TabSwitch"), &UPDA_AudioConfig::TabSwitch);
			M.Add(TEXT("PauseOpen"), &UPDA_AudioConfig::PauseOpen);
			M.Add(TEXT("ScreenClose"), &UPDA_AudioConfig::ScreenClose);
			M.Add(TEXT("MainMenuStart"), &UPDA_AudioConfig::MainMenuStart);
			M.Add(TEXT("IntroStart"), &UPDA_AudioConfig::IntroStart);
			M.Add(TEXT("IntroSkip"), &UPDA_AudioConfig::IntroSkip);
			M.Add(TEXT("DayStart"), &UPDA_AudioConfig::DayStart);
			M.Add(TEXT("NightStart"), &UPDA_AudioConfig::NightStart);
			M.Add(TEXT("DaySummaryOpen"), &UPDA_AudioConfig::DaySummaryOpen);
			M.Add(TEXT("OpenStore"), &UPDA_AudioConfig::OpenStore);
			M.Add(TEXT("TutorialStep"), &UPDA_AudioConfig::TutorialStep);
			M.Add(TEXT("TutorialComplete"), &UPDA_AudioConfig::TutorialComplete);
			M.Add(TEXT("GameOver"), &UPDA_AudioConfig::GameOver);
			M.Add(TEXT("Victory"), &UPDA_AudioConfig::Victory);
			return M;
		}();
		return Map;
	}
}

UPDA_AudioConfig::UPDA_AudioConfig()
{
	PopulateMissingDefaultEvents(*this);
}

UFMODEvent* UPDA_AudioConfig::ResolveCue(FName Name) const
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (const FAudioEventMember* Member = GetCueLookup().Find(Name))
	{
		if (UFMODEvent* Event = (this->*(*Member)).Get())
		{
			return Event;
		}
	}
	return CustomCues.FindRef(Name);
}

void ADCAudioManager::BeginPlay()
{
	Super::BeginPlay();
	EnsureAudioConfig();
}

void ADCAudioManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllLoops();
	Super::EndPlay(EndPlayReason);
}

void ADCAudioManager::EnsureAudioConfig()
{
	if (AudioConfig)
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		if (const ADCGameMode* GM = World->GetAuthGameMode<ADCGameMode>())
		{
			if (GM->Config && GM->Config->AudioConfig)
			{
				AudioConfig = GM->Config->AudioConfig;
				return;
			}
		}
		else if (const ADCMainMenuGameMode* MenuGM = World->GetAuthGameMode<ADCMainMenuGameMode>())
		{
			if (MenuGM->Config && MenuGM->Config->AudioConfig)
			{
				AudioConfig = MenuGM->Config->AudioConfig;
				return;
			}
		}
	}
	AudioConfig = LoadObject<UPDA_AudioConfig>(nullptr, TEXT("/Game/DirectorsCut/Audio/DA_AudioConfig.DA_AudioConfig"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!AudioConfig)
	{
		AudioConfig = LoadObject<UPDA_AudioConfig>(nullptr, TEXT("/Game/DirectorsCut/Data/DA_AudioConfig.DA_AudioConfig"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!AudioConfig)
	{
		AudioConfig = NewObject<UPDA_AudioConfig>(this);
		if (AudioConfig)
		{
			PopulateMissingDefaultEvents(*AudioConfig);
		}
	}
}

UFMODEvent* ADCAudioManager::ResolveEvent(FName Name) const
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (const TObjectPtr<UFMODEvent>* Override = Cues.Find(Name))
	{
		if (Override->Get())
		{
			return Override->Get();
		}
	}
	return AudioConfig ? AudioConfig->ResolveCue(Name) : nullptr;
}

void ADCAudioManager::PlayCue_Implementation(FName Name)
{
	EnsureAudioConfig();
	if (UFMODEvent* Event = ResolveEvent(Name))
	{
		UFMODBlueprintStatics::PlayEvent2D(this, Event, true);
	}
}

void ADCAudioManager::PlayCueAtLocation(FName Name, FVector Location)
{
	EnsureAudioConfig();
	if (UFMODEvent* Event = ResolveEvent(Name))
	{
		UFMODBlueprintStatics::PlayEventAtLocation(this, Event, FTransform(Location), true);
	}
}

void ADCAudioManager::StartOrSwitchLoop(FFMODEventInstance& Instance, TWeakObjectPtr<UFMODEvent>& CurrentEvent, UFMODEvent* TargetEvent)
{
	if (CurrentEvent.Get() == TargetEvent && UFMODBlueprintStatics::EventInstanceIsValid(Instance))
	{
		return;
	}
	if (UFMODBlueprintStatics::EventInstanceIsValid(Instance))
	{
		UFMODBlueprintStatics::EventInstanceStop(Instance, true);
	}
	Instance.Instance = nullptr;
	CurrentEvent.Reset();

	if (TargetEvent)
	{
		Instance = UFMODBlueprintStatics::PlayEvent2D(this, TargetEvent, false);
		if (Instance.Instance)
		{
			Instance.Instance->start();
			CurrentEvent = TargetEvent;
		}
	}
}

void ADCAudioManager::SetOptionalGlobalParameter(const char* Name, float Value) const
{
	if (!IFMODStudioModule::IsAvailable())
	{
		return;
	}
	if (FMOD::Studio::System* System = IFMODStudioModule::Get().GetStudioSystem(EFMODSystemContext::Runtime))
	{
		FMOD_STUDIO_PARAMETER_DESCRIPTION Desc;
		if (System->getParameterDescriptionByName(Name, &Desc) == FMOD_OK)
		{
			System->setParameterByName(Name, Value);
		}
	}
}

void ADCAudioManager::SetZoneAmbience(ECameraZone Zone)
{
	EnsureAudioConfig();
	const bool bBackRoom = (Zone == ECameraZone::BackRoom || Zone == ECameraZone::ToBackRoom);
	UFMODEvent* ZoneEvent = AudioConfig ? (bBackRoom ? AudioConfig->WarehouseAmbience.Get() : AudioConfig->CounterAmbience.Get()) : nullptr;
	StartOrSwitchLoop(AmbienceInstance, CurrentAmbienceEvent, ZoneEvent);
	UFMODEvent* BuzzEvent = AudioConfig ? AudioConfig->LightBuzz.Get() : nullptr;
	StartOrSwitchLoop(LightBuzzInstance, CurrentLightBuzzEvent, BuzzEvent);
	SetOptionalGlobalParameter("CameraZone", bBackRoom ? 1.f : 0.f);
}

void ADCAudioManager::SetPhaseMusic(EPhase Phase)
{
	EnsureAudioConfig();
	UFMODEvent* PhaseEvent = AudioConfig ? (Phase == EPhase::Night ? AudioConfig->NightMusic.Get() : AudioConfig->DayMusic.Get()) : nullptr;
	StartOrSwitchLoop(MusicInstance, CurrentMusicEvent, PhaseEvent);
	SetOptionalGlobalParameter("Phase", Phase == EPhase::Night ? 1.f : 0.f);
}

void ADCAudioManager::StartMainMenuAudio()
{
	EnsureAudioConfig();
	UFMODEvent* MenuMusic = AudioConfig ? AudioConfig->MainMenuMusic.Get() : nullptr;
	StartOrSwitchLoop(MusicInstance, CurrentMusicEvent, MenuMusic);
}

void ADCAudioManager::StopAllLoops()
{
	auto StopLoop = [](FFMODEventInstance& Inst)
	{
		if (UFMODBlueprintStatics::EventInstanceIsValid(Inst))
		{
			UFMODBlueprintStatics::EventInstanceStop(Inst, true);
		}
		Inst.Instance = nullptr;
	};
	StopLoop(AmbienceInstance);
	StopLoop(LightBuzzInstance);
	StopLoop(MusicInstance);
	CurrentAmbienceEvent.Reset();
	CurrentLightBuzzEvent.Reset();
	CurrentMusicEvent.Reset();
}

ADCAudioManager* ADCAudioManager::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (const ADCGameMode* GM = World->GetAuthGameMode<ADCGameMode>())
	{
		return GM->AudioManager;
	}
	if (const ADCMainMenuGameMode* MenuGM = World->GetAuthGameMode<ADCMainMenuGameMode>())
	{
		return MenuGM->AudioManager;
	}
	return Cast<ADCAudioManager>(UGameplayStatics::GetActorOfClass(World, ADCAudioManager::StaticClass()));
}

void ADCAudioManager::PlayCueStatic(const UObject* WorldContextObject, FName Name)
{
	if (ADCAudioManager* Mgr = Get(WorldContextObject))
	{
		Mgr->PlayCue(Name);
	}
}

void ADCAudioManager::PlayCueAtLocationStatic(const UObject* WorldContextObject, FName Name, FVector Location)
{
	if (ADCAudioManager* Mgr = Get(WorldContextObject))
	{
		Mgr->PlayCueAtLocation(Name, Location);
	}
}
