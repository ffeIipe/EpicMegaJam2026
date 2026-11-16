#include "Core/DCGameInstance.h"
#include "Core/DCAudioManager.h"
#include "Core/DCGameMode.h"
#include "Core/DCSettings.h"
#include "Components/DCComponents.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Kismet/GameplayStatics.h"

void UDCGameInstance::Init()
{
	Super::Init();
#if WITH_EDITOR
	if (GIsEditor)
	{
		EditorCultureToRestore = FInternationalization::Get().GetCurrentLanguage()->GetName();
	}
#endif
	TextRevisionHandle = FTextLocalizationManager::Get().OnTextRevisionChangedEvent.AddStatic(&UDCSettings::HandleTextRevisionChanged);
	UDCSettings::Get()->ApplyLanguage();
	ADCAudioManager::SetGlobalParameterStatic("Location", 2.f);
	ADCAudioManager::SetGlobalParameterStatic("Day-Night", 0.f);
	ADCAudioManager::EnsurePersistentMusicStarted(this);
}

void UDCGameInstance::Shutdown()
{
	ADCAudioManager::StopPersistentMusic();
	FTextLocalizationManager::Get().OnTextRevisionChangedEvent.Remove(TextRevisionHandle);
	TextRevisionHandle.Reset();
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (FTextLocalizationManager::Get().IsGameLocalizationPreviewEnabled())
		{
			FTextLocalizationManager::Get().DisableGameLocalizationPreview();
		}
		if (!EditorCultureToRestore.IsEmpty() && FInternationalization::Get().GetCurrentLanguage()->GetName() != EditorCultureToRestore)
		{
			FInternationalization::Get().SetCurrentCulture(EditorCultureToRestore);
		}
	}
#endif
	Super::Shutdown();
}

FString UDCGameInstance::SlotNameForIndex(int32 SlotIndex)
{
	return FString::Printf(TEXT("Slot%d"), SlotIndex + 1);
}

TArray<FDCSaveSlotSummary> UDCGameInstance::GetSaveSlots() const
{
	TArray<FDCSaveSlotSummary> Slots;
	Slots.Reserve(MaxSaveSlots);
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		FDCSaveSlotSummary Summary;
		Summary.SlotIndex = i;
		Summary.SlotName = SlotNameForIndex(i);
		if (UGameplayStatics::DoesSaveGameExist(Summary.SlotName, SaveUserIndex))
		{
			if (const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(Summary.SlotName, SaveUserIndex)))
			{
				Summary.bExists = true;
				Summary.CreatedAt = Save->CreatedAt;
				Summary.LastSavedAt = Save->LastSavedAt;
				Summary.Money = Save->Money;
				Summary.DayIndex = Save->DayIndex;
			}
		}
		Slots.Add(Summary);
	}
	return Slots;
}

bool UDCGameInstance::HasAnySaveSlot() const
{
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		if (UGameplayStatics::DoesSaveGameExist(SlotNameForIndex(i), SaveUserIndex))
		{
			return true;
		}
	}
	return false;
}

UDCSaveGame* UDCGameInstance::CreateNewSaveSlot(const FString& SlotName, int32 StartingMoney)
{
	UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
	if (!Save)
	{
		return nullptr;
	}
	const FDateTime Now = FDateTime::Now();
	Save->SlotName = SlotName;
	Save->CreatedAt = Now;
	Save->LastSavedAt = Now;
	Save->Money = StartingMoney;
	Save->DayIndex = 1;
	UGameplayStatics::SaveGameToSlot(Save, SlotName, SaveUserIndex);
	ActiveSlotName = SlotName;
	return Save;
}

bool UDCGameInstance::SelectSaveSlot(const FString& SlotName)
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return false;
	}
	ActiveSlotName = SlotName;
	return true;
}

bool UDCGameInstance::DeleteSaveSlot(const FString& SlotName)
{
	if (SlotName.IsEmpty())
	{
		return false;
	}
	const bool bDeleted = UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex)
		? UGameplayStatics::DeleteGameInSlot(SlotName, SaveUserIndex)
		: false;
	if (ActiveSlotName == SlotName)
	{
		ActiveSlotName.Empty();
	}
	return bDeleted;
}

void UDCGameInstance::ResetActiveSaveSlotToDayOne(int32 StartingMoney)
{
	if (ActiveSlotName.IsEmpty())
	{
		return;
	}
	FDateTime OldCreatedAt = FDateTime::Now();
	bool bOldIntroPlayed = false;
	bool bOldTutorialCompleted = false;
	if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		if (const UDCSaveGame* Existing = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex)))
		{
			OldCreatedAt = Existing->CreatedAt;
			bOldIntroPlayed = Existing->bIntroPlayed;
			bOldTutorialCompleted = Existing->bTutorialCompleted;
		}
	}
	UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->SlotName = ActiveSlotName;
	Save->CreatedAt = OldCreatedAt;
	Save->LastSavedAt = FDateTime::Now();
	Save->Money = StartingMoney;
	Save->DayIndex = 1;
	Save->bIntroPlayed = bOldIntroPlayed;
	Save->bTutorialCompleted = bOldTutorialCompleted;
	Save->bHasSavedGameplayState = false;
	UGameplayStatics::SaveGameToSlot(Save, ActiveSlotName, SaveUserIndex);
}

UDCSaveGame* UDCGameInstance::LoadOrCreateActiveSave() const
{
	if (ActiveSlotName.IsEmpty())
	{
		return nullptr;
	}
	UDCSaveGame* Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex));
	}
	if (!Save)
	{
		Save = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
		if (Save)
		{
			Save->SlotName = ActiveSlotName;
			Save->CreatedAt = FDateTime::Now();
		}
	}
	return Save;
}

void UDCGameInstance::SaveActiveSlotMetadata(int32 CurrentMoney, int32 CurrentDay)
{
	UDCSaveGame* Save = LoadOrCreateActiveSave();
	if (!Save)
	{
		return;
	}
	Save->LastSavedAt = FDateTime::Now();
	Save->Money = CurrentMoney;
	Save->DayIndex = FMath::Max(1, CurrentDay);
	UGameplayStatics::SaveGameToSlot(Save, ActiveSlotName, SaveUserIndex);
}

bool UDCGameInstance::ShouldPlayIntro() const
{
	if (ActiveSlotName.IsEmpty())
	{
		return !bIntroPlayedWithoutSlot;
	}
	if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		if (const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex)))
		{
			return !Save->bIntroPlayed;
		}
	}
	return true;
}

void UDCGameInstance::MarkIntroPlayed()
{
	if (ActiveSlotName.IsEmpty())
	{
		bIntroPlayedWithoutSlot = true;
		return;
	}
	UDCSaveGame* Save = LoadOrCreateActiveSave();
	if (!Save)
	{
		return;
	}
	Save->LastSavedAt = FDateTime::Now();
	Save->bIntroPlayed = true;
	UGameplayStatics::SaveGameToSlot(Save, ActiveSlotName, SaveUserIndex);
}

bool UDCGameInstance::ShouldRunTutorial() const
{
	if (ActiveSlotName.IsEmpty())
	{
		return !bTutorialCompletedWithoutSlot;
	}
	if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		if (const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex)))
		{
			return !Save->bTutorialCompleted;
		}
	}
	return true;
}

void UDCGameInstance::MarkTutorialCompleted()
{
	if (ActiveSlotName.IsEmpty())
	{
		bTutorialCompletedWithoutSlot = true;
		return;
	}
	UDCSaveGame* Save = LoadOrCreateActiveSave();
	if (!Save)
	{
		return;
	}
	Save->LastSavedAt = FDateTime::Now();
	Save->bTutorialCompleted = true;
	UGameplayStatics::SaveGameToSlot(Save, ActiveSlotName, SaveUserIndex);
}

void UDCGameInstance::CaptureGameModeToActiveSave(const ADCGameMode* GameMode, int32 OverrideDayIndex)
{
	if (!GameMode || ActiveSlotName.IsEmpty())
	{
		return;
	}
	UDCSaveGame* Save = LoadOrCreateActiveSave();
	if (!Save)
	{
		return;
	}
	const bool bHadGameplayState = Save->bHasSavedGameplayState;
	Save->bHasSavedGameplayState = true;
	Save->LastSavedAt = FDateTime::Now();
	Save->Money = GameMode->Economy ? GameMode->Economy->Money : Save->Money;
	Save->Criterio = GameMode->Economy ? GameMode->Economy->Criterio : 0;
	// Without an override never go back a day: after the day closes the save already holds Day+1 while the cycle still shows Day.
	const int32 CycleDay = GameMode->DayNightCycle ? GameMode->DayNightCycle->DayIndex : 1;
	const int32 RawDay = OverrideDayIndex > 0 ? OverrideDayIndex : (bHadGameplayState ? FMath::Max(CycleDay, Save->DayIndex) : CycleDay);
	Save->DayIndex = FMath::Max(1, RawDay);

	if (GameMode->Unlocks)
	{
		Save->UnlockedStarTier = GameMode->Unlocks->UnlockedStarTier;
		Save->UnlockedGenres = GameMode->Unlocks->UnlockedGenres.Array();
		Save->OwnedUnlocks = GameMode->Unlocks->OwnedUnlocks.Array();
	}
	if (GameMode->Inventory)
	{
		Save->InventoryStock = GameMode->Inventory->Stock;
	}
	if (GameMode->FilmCrafting)
	{
		Save->UnlockedSlots = GameMode->FilmCrafting->UnlockedSlots;
		Save->CraftedFilms = GameMode->FilmCrafting->CraftedFilms;
		Save->HeldFilms = GameMode->FilmCrafting->HeldFilms;
		Save->FilmsCreated = GameMode->FilmCrafting->GetFilmsCreated();
		Save->UsedTitles = GameMode->FilmCrafting->GetUsedTitles().Array();
	}
	if (GameMode->ClientDirector)
	{
		Save->ReviewHistory = GameMode->ClientDirector->ReviewHistory;
		Save->ReviewStreak = GameMode->ClientDirector->GetReviewStreak();
		Save->ScriptedUsed = GameMode->ClientDirector->GetScriptedUsed();
	}

	UGameplayStatics::SaveGameToSlot(Save, ActiveSlotName, SaveUserIndex);
}

void UDCGameInstance::ApplyActiveSaveToGameMode(ADCGameMode* GameMode)
{
	if (!GameMode || ActiveSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		return;
	}
	const UDCSaveGame* Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex));
	if (!Save)
	{
		return;
	}
	if (!Save->bHasSavedGameplayState)
	{
		// First entry into MAP_Rewind for a new/reset slot: snapshot the clean Day 1 defaults initialized by GameMode BeginPlay.
		CaptureGameModeToActiveSave(GameMode, 1);
		return;
	}

	if (GameMode->Economy)
	{
		GameMode->Economy->Money = Save->Money;
		GameMode->Economy->Criterio = Save->Criterio;
		GameMode->Economy->DayEarned = 0;
		GameMode->Economy->bGameIsOver = false;
		GameMode->Economy->bWon = false;
		GameMode->Economy->OnMoneyChanged.Broadcast(GameMode->Economy->Money, 0);
	}
	if (GameMode->Unlocks)
	{
		GameMode->Unlocks->UnlockedStarTier = Save->UnlockedStarTier;
		GameMode->Unlocks->UnlockedGenres = TSet<EGenre>(Save->UnlockedGenres);
		GameMode->Unlocks->OwnedUnlocks = TSet<FName>(Save->OwnedUnlocks);
	}
	if (GameMode->Inventory)
	{
		GameMode->Inventory->Stock = Save->InventoryStock;
		GameMode->Inventory->OnInventoryChanged.Broadcast();
	}
	if (GameMode->FilmCrafting)
	{
		GameMode->FilmCrafting->RestoreSavedState(Save->UnlockedSlots, Save->CraftedFilms, Save->HeldFilms, Save->FilmsCreated, Save->UsedTitles);
	}
	if (GameMode->ClientDirector)
	{
		GameMode->ClientDirector->CloseStore();
		GameMode->ClientDirector->RestoreSavedState(Save->ReviewHistory, Save->ReviewStreak, Save->ScriptedUsed);
	}
	if (GameMode->DayNightCycle)
	{
		GameMode->DayNightCycle->StartDay(FMath::Max(1, Save->DayIndex));
	}
}

void UDCGameInstance::TravelToGameplay()
{
	ADCAudioManager::SetGlobalParameterStatic("Day-Night", 0.f);
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

void UDCGameInstance::TravelToMainMenu()
{
	ADCAudioManager::SetGlobalParameterStatic("Location", 2.f);
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}
