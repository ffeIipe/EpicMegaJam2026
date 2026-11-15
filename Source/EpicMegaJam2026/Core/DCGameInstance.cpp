#include "Core/DCGameInstance.h"
#include "Kismet/GameplayStatics.h"

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

void UDCGameInstance::SaveActiveSlotMetadata(int32 CurrentMoney, int32 CurrentDay)
{
	if (ActiveSlotName.IsEmpty())
	{
		return;
	}
	UDCSaveGame* Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex));
	}
	if (!Save)
	{
		Save = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
		if (!Save)
		{
			return;
		}
		Save->SlotName = ActiveSlotName;
		Save->CreatedAt = FDateTime::Now();
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
	UDCSaveGame* Save = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(ActiveSlotName, SaveUserIndex))
	{
		Save = Cast<UDCSaveGame>(UGameplayStatics::LoadGameFromSlot(ActiveSlotName, SaveUserIndex));
	}
	if (!Save)
	{
		Save = Cast<UDCSaveGame>(UGameplayStatics::CreateSaveGameObject(UDCSaveGame::StaticClass()));
		if (!Save)
		{
			return;
		}
		Save->SlotName = ActiveSlotName;
		Save->CreatedAt = FDateTime::Now();
	}
	Save->LastSavedAt = FDateTime::Now();
	Save->bIntroPlayed = true;
	UGameplayStatics::SaveGameToSlot(Save, ActiveSlotName, SaveUserIndex);
}

void UDCGameInstance::TravelToGameplay()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, GameplayLevelName);
}

void UDCGameInstance::TravelToMainMenu()
{
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}
