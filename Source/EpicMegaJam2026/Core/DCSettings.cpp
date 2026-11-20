#include "Core/DCSettings.h"
#include "FMODStudioModule.h"
#include "fmod_studio.hpp"
#include "Scalability.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "DirectorsCut"

UDCSettings::FOnDCLanguageChanged UDCSettings::OnLanguageChanged;

FKey& UDCSettings::Key(EDCAction Action)
{
	return Action == EDCAction::BackRoom ? BackRoom : Interact;
}

const TCHAR* UDCSettings::ActionLabel(EDCAction Action)
{
	static const TCHAR* Labels[] = {TEXT("Interact"), TEXT("Go to the back room")};
	return Labels[(int32)Action];
}

FText UDCSettings::ActionDisplayLabel(EDCAction Action)
{
	return Action == EDCAction::BackRoom
		? LOCTEXT("Settings_Action_BackRoom", "Go to the back room")
		: LOCTEXT("Settings_Action_Interact", "Interact");
}

FString UDCSettings::DetectDefaultLanguage()
{
	FString Locale = FPlatformMisc::GetDefaultLocale();
	if (Locale.IsEmpty())
	{
		Locale = FInternationalization::Get().GetDefaultLanguage()->GetName();
	}
	return Locale.StartsWith(TEXT("es"), ESearchCase::IgnoreCase) ? TEXT("es") : TEXT("en");
}

FText UDCSettings::LanguageDisplayName(const FString& Code)
{
	// Shown in each language's own name regardless of the active locale
	return Code == TEXT("es") ? FText::FromString(TEXT("ESPAÑOL")) : FText::FromString(TEXT("ENGLISH"));
}

FString UDCSettings::GetEffectiveLanguage() const
{
	if (Language == TEXT("es") || Language == TEXT("en"))
	{
		return Language;
	}
	return DetectDefaultLanguage();
}

void UDCSettings::RefreshWorldTextRenders()
{
	// World-space 3D TextRenderComponents cache their glyph mesh until SetText is called again
	for (TObjectIterator<UTextRenderComponent> It; It; ++It)
	{
		UTextRenderComponent* Comp = *It;
		if (IsValid(Comp) && Comp->GetWorld() && Comp->GetWorld()->IsGameWorld() && !Comp->Text.IsEmpty())
		{
			const FText Saved = Comp->Text;
			Comp->SetText(FText::GetEmpty());
			Comp->SetText(Saved);
		}
	}
}

void UDCSettings::HandleTextRevisionChanged()
{
	static bool bRefreshing = false;
	if (bRefreshing || IsEngineExitRequested())
	{
		return;
	}
	TGuardValue<bool> Guard(bRefreshing, true);
	RefreshWorldTextRenders();
	OnLanguageChanged.Broadcast();
}

void UDCSettings::ApplyLanguage() const
{
	const FString Target = GetEffectiveLanguage();
	FInternationalization& I18n = FInternationalization::Get();
	if (I18n.GetCurrentLanguage()->GetName() != Target)
	{
		I18n.SetCurrentCulture(Target);
	}
#if WITH_EDITOR
	if (GIsEditor)
	{
		// In editor/PIE builds UE keeps Game .locres unloaded until game localization preview is toggled
		if (Target == TEXT("en"))
		{
			if (FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled())
			{
				FTextLocalizationManager::Get().DisableGameLocalizationPreview();
			}
		}
		else
		{
			FTextLocalizationManager::Get().EnableGameLocalizationPreview(Target);
		}
	}
#endif
	RefreshWorldTextRenders();
}

void UDCSettings::SetLanguage(const FString& NewLanguage)
{
	const FString Normalized = NewLanguage == TEXT("es") ? TEXT("es") : TEXT("en");
	Language = Normalized;
	SaveConfig();
	ApplyLanguage();
	OnLanguageChanged.Broadcast();
}

void UDCSettings::Apply() const
{
	ApplyLanguage();
	// Graphics go through cvars so nothing is written to the engine's own scalability ini
	Scalability::FQualityLevels Levels;
	Levels.SetFromSingleQualityLevel(FMath::Clamp(Quality, 0, 3));
	Levels.ShadowQuality = FMath::Max(Levels.ShadowQuality, 1); // Low would turn shadows off
	if (!(Scalability::GetQualityLevels() == Levels))
	{
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

#if WITH_DEV_AUTOMATION_TESTS
#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Data/DCTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDCLocalizationRoundTripTest, "DirectorsCut.Localization.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FDCLocalizationRoundTripTest::RunTest(const FString& Parameters)
{
	UDCSettings* Settings = UDCSettings::Get();
	const FString SavedLanguage = Settings->Language;
	const FString SavedCulture = FInternationalization::Get().GetCurrentLanguage()->GetName();

	const FString Detected = UDCSettings::DetectDefaultLanguage();
	TestTrue(TEXT("DetectDefaultLanguage returns en or es"), Detected == TEXT("en") || Detected == TEXT("es"));
	TestEqual(TEXT("LanguageDisplayName en"), UDCSettings::LanguageDisplayName(TEXT("en")).ToString(), TEXT("ENGLISH"));
	TestEqual(TEXT("LanguageDisplayName es"), UDCSettings::LanguageDisplayName(TEXT("es")).ToString(), TEXT("ESPAÑOL"));

	auto WaitForLoc = [](const FText& TextToCheck, const TCHAR* Expected)
	{
		const double Deadline = FPlatformTime::Seconds() + 3.0;
		while (FPlatformTime::Seconds() < Deadline)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			if (TextToCheck.ToString() == Expected)
			{
				return true;
			}
			FPlatformProcess::Sleep(0.02f);
		}
		return TextToCheck.ToString() == Expected;
	};

	const FText LangTabText = LOCTEXT("Options_TabLanguage", "LANGUAGE");
	const FText GameOverText = LOCTEXT("EndScreen_GameOver", "Eviction notice");

	// Switch to Spanish
	Settings->Language = TEXT("es");
	Settings->ApplyLanguage();
	TestTrue(TEXT("Options_TabLanguage resolves to IDIOMA in es"), WaitForLoc(LangTabText, TEXT("IDIOMA")));
	TestEqual(TEXT("EndScreen_GameOver resolves in es"), GameOverText.ToString(), TEXT("Aviso de desalojo"));

	if (const UDataTable* Genres = LoadObject<UDataTable>(nullptr, TEXT("/Game/DirectorsCut/Data/DT_Genres.DT_Genres")))
	{
		if (const FGenreData* Comedy = Genres->FindRow<FGenreData>(TEXT("Comedy"), TEXT("LocTest")))
		{
			TestEqual(TEXT("DT_Genres Comedy DisplayName in es"), Comedy->DisplayName.ToString(), FString(TEXT("Comedia")));
		}
		else
		{
			AddError(TEXT("Comedy row missing from DT_Genres"));
		}
	}

	if (const UDataTable* Feedback = LoadObject<UDataTable>(nullptr, TEXT("/Game/DirectorsCut/Data/DT_FeedbackLines.DT_FeedbackLines")))
	{
		if (const FFeedbackLineData* Row = Feedback->FindRow<FFeedbackLineData>(TEXT("Funny_TooLittle_1"), TEXT("LocTest")))
		{
			TestEqual(TEXT("DT_FeedbackLines translated in es"), Row->Text.ToString(), FString(TEXT("Podría haber sido mucho más graciosa.")));
		}
	}

	if (const UDataTable* VHS = LoadObject<UDataTable>(nullptr, TEXT("/Game/DirectorsCut/Data/DT_VHS.DT_VHS")))
	{
		const TArray<FName> Names = VHS->GetRowNames();
		if (Names.Num() > 0)
		{
			if (const FVHSData* First = VHS->FindRow<FVHSData>(Names[0], TEXT("LocTest")))
			{
				TestEqual(TEXT("DT_VHS Title stays untranslated in es"), First->Title.ToString(), FString(TEXT("Wicked Dead II")));
			}
		}
	}

	// Switch back to English and verify live switch back
	Settings->Language = TEXT("en");
	Settings->ApplyLanguage();
	TestTrue(TEXT("Options_TabLanguage resolves back to LANGUAGE in en"), WaitForLoc(LangTabText, TEXT("LANGUAGE")));

	// Restore prior state cleanly without dirtying user config
	Settings->Language = SavedLanguage;
	if (FInternationalization::Get().GetCurrentLanguage()->GetName() != SavedCulture)
	{
		FInternationalization::Get().SetCurrentCulture(SavedCulture);
	}
#if WITH_EDITOR
	if (GIsEditor && FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled())
	{
		FTextLocalizationManager::Get().DisableGameLocalizationPreview();
	}
#endif
	return !HasAnyErrors();
}
#endif

#undef LOCTEXT_NAMESPACE
