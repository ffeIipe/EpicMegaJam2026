#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"

static const UPDA_GameConfig* GetConfig(const UActorComponent* C)
{
	const ADCGameMode* GM = Cast<ADCGameMode>(C->GetOwner());
	return GM ? GM->Config.Get() : nullptr;
}

// ---------------------------------------------------------------- Economy

void UAC_Economy::BeginPlay()
{
	Super::BeginPlay();
	if (const UPDA_GameConfig* Cfg = GetConfig(this))
	{
		Money = Cfg->StartMoney;
		OnMoneyChanged.Broadcast(Money, 0);
	}
}

bool UAC_Economy::SpendMoney(int32 Amount)
{
	if (Amount < 0 || Money < Amount)
	{
		return false;
	}
	Money -= Amount;
	OnMoneyChanged.Broadcast(Money, -Amount);
	return true;
}

void UAC_Economy::AddMoney(int32 Delta)
{
	Money += Delta;
	DayEarned += Delta;
	OnMoneyChanged.Broadcast(Money, Delta);
	const UPDA_GameConfig* Cfg = GetConfig(this);
	if (Cfg && !bWon && !bGameIsOver && Money >= Cfg->MovieBudgetGoal)
	{
		bWon = true;
		OnVictory.Broadcast();
	}
}

int32 UAC_Economy::GetRentForDay(int32 DayIndex) const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	if (!Cfg || !Cfg->DayTable)
	{
		return 0;
	}
	const FDayData* Last = nullptr;
	for (const TPair<FName, uint8*>& P : Cfg->DayTable->GetRowMap())
	{
		const FDayData* Row = reinterpret_cast<const FDayData*>(P.Value);
		if (Row->DayIndex == DayIndex)
		{
			return Row->RentAmount;
		}
		if (!Last || Row->DayIndex > Last->DayIndex)
		{
			Last = Row;
		}
	}
	return Last ? Last->RentAmount + (DayIndex - Last->DayIndex) * Cfg->RentIncreaseAfterLastDay : 0;
}

bool UAC_Economy::ChargeRent(int32 DayIndex)
{
	const int32 Rent = GetRentForDay(DayIndex);
	if (bGameIsOver || bWon)
	{
		return false;
	}
	if (Money < Rent)
	{
		bGameIsOver = true;
		OnGameOver.Broadcast();
		return false;
	}
	SpendMoney(Rent);
	OnRentCharged.Broadcast(Rent);
	return true;
}

// ---------------------------------------------------------------- Inventory

void UAC_Inventory::BeginPlay()
{
	Super::BeginPlay();
	const UPDA_GameConfig* Cfg = GetConfig(this);
	if (!Cfg || !Cfg->CatalogTable)
	{
		return;
	}
	for (const TPair<FName, uint8*>& Pair : Cfg->CatalogTable->GetRowMap())
	{
		const FCatalogData* Row = reinterpret_cast<const FCatalogData*>(Pair.Value);
		if (Row->bOwnedAtStart)
		{
			AddCatalog(Pair.Key);
		}
	}
}

void UAC_Inventory::AddCatalog(FName CatalogRow)
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	const FCatalogData* Row = Cfg && Cfg->CatalogTable ? Cfg->CatalogTable->FindRow<FCatalogData>(CatalogRow, TEXT("AddCatalog")) : nullptr;
	if (!Row || OwnedCatalogs.Contains(CatalogRow))
	{
		return;
	}
	OwnedCatalogs.Add(CatalogRow);
	for (const FName& Tape : Row->VHSRows)
	{
		OwnedVHS.AddUnique(Tape);
	}
	OnInventoryChanged.Broadcast();
}

bool UAC_Inventory::GetVHSData(FName Row, FVHSData& Out) const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	const FVHSData* Data = Cfg && Cfg->VHSTable ? Cfg->VHSTable->FindRow<FVHSData>(Row, TEXT("GetVHSData")) : nullptr;
	if (Data)
	{
		Out = *Data;
	}
	return Data != nullptr;
}

TArray<FName> UAC_Inventory::GetOwnedVHSByGenre(EGenre Genre) const
{
	TArray<FName> Result;
	FVHSData Data;
	for (const FName& Row : OwnedVHS)
	{
		if (GetVHSData(Row, Data) && Data.PrimaryGenre == Genre)
		{
			Result.Add(Row);
		}
	}
	return Result;
}

// ---------------------------------------------------------------- Film crafting

bool UAC_FilmCrafting::SetSlot(EFilmSlot Slot, FName VHSRow)
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	if (!GM || !GM->Inventory->HasVHS(VHSRow))
	{
		return false;
	}
	for (const TPair<EFilmSlot, FName>& Pair : Slots)
	{
		if (Pair.Key != Slot && Pair.Value == VHSRow)
		{
			return false; // same tape can't fill two slots
		}
	}
	Slots.Add(Slot, VHSRow);
	OnFilmStateChanged.Broadcast();
	return true;
}

void UAC_FilmCrafting::ClearSlot(EFilmSlot Slot)
{
	if (Slots.Remove(Slot) > 0)
	{
		OnFilmStateChanged.Broadcast();
	}
}

bool UAC_FilmCrafting::AreSlotsFull() const
{
	return Slots.Contains(EFilmSlot::Intro) && Slots.Contains(EFilmSlot::Middle) && Slots.Contains(EFilmSlot::Ending);
}

bool UAC_FilmCrafting::CanConfirm() const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	return Cfg && AreSlotsFull() && HeldFilms.Num() < Cfg->MaxHeldFilms;
}

bool UAC_FilmCrafting::ConfirmFilm(FText Title)
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	if (!CanConfirm() || !GM)
	{
		return false;
	}

	FCreatedFilm Film;
	FString TitleStr = Title.ToString().TrimStartAndEnd().Left(Cfg->MaxFilmTitleLength);
	Film.Title = TitleStr.IsEmpty() ? FText::FromString(FString::Printf(TEXT("Untitled Cut #%d"), FilmsCreated + 1)) : FText::FromString(TitleStr);
	Film.IntroRow = Slots[EFilmSlot::Intro];
	Film.MiddleRow = Slots[EFilmSlot::Middle];
	Film.EndingRow = Slots[EFilmSlot::Ending];

	float RatingSum = 0.f;
	for (const TPair<EFilmSlot, FName>& Pair : Slots)
	{
		FVHSData Tape;
		if (!GM->Inventory->GetVHSData(Pair.Value, Tape))
		{
			return false;
		}
		const float* Mult = Cfg->SlotMultipliers.Find(Pair.Key);
		for (const FGenreValue& GV : Tape.GenreValues)
		{
			Film.GenreProfile.FindOrAdd(GV.Genre) += GV.Value * (Mult ? *Mult : 1.f);
		}
		RatingSum += Tape.Rating;
	}
	Film.AverageRating = RatingSum / 3.f;

	++FilmsCreated;
	HeldFilms.Add(Film);
	Slots.Empty();
	OnFilmCreated.Broadcast(Film);
	OnFilmStateChanged.Broadcast();
	return true;
}

void UAC_FilmCrafting::DiscardHeldFilm()
{
	if (HeldFilms.Num() > 0)
	{
		HeldFilms.RemoveAt(0);
		OnFilmStateChanged.Broadcast();
	}
}

// ---------------------------------------------------------------- Supplier

bool UAC_Inventory::CanBuyCatalog(FName CatalogRow) const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	const FCatalogData* Row = Cfg && Cfg->CatalogTable ? Cfg->CatalogTable->FindRow<FCatalogData>(CatalogRow, TEXT("CanBuyCatalog")) : nullptr;
	return Row && GM && !OwnedCatalogs.Contains(CatalogRow)
		&& GM->DayNightCycle->DayIndex >= Row->UnlockDay && GM->Economy->Money >= Row->Price;
}

bool UAC_Inventory::BuyCatalog(FName CatalogRow)
{
	if (!CanBuyCatalog(CatalogRow))
	{
		return false;
	}
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	const FCatalogData* Row = GetConfig(this)->CatalogTable->FindRow<FCatalogData>(CatalogRow, TEXT("BuyCatalog"));
	if (!GM->Economy->SpendMoney(Row->Price))
	{
		return false;
	}
	AddCatalog(CatalogRow); // new tapes appear instantly; floor boxes are cosmetic (TODO(art))
	return true;
}
