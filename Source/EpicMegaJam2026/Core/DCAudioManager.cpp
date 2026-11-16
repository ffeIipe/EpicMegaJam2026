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
	using FAudioEventMember = FDCAudioCue UPDA_AudioConfig::*;

	struct FDefaultEventEntry
	{
		FAudioEventMember Member;
		const TCHAR* AssetPath;
		const TCHAR* FallbackAssetPath = nullptr;
	};

	const FDefaultEventEntry DefaultEventEntries[] = {
		// Ambience & Music
		{ &UPDA_AudioConfig::Music, TEXT("/Game/FMOD/Events/MUS/Music.Music") },
		{ &UPDA_AudioConfig::MainMenuMusic, TEXT("/Game/FMOD/Events/MUS/Music.Music") },
		{ &UPDA_AudioConfig::DayMusic, TEXT("/Game/FMOD/Events/MUS/Music.Music") },
		{ &UPDA_AudioConfig::NightMusic, TEXT("/Game/FMOD/Events/MUS/Music.Music") },
		{ &UPDA_AudioConfig::CounterAmbience, TEXT("/Game/FMOD/Events/AMB/CounterAmb.CounterAmb") },
		{ &UPDA_AudioConfig::WarehouseAmbience, TEXT("/Game/FMOD/Events/AMB/WarehouseAmb.WarehouseAmb") },
		{ &UPDA_AudioConfig::LightBuzz, TEXT("/Game/FMOD/Events/SFX/ENV/LightBuzz.LightBuzz") },
		{ &UPDA_AudioConfig::PauseOrDialogue, TEXT("/Game/FMOD/Snapshots/PauseOrDialogue.PauseOrDialogue"), TEXT("/Game/FMOD/Events/PauseOrDialogue.PauseOrDialogue") },
		// Environment & Doors / Interactables
		{ &UPDA_AudioConfig::DoorbellRing, TEXT("/Game/FMOD/Events/SFX/ENV/DoorbellRing.DoorbellRing") },
		{ &UPDA_AudioConfig::StoreDoorOpen, TEXT("/Game/FMOD/Events/SFX/ENV/StoreDoorOpen.StoreDoorOpen") },
		{ &UPDA_AudioConfig::StoreDoorClose, TEXT("/Game/FMOD/Events/SFX/ENV/StoreDoorClose.StoreDoorClose") },
		{ &UPDA_AudioConfig::WarehouseDoorOpen, TEXT("/Game/FMOD/Events/SFX/ENV/WarehouseDoorOpen.WarehouseDoorOpen") },
		{ &UPDA_AudioConfig::WarehouseDoorClose, TEXT("/Game/FMOD/Events/SFX/ENV/WarehouseDoorClose.WarehouseDoorClose") },
		{ &UPDA_AudioConfig::OpenSupplier, TEXT("/Game/FMOD/Events/SFX/INTERACTABLES/OpenSupplier.OpenSupplier") },
		{ &UPDA_AudioConfig::CloseSupplier, TEXT("/Game/FMOD/Events/SFX/INTERACTABLES/CloseSupplier.CloseSupplier") },
		{ &UPDA_AudioConfig::TurnOnComputer, TEXT("/Game/FMOD/Events/SFX/INTERACTABLES/TurnOnComputer.TurnOnComputer") },
		{ &UPDA_AudioConfig::TurnOffComputer, TEXT("/Game/FMOD/Events/SFX/INTERACTABLES/TurnOffComputer.TurnOffComputer") },
		// Clients
		{ &UPDA_AudioConfig::Footsteps, TEXT("/Game/FMOD/Events/SFX/Footsteps.Footsteps") },
		{ &UPDA_AudioConfig::FemaleCustomerTalk, TEXT("/Game/FMOD/Events/SFX/FemaleCustomerTalk.FemaleCustomerTalk") },
		{ &UPDA_AudioConfig::MaleCustomerTalk, TEXT("/Game/FMOD/Events/SFX/MaleCustomerTalk.MaleCustomerTalk") },
		{ &UPDA_AudioConfig::Delivered, TEXT("/Game/FMOD/Events/UI/SALES/SellFilm.SellFilm") },
		// Crafting
		{ &UPDA_AudioConfig::FilmPlace, TEXT("/Game/FMOD/Events/UI/FilmPlace.FilmPlace") },
		{ &UPDA_AudioConfig::FilmSelect, TEXT("/Game/FMOD/Events/UI/FilmSelect.FilmSelect") },
		// Economy & Computer
		{ &UPDA_AudioConfig::BuyFilm, TEXT("/Game/FMOD/Events/UI/SALES/BuyFilm.BuyFilm") },
		{ &UPDA_AudioConfig::SellFilm, TEXT("/Game/FMOD/Events/UI/SALES/SellFilm.SellFilm") },
		{ &UPDA_AudioConfig::TapePurchased, TEXT("/Game/FMOD/Events/UI/SALES/BuyFilm.BuyFilm") },
		{ &UPDA_AudioConfig::UnlockPurchased, TEXT("/Game/FMOD/Events/UI/SALES/BuyFilm.BuyFilm") },
		{ &UPDA_AudioConfig::ComputerNotification, TEXT("/Game/FMOD/Events/UI/Notification.Notification") },
		{ &UPDA_AudioConfig::GoodReview, TEXT("/Game/FMOD/Events/UI/GoodReview.GoodReview") },
		{ &UPDA_AudioConfig::BadReview, TEXT("/Game/FMOD/Events/UI/BadReview.BadReview") },
		{ &UPDA_AudioConfig::ReviewLoved, TEXT("/Game/FMOD/Events/UI/GoodReview.GoodReview") },
		{ &UPDA_AudioConfig::ReviewLiked, TEXT("/Game/FMOD/Events/UI/GoodReview.GoodReview") },
		{ &UPDA_AudioConfig::ReviewNeutral, TEXT("/Game/FMOD/Events/UI/BadReview.BadReview") },
		{ &UPDA_AudioConfig::ReviewHated, TEXT("/Game/FMOD/Events/UI/BadReview.BadReview") },
		// UI
		{ &UPDA_AudioConfig::MenuClick, TEXT("/Game/FMOD/Events/UI/MENU/MenuClick.MenuClick") },
		{ &UPDA_AudioConfig::MenuHover, TEXT("/Game/FMOD/Events/UI/MENU/MenuHover.MenuHover") },
		{ &UPDA_AudioConfig::Notification, TEXT("/Game/FMOD/Events/UI/Notification.Notification") },
	};

	FFMODEventInstance GPersistentMusicInstance;
	TWeakObjectPtr<UFMODEvent> GPersistentMusicEvent;
	float GPersistentMusicVolume = 1.f;

	void StopEventInstance(FFMODEventInstance& Inst)
	{
		if (UFMODBlueprintStatics::EventInstanceIsValid(Inst))
		{
			UFMODBlueprintStatics::EventInstanceStop(Inst, true);
		}
		Inst.Instance = nullptr;
	}

	UPDA_AudioConfig* LoadDefaultAudioConfigAsset()
	{
		if (UPDA_AudioConfig* Cfg = LoadObject<UPDA_AudioConfig>(nullptr, TEXT("/Game/DirectorsCut/Audio/DA_AudioConfig.DA_AudioConfig"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Cfg;
		}
		return LoadObject<UPDA_AudioConfig>(nullptr, TEXT("/Game/DirectorsCut/Data/DA_AudioConfig.DA_AudioConfig"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	void PopulateMissingDefaultEvents(UPDA_AudioConfig& Cfg)
	{
		for (const FDefaultEventEntry& Entry : DefaultEventEntries)
		{
			FDCAudioCue& Cue = Cfg.*Entry.Member;
			if (!Cue.IsValid())
			{
				UFMODEvent* Loaded = LoadObject<UFMODEvent>(nullptr, Entry.AssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (!Loaded && Entry.FallbackAssetPath)
				{
					Loaded = LoadObject<UFMODEvent>(nullptr, Entry.FallbackAssetPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
				}
				Cue.Event = Loaded;
			}
		}
	}

	const TMap<FName, FAudioEventMember>& GetCueLookup()
	{
		static const TMap<FName, FAudioEventMember> Map = []()
		{
			TMap<FName, FAudioEventMember> M;
			// Ambience & Music
			M.Add(TEXT("Music"), &UPDA_AudioConfig::Music);
			M.Add(TEXT("MainMenuMusic"), &UPDA_AudioConfig::MainMenuMusic);
			M.Add(TEXT("DayMusic"), &UPDA_AudioConfig::DayMusic);
			M.Add(TEXT("NightMusic"), &UPDA_AudioConfig::NightMusic);
			M.Add(TEXT("CounterAmbience"), &UPDA_AudioConfig::CounterAmbience);
			M.Add(TEXT("CounterAmb"), &UPDA_AudioConfig::CounterAmbience);
			M.Add(TEXT("WarehouseAmbience"), &UPDA_AudioConfig::WarehouseAmbience);
			M.Add(TEXT("WarehouseAmb"), &UPDA_AudioConfig::WarehouseAmbience);
			M.Add(TEXT("LightBuzz"), &UPDA_AudioConfig::LightBuzz);
			M.Add(TEXT("PauseOrDialogue"), &UPDA_AudioConfig::PauseOrDialogue);
			// Environment & Doors / Interactables
			M.Add(TEXT("DoorbellRing"), &UPDA_AudioConfig::DoorbellRing);
			M.Add(TEXT("StoreDoorOpen"), &UPDA_AudioConfig::StoreDoorOpen);
			M.Add(TEXT("StoreDoorClose"), &UPDA_AudioConfig::StoreDoorClose);
			M.Add(TEXT("WarehouseDoorOpen"), &UPDA_AudioConfig::WarehouseDoorOpen);
			M.Add(TEXT("WarehouseDoorClose"), &UPDA_AudioConfig::WarehouseDoorClose);
			M.Add(TEXT("OpenSupplier"), &UPDA_AudioConfig::OpenSupplier);
			M.Add(TEXT("CloseSupplier"), &UPDA_AudioConfig::CloseSupplier);
			M.Add(TEXT("TurnOnComputer"), &UPDA_AudioConfig::TurnOnComputer);
			M.Add(TEXT("TurnOffComputer"), &UPDA_AudioConfig::TurnOffComputer);
			// Clients
			M.Add(TEXT("Footsteps"), &UPDA_AudioConfig::Footsteps);
			M.Add(TEXT("FemaleCustomerTalk"), &UPDA_AudioConfig::FemaleCustomerTalk);
			M.Add(TEXT("MaleCustomerTalk"), &UPDA_AudioConfig::MaleCustomerTalk);
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
			M.Add(TEXT("BuyFilm"), &UPDA_AudioConfig::BuyFilm);
			M.Add(TEXT("SellFilm"), &UPDA_AudioConfig::SellFilm);
			M.Add(TEXT("TapePurchased"), &UPDA_AudioConfig::TapePurchased);
			M.Add(TEXT("UnlockPurchased"), &UPDA_AudioConfig::UnlockPurchased);
			M.Add(TEXT("ComputerNotification"), &UPDA_AudioConfig::ComputerNotification);
			M.Add(TEXT("RentCharged"), &UPDA_AudioConfig::RentCharged);
			M.Add(TEXT("GoodReview"), &UPDA_AudioConfig::GoodReview);
			M.Add(TEXT("BadReview"), &UPDA_AudioConfig::BadReview);
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
			M.Add(TEXT("Notification"), &UPDA_AudioConfig::Notification);
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

	const TMap<FName, FAudioEventMember>& GetFallbackCueLookup()
	{
		static const TMap<FName, FAudioEventMember> Map = []()
		{
			TMap<FName, FAudioEventMember> M;
			M.Add(TEXT("ReviewLoved"), &UPDA_AudioConfig::GoodReview);
			M.Add(TEXT("Review_Loved"), &UPDA_AudioConfig::GoodReview);
			M.Add(TEXT("ReviewLiked"), &UPDA_AudioConfig::GoodReview);
			M.Add(TEXT("Review_Liked"), &UPDA_AudioConfig::GoodReview);
			M.Add(TEXT("ReviewNeutral"), &UPDA_AudioConfig::BadReview);
			M.Add(TEXT("Review_Neutral"), &UPDA_AudioConfig::BadReview);
			M.Add(TEXT("ReviewHated"), &UPDA_AudioConfig::BadReview);
			M.Add(TEXT("Review_Hated"), &UPDA_AudioConfig::BadReview);
			M.Add(TEXT("TapePurchased"), &UPDA_AudioConfig::BuyFilm);
			M.Add(TEXT("UnlockPurchased"), &UPDA_AudioConfig::BuyFilm);
			M.Add(TEXT("Delivered"), &UPDA_AudioConfig::SellFilm);
			M.Add(TEXT("ComputerNotification"), &UPDA_AudioConfig::Notification);
			M.Add(TEXT("MainMenuMusic"), &UPDA_AudioConfig::Music);
			M.Add(TEXT("DayMusic"), &UPDA_AudioConfig::Music);
			M.Add(TEXT("NightMusic"), &UPDA_AudioConfig::Music);
			return M;
		}();
		return Map;
	}
}

UPDA_AudioConfig::UPDA_AudioConfig()
{
	PopulateMissingDefaultEvents(*this);
}

void UPDA_AudioConfig::PostLoad()
{
	Super::PostLoad();
	PopulateMissingDefaultEvents(*this);
}

const FDCAudioCue* UPDA_AudioConfig::FindCue(FName Name) const
{
	if (Name.IsNone())
	{
		return nullptr;
	}
	if (const FAudioEventMember* Member = GetCueLookup().Find(Name))
	{
		const FDCAudioCue& Cue = this->*(*Member);
		if (Cue.IsValid())
		{
			return &Cue;
		}
	}
	if (const FAudioEventMember* Fallback = GetFallbackCueLookup().Find(Name))
	{
		const FDCAudioCue& Cue = this->*(*Fallback);
		if (Cue.IsValid())
		{
			return &Cue;
		}
	}
	return CustomCues.Find(Name);
}

bool UPDA_AudioConfig::ResolveCueConfig(FName Name, FDCAudioCue& OutCue) const
{
	if (const FDCAudioCue* Found = FindCue(Name))
	{
		OutCue = *Found;
		return OutCue.IsValid();
	}
	OutCue = FDCAudioCue();
	return false;
}

UFMODEvent* UPDA_AudioConfig::ResolveCue(FName Name) const
{
	const FDCAudioCue* Found = FindCue(Name);
	return Found ? Found->Get() : nullptr;
}

ADCAudioManager::ADCAudioManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ADCAudioManager::BeginPlay()
{
	Super::BeginPlay();
	EnsureAudioConfig();
}

void ADCAudioManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllLoops();
	if (EndPlayReason == EEndPlayReason::Quit || EndPlayReason == EEndPlayReason::EndPlayInEditor)
	{
		StopPersistentMusic();
	}
	Super::EndPlay(EndPlayReason);
}

void ADCAudioManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MusicFadeState == EMusicFadeState::FadingOutForIntro)
	{
		AdvanceIntroFade(DeltaSeconds, 0.f, EMusicFadeState::MutedInIntro, true);
	}
	else if (MusicFadeState == EMusicFadeState::FadingInFromIntro)
	{
		AdvanceIntroFade(DeltaSeconds, 1.f, EMusicFadeState::None, false);
	}
}

void ADCAudioManager::EnsureAudioConfig()
{
	if (!AudioConfig)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const ADCGameMode* GM = World->GetAuthGameMode<ADCGameMode>())
			{
				if (GM->Config && GM->Config->AudioConfig)
				{
					AudioConfig = GM->Config->AudioConfig;
				}
			}
			else if (const ADCMainMenuGameMode* MenuGM = World->GetAuthGameMode<ADCMainMenuGameMode>())
			{
				if (MenuGM->Config && MenuGM->Config->AudioConfig)
				{
					AudioConfig = MenuGM->Config->AudioConfig;
				}
			}
		}
	}
	if (!AudioConfig)
	{
		AudioConfig = LoadDefaultAudioConfigAsset();
	}
	if (!AudioConfig)
	{
		AudioConfig = NewObject<UPDA_AudioConfig>(this);
	}
	if (AudioConfig)
	{
		PopulateMissingDefaultEvents(*AudioConfig);
	}
}

const FDCAudioCue* ADCAudioManager::ResolveCueStruct(FName Name) const
{
	return AudioConfig ? AudioConfig->FindCue(Name) : nullptr;
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
	PlayCueInternal(Name, false, FVector::ZeroVector, nullptr);
}

void ADCAudioManager::PlayCueWithParameters(FName Name, const TMap<FName, float>& Parameters)
{
	PlayCueInternal(Name, false, FVector::ZeroVector, &Parameters);
}

void ADCAudioManager::PlayCueAtLocation(FName Name, FVector Location)
{
	PlayCueInternal(Name, true, Location, nullptr);
}

void ADCAudioManager::PlayCueAtLocationWithParameters(FName Name, FVector Location, const TMap<FName, float>& Parameters)
{
	PlayCueInternal(Name, true, Location, &Parameters);
}

void ADCAudioManager::PlayCueInternal(FName Name, bool bAtLocation, FVector Location, const TMap<FName, float>* ExtraParameters)
{
	EnsureAudioConfig();
	const FDCAudioCue* CueCfg = ResolveCueStruct(Name);

	if (Name == TEXT("TurnOnComputer"))
	{
		StopEventInstance(ComputerInstance);
		if (UFMODEvent* Event = ResolveEvent(Name))
		{
			ComputerInstance = bAtLocation
				? UFMODBlueprintStatics::PlayEventAtLocation(this, Event, FTransform(Location), false)
				: UFMODBlueprintStatics::PlayEvent2D(this, Event, false);
			if (CueCfg)
			{
				ApplyParametersToInstance(ComputerInstance, CueCfg->Parameters);
			}
			if (ExtraParameters)
			{
				ApplyParametersToInstance(ComputerInstance, *ExtraParameters);
			}
			if (ComputerInstance.Instance)
			{
				ComputerInstance.Instance->start();
			}
		}
		return;
	}

	if (Name == TEXT("TurnOffComputer"))
	{
		StopEventInstance(ComputerInstance);
	}

	UFMODEvent* Event = ResolveEvent(Name);
	if (!Event)
	{
		return;
	}

	const bool bHasParams = (CueCfg && !CueCfg->Parameters.IsEmpty()) || (ExtraParameters && !ExtraParameters->IsEmpty());
	if (!bHasParams)
	{
		if (bAtLocation)
		{
			UFMODBlueprintStatics::PlayEventAtLocation(this, Event, FTransform(Location), true);
		}
		else
		{
			UFMODBlueprintStatics::PlayEvent2D(this, Event, true);
		}
		return;
	}

	FFMODEventInstance Inst = bAtLocation
		? UFMODBlueprintStatics::PlayEventAtLocation(this, Event, FTransform(Location), false)
		: UFMODBlueprintStatics::PlayEvent2D(this, Event, false);
	if (CueCfg)
	{
		ApplyParametersToInstance(Inst, CueCfg->Parameters);
	}
	if (ExtraParameters)
	{
		ApplyParametersToInstance(Inst, *ExtraParameters);
	}
	if (Inst.Instance)
	{
		Inst.Instance->start();
		Inst.Instance->release();
	}
}

void ADCAudioManager::ApplyParametersToInstance(FFMODEventInstance Instance, const TMap<FName, float>& Parameters)
{
	if (Parameters.IsEmpty())
	{
		return;
	}

	FMOD::Studio::EventDescription* EventDesc = nullptr;
	if (Instance.Instance)
	{
		Instance.Instance->getDescription(&EventDesc);
	}

	for (const TPair<FName, float>& Pair : Parameters)
	{
		if (Pair.Key.IsNone())
		{
			continue;
		}
		const FTCHARToUTF8 Utf8Name(*Pair.Key.ToString());
		const char* ParamName = Utf8Name.Get();

		bool bHandledAsLocal = false;
		if (Instance.Instance && EventDesc)
		{
			FMOD_STUDIO_PARAMETER_DESCRIPTION ParamDesc;
			if (EventDesc->getParameterDescriptionByName(ParamName, &ParamDesc) == FMOD_OK
				&& (ParamDesc.flags & FMOD_STUDIO_PARAMETER_GLOBAL) == 0)
			{
				Instance.Instance->setParameterByName(ParamName, Pair.Value);
				bHandledAsLocal = true;
			}
		}

		if (!bHandledAsLocal)
		{
			SetGlobalParameterStatic(ParamName, Pair.Value);
		}
	}
}

void ADCAudioManager::SetGlobalParameterStatic(const char* Name, float Value)
{
	if (!Name || !Name[0] || !IFMODStudioModule::IsAvailable())
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

void ADCAudioManager::SetGlobalParameter(FName ParameterName, float Value)
{
	if (!ParameterName.IsNone())
	{
		const FTCHARToUTF8 Utf8Name(*ParameterName.ToString());
		SetGlobalParameterStatic(Utf8Name.Get(), Value);
	}
}

void ADCAudioManager::SetCueParameter(FName CueName, FName ParameterName, float Value)
{
	if (CueName.IsNone() || ParameterName.IsNone())
	{
		return;
	}
	EnsureAudioConfig();
	TMap<FName, float> SingleParam;
	SingleParam.Add(ParameterName, Value);

	bool bAppliedToInstance = false;
	if (UFMODEvent* Event = ResolveEvent(CueName))
	{
		if (Event == GPersistentMusicEvent.Get() && UFMODBlueprintStatics::EventInstanceIsValid(GPersistentMusicInstance))
		{
			ApplyParametersToInstance(GPersistentMusicInstance, SingleParam);
			bAppliedToInstance = true;
		}
		const TArray<FFMODEventInstance> ActiveInstances = UFMODBlueprintStatics::FindEventInstances(this, Event);
		for (const FFMODEventInstance& Inst : ActiveInstances)
		{
			ApplyParametersToInstance(Inst, SingleParam);
			bAppliedToInstance = true;
		}
	}

	if (!bAppliedToInstance)
	{
		const FTCHARToUTF8 Utf8Name(*ParameterName.ToString());
		SetGlobalParameterStatic(Utf8Name.Get(), Value);
	}
}

void ADCAudioManager::StartOrSwitchLoop(FFMODEventInstance& Instance, TWeakObjectPtr<UFMODEvent>& CurrentEvent, const FDCAudioCue* TargetCue)
{
	UFMODEvent* TargetEvent = TargetCue ? TargetCue->Get() : nullptr;
	if (CurrentEvent.Get() == TargetEvent && UFMODBlueprintStatics::EventInstanceIsValid(Instance))
	{
		if (TargetCue)
		{
			ApplyParametersToInstance(Instance, TargetCue->Parameters);
		}
		return;
	}
	StopEventInstance(Instance);
	CurrentEvent.Reset();

	if (TargetEvent)
	{
		Instance = UFMODBlueprintStatics::PlayEvent2D(this, TargetEvent, false);
		if (TargetCue)
		{
			ApplyParametersToInstance(Instance, TargetCue->Parameters);
		}
		if (Instance.Instance)
		{
			Instance.Instance->start();
			CurrentEvent = TargetEvent;
		}
	}
}

void ADCAudioManager::EnsurePersistentMusicStarted(const UObject* WorldContextObject, UPDA_AudioConfig* OptionalConfig)
{
	UPDA_AudioConfig* Cfg = OptionalConfig;
	if (!Cfg && WorldContextObject)
	{
		if (const ADCAudioManager* ExistingMgr = Get(WorldContextObject))
		{
			Cfg = ExistingMgr->AudioConfig;
		}
	}
	if (!Cfg)
	{
		Cfg = LoadDefaultAudioConfigAsset();
	}

	if (UFMODBlueprintStatics::EventInstanceIsValid(GPersistentMusicInstance))
	{
		if (Cfg)
		{
			ApplyParametersToInstance(GPersistentMusicInstance, Cfg->Music.Parameters);
		}
		return;
	}

	UFMODEvent* MusicEvent = Cfg ? Cfg->Music.Get() : nullptr;
	if (!MusicEvent)
	{
		MusicEvent = LoadObject<UFMODEvent>(nullptr, TEXT("/Game/FMOD/Events/MUS/Music.Music"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
	if (!MusicEvent)
	{
		return;
	}

	GPersistentMusicInstance = UFMODBlueprintStatics::PlayEvent2D(const_cast<UObject*>(WorldContextObject), MusicEvent, false);
	if (Cfg)
	{
		ApplyParametersToInstance(GPersistentMusicInstance, Cfg->Music.Parameters);
	}
	if (GPersistentMusicInstance.Instance)
	{
		GPersistentMusicInstance.Instance->start();
		GPersistentMusicEvent = MusicEvent;
		SetPersistentMusicVolume(GPersistentMusicVolume);
	}
}

void ADCAudioManager::StopPersistentMusic()
{
	StopEventInstance(GPersistentMusicInstance);
	GPersistentMusicEvent.Reset();
	GPersistentMusicVolume = 1.f;
}

void ADCAudioManager::SetPersistentMusicVolume(float Volume)
{
	GPersistentMusicVolume = FMath::Clamp(Volume, 0.f, 1.f);
	if (UFMODBlueprintStatics::EventInstanceIsValid(GPersistentMusicInstance))
	{
		UFMODBlueprintStatics::EventInstanceSetVolume(GPersistentMusicInstance, GPersistentMusicVolume);
	}
}

void ADCAudioManager::BeginIntroMusicFadeOut(float DurationSec)
{
	EnsureAudioConfig();
	EnsurePersistentMusicStarted(this, AudioConfig);
	if (DurationSec <= KINDA_SMALL_NUMBER)
	{
		SetPersistentMusicVolume(0.f);
		SetGlobalParameterStatic("Location", 0.f);
		SetGlobalParameterStatic("Day-Night", 0.f);
		MusicFadeState = EMusicFadeState::MutedInIntro;
		return;
	}
	MusicFadeState = EMusicFadeState::FadingOutForIntro;
	MusicFadeStartVolume = GPersistentMusicVolume;
	MusicFadeElapsed = 0.f;
	MusicFadeDuration = DurationSec;
}

void ADCAudioManager::EndIntroMusicFadeIn(float DurationSec)
{
	EnsureAudioConfig();
	EnsurePersistentMusicStarted(this, AudioConfig);
	SetGlobalParameterStatic("Location", bCurrentBackRoom ? 1.f : 0.f);
	SetGlobalParameterStatic("Day-Night", CurrentPhase == EPhase::Night ? 1.f : 0.f);
	if (DurationSec <= KINDA_SMALL_NUMBER)
	{
		SetPersistentMusicVolume(1.f);
		MusicFadeState = EMusicFadeState::None;
		return;
	}
	MusicFadeState = EMusicFadeState::FadingInFromIntro;
	MusicFadeStartVolume = GPersistentMusicVolume;
	MusicFadeElapsed = 0.f;
	MusicFadeDuration = DurationSec;
}

void ADCAudioManager::AdvanceIntroFade(float DeltaSeconds, float TargetVolume, EMusicFadeState CompletedState, bool bSwitchToCounterOnFinish)
{
	MusicFadeElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(MusicFadeElapsed / FMath::Max(MusicFadeDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	SetPersistentMusicVolume(FMath::Lerp(MusicFadeStartVolume, TargetVolume, Alpha));
	if (Alpha >= 1.f)
	{
		SetPersistentMusicVolume(TargetVolume);
		if (bSwitchToCounterOnFinish)
		{
			SetGlobalParameterStatic("Location", 0.f);
			SetGlobalParameterStatic("Day-Night", 0.f);
		}
		MusicFadeState = CompletedState;
	}
}

void ADCAudioManager::SetPauseOrDialogueActive(bool bActive)
{
	EnsureAudioConfig();
	if (bActive)
	{
		if (!UFMODBlueprintStatics::EventInstanceIsValid(PauseOrDialogueInstance))
		{
			if (UFMODEvent* SnapshotEvent = ResolveEvent(TEXT("PauseOrDialogue")))
			{
				PauseOrDialogueInstance = UFMODBlueprintStatics::PlayEvent2D(this, SnapshotEvent, false);
				if (const FDCAudioCue* CueCfg = ResolveCueStruct(TEXT("PauseOrDialogue")))
				{
					ApplyParametersToInstance(PauseOrDialogueInstance, CueCfg->Parameters);
				}
				if (PauseOrDialogueInstance.Instance)
				{
					PauseOrDialogueInstance.Instance->start();
				}
			}
		}
	}
	else
	{
		StopEventInstance(PauseOrDialogueInstance);
	}
}

void ADCAudioManager::SetZoneAmbience(ECameraZone Zone)
{
	EnsureAudioConfig();
	bCurrentBackRoom = (Zone == ECameraZone::BackRoom || Zone == ECameraZone::ToBackRoom);
	const FDCAudioCue* ZoneCue = AudioConfig ? (bCurrentBackRoom ? &AudioConfig->WarehouseAmbience : &AudioConfig->CounterAmbience) : nullptr;
	StartOrSwitchLoop(AmbienceInstance, CurrentAmbienceEvent, ZoneCue);
	const FDCAudioCue* BuzzCue = AudioConfig ? &AudioConfig->LightBuzz : nullptr;
	StartOrSwitchLoop(LightBuzzInstance, CurrentLightBuzzEvent, BuzzCue);
	EnsurePersistentMusicStarted(this, AudioConfig);
	SetGlobalParameterStatic("Location", bCurrentBackRoom ? 1.f : 0.f);
}

void ADCAudioManager::SetPhaseMusic(EPhase Phase)
{
	EnsureAudioConfig();
	CurrentPhase = Phase;
	EnsurePersistentMusicStarted(this, AudioConfig);
	if (AudioConfig && UFMODBlueprintStatics::EventInstanceIsValid(GPersistentMusicInstance))
	{
		const FDCAudioCue& PhaseCue = (Phase == EPhase::Night) ? AudioConfig->NightMusic : AudioConfig->DayMusic;
		ApplyParametersToInstance(GPersistentMusicInstance, PhaseCue.Parameters);
	}
	SetGlobalParameterStatic("Day-Night", Phase == EPhase::Night ? 1.f : 0.f);
}

void ADCAudioManager::StartMainMenuAudio()
{
	EnsureAudioConfig();
	MusicFadeState = EMusicFadeState::None;
	SetGlobalParameterStatic("Location", 2.f);
	EnsurePersistentMusicStarted(this, AudioConfig);
	SetPersistentMusicVolume(1.f);
	if (AudioConfig && UFMODBlueprintStatics::EventInstanceIsValid(GPersistentMusicInstance))
	{
		ApplyParametersToInstance(GPersistentMusicInstance, AudioConfig->MainMenuMusic.Parameters);
	}
	SetGlobalParameterStatic("Location", 2.f);
}

void ADCAudioManager::StopAllLoops()
{
	StopEventInstance(AmbienceInstance);
	StopEventInstance(LightBuzzInstance);
	StopEventInstance(ComputerInstance);
	StopEventInstance(PauseOrDialogueInstance);
	CurrentAmbienceEvent.Reset();
	CurrentLightBuzzEvent.Reset();
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
