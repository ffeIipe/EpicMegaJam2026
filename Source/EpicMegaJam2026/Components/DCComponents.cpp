#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Core/BPFL_Scoring.h"

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

// ---------------------------------------------------------------- Unlocks

void UAC_Unlocks::BeginPlay()
{
	Super::BeginPlay();
	const UPDA_GameConfig* Cfg = GetConfig(this);
	if (!Cfg)
	{
		return;
	}
	UnlockedStarTier = Cfg->StartUnlockedStarTier;
	if (Cfg->GenreTable)
	{
		for (const TPair<FName, uint8*>& Pair : Cfg->GenreTable->GetRowMap())
		{
			const FGenreData* Row = reinterpret_cast<const FGenreData*>(Pair.Value);
			if (Row->bUnlockedAtStart)
			{
				UnlockedGenres.Add(Row->Genre);
			}
		}
	}
}

const FUnlockData* UAC_Unlocks::FindUnlock(FName UnlockId) const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	return Cfg && Cfg->UnlockTable ? Cfg->UnlockTable->FindRow<FUnlockData>(UnlockId, TEXT("FindUnlock"), false) : nullptr;
}

int32 UAC_Unlocks::GetGenreCount() const
{
	return UnlockedGenres.Num() - (UnlockedGenres.Contains(EGenre::Paranormal) ? 1 : 0);
}

bool UAC_Unlocks::CanBuyUnlock(FName UnlockId) const
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	const FUnlockData* Row = FindUnlock(UnlockId);
	if (!GM || !Row || OwnedUnlocks.Contains(UnlockId) || GM->Economy->Money < Row->Price)
	{
		return false;
	}
	for (const FName& Required : Row->RequiresUnlocks)
	{
		if (!OwnedUnlocks.Contains(Required))
		{
			return false;
		}
	}
	return GetGenreCount() >= Row->MinGenresOwned;
}

bool UAC_Unlocks::BuyUnlock(FName UnlockId)
{
	ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	const FUnlockData* Row = FindUnlock(UnlockId);
	if (!CanBuyUnlock(UnlockId) || !GM->Economy->SpendMoney(Row->Price))
	{
		return false;
	}
	OwnedUnlocks.Add(UnlockId);
	switch (Row->Type)
	{
	case EUnlockType::Genre: UnlockedGenres.Add(Row->Genre); break;
	case EUnlockType::Slot: GM->FilmCrafting->SetUnlockedSlots(GM->FilmCrafting->UnlockedSlots + 1); break;
	case EUnlockType::StarTier: UnlockedStarTier = FMath::Min(UnlockedStarTier + 1, 5); break;
	}
	OnUnlockPurchased.Broadcast(UnlockId);
	return true;
}

// ---------------------------------------------------------------- Inventory

void UAC_Inventory::BeginPlay()
{
	Super::BeginPlay();
	const UPDA_GameConfig* Cfg = GetConfig(this);
	if (!Cfg)
	{
		return;
	}
	for (const TPair<FName, int32>& Pair : Cfg->StartingTapes)
	{
		AddTape(Pair.Key, Pair.Value);
	}
}

void UAC_Inventory::AddTape(FName Row, int32 Count)
{
	if (Row.IsNone() || Count <= 0)
	{
		return;
	}
	Stock.FindOrAdd(Row) += Count;
	OnInventoryChanged.Broadcast();
}

bool UAC_Inventory::ConsumeTape(FName Row, int32 Count)
{
	int32* Have = Stock.Find(Row);
	if (Count <= 0 || !Have || *Have < Count)
	{
		return false;
	}
	if ((*Have -= Count) == 0)
	{
		Stock.Remove(Row);
	}
	OnInventoryChanged.Broadcast();
	return true;
}

TArray<FName> UAC_Inventory::GetOwnedVHS() const
{
	TArray<FName> Rows;
	Stock.GetKeys(Rows);
	Rows.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Rows;
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
	for (const FName& Row : GetOwnedVHS())
	{
		if (GetVHSData(Row, Data) && Data.PrimaryGenre == Genre)
		{
			Result.Add(Row);
		}
	}
	return Result;
}

// ---------------------------------------------------------------- Film crafting

void UAC_FilmCrafting::BeginPlay()
{
	Super::BeginPlay();
	const UPDA_GameConfig* Cfg = GetConfig(this);
	SetUnlockedSlots(Cfg ? Cfg->StartUnlockedSlots : UnlockedSlots);
}

void UAC_FilmCrafting::SetUnlockedSlots(int32 Count)
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	UnlockedSlots = FMath::Clamp(Count, 1, Cfg ? FMath::Max(Cfg->MaxSlots, 1) : 5);
	SlotRows.SetNum(UnlockedSlots);
	OnFilmStateChanged.Broadcast();
}

int32 UAC_FilmCrafting::GetAvailableCount(FName VHSRow) const
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	int32 Used = 0;
	for (const FName& Row : SlotRows)
	{
		Used += Row == VHSRow ? 1 : 0;
	}
	return GM ? GM->Inventory->GetCount(VHSRow) - Used : 0;
}

bool UAC_FilmCrafting::SetSlot(int32 Index, FName VHSRow)
{
	if (!SlotRows.IsValidIndex(Index) || VHSRow.IsNone())
	{
		return false;
	}
	// A tape already in this slot goes back to the pool before the new one is counted
	const FName Previous = SlotRows[Index];
	SlotRows[Index] = NAME_None;
	if (GetAvailableCount(VHSRow) < 1)
	{
		SlotRows[Index] = Previous;
		return false;
	}
	SlotRows[Index] = VHSRow;
	OnFilmStateChanged.Broadcast();
	return true;
}

void UAC_FilmCrafting::ClearSlot(int32 Index)
{
	if (SlotRows.IsValidIndex(Index) && !SlotRows[Index].IsNone())
	{
		SlotRows[Index] = NAME_None;
		OnFilmStateChanged.Broadcast();
	}
}

int32 UAC_FilmCrafting::GetFilledSlots() const
{
	int32 Filled = 0;
	for (const FName& Row : SlotRows)
	{
		Filled += Row.IsNone() ? 0 : 1;
	}
	return Filled;
}

FSkillVector UAC_FilmCrafting::GetProfile() const
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	TArray<FSkillVector> Skills;
	FVHSData Tape;
	for (const FName& Row : SlotRows)
	{
		if (!Row.IsNone() && GM->Inventory->GetVHSData(Row, Tape))
		{
			Skills.Add(Tape.Skills);
		}
	}
	return UBPFL_Scoring::ComputeProfile(Skills, GM->Config);
}

bool UAC_FilmCrafting::CanConfirm() const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	return Cfg && GetFilledSlots() >= 1 && HeldFilms.Num() < Cfg->MaxHeldFilms;
}

bool UAC_FilmCrafting::ConfirmFilm(FText Title)
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	if (!CanConfirm() || !GM)
	{
		return false;
	}
	for (const FName& Row : SlotRows)
	{
		if (!Row.IsNone() && GetAvailableCount(Row) < 0) // stock dropped under what the slots hold
		{
			return false;
		}
	}

	FCreatedFilm Film;
	FString TitleStr = Title.ToString().TrimStartAndEnd().Left(Cfg->MaxFilmTitleLength);
	Film.Title = TitleStr.IsEmpty() ? FText::FromString(FString::Printf(TEXT("Untitled Cut #%d"), FilmsCreated + 1)) : FText::FromString(TitleStr);
	Film.SlotRows = SlotRows;
	Film.SkillProfile = GetProfile();
	for (const FName& Row : SlotRows)
	{
		GM->Inventory->ConsumeTape(Row); // one copy per filled slot; NAME_None is ignored
	}

	++FilmsCreated;
	HeldFilms.Add(Film);
	SlotRows.Init(NAME_None, UnlockedSlots);
	OnFilmCreated.Broadcast(Film);
	OnFilmStateChanged.Broadcast();
	return true;
}

void UAC_FilmCrafting::DiscardHeldFilm()
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	if (HeldFilms.Num() > 0 && GM)
	{
		for (const FName& Row : HeldFilms[0].SlotRows)
		{
			GM->Inventory->AddTape(Row); // free, and reveals nothing about the order (4.4 rule 8)
		}
		HeldFilms.RemoveAt(0);
		OnFilmStateChanged.Broadcast();
	}
}

void UAC_FilmCrafting::ConsumeHeldFilm()
{
	if (HeldFilms.Num() > 0)
	{
		HeldFilms.RemoveAt(0);
		OnFilmStateChanged.Broadcast();
	}
}

// ---------------------------------------------------------------- Supplier

int32 UAC_Inventory::GetTapePrice(FName Row) const
{
	const UPDA_GameConfig* Cfg = GetConfig(this);
	FVHSData Tape;
	if (!Cfg || !GetVHSData(Row, Tape) || !Cfg->TapePriceByStar.IsValidIndex(Tape.Rating - 1))
	{
		return 0;
	}
	return Cfg->TapePriceByStar[Tape.Rating - 1];
}

bool UAC_Inventory::IsTapeForSale(FName Row) const
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	FVHSData Tape;
	if (!GM || !GetVHSData(Row, Tape))
	{
		return false;
	}
	const int32 Points = Tape.Skills.Funny + Tape.Skills.Scary + Tape.Skills.Brave + Tape.Skills.Thoughtful + Tape.Skills.Romantic;
	return Points > 0 // the v1 rows have no skills and are never sold
		&& GM->Unlocks->IsGenreUnlocked(Tape.PrimaryGenre) && Tape.Rating <= GM->Unlocks->UnlockedStarTier;
}

bool UAC_Inventory::CanBuyTape(FName Row, int32 Count) const
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	return GM && Count > 0 && IsTapeForSale(Row) && GM->Economy->Money >= GetTapePrice(Row) * Count;
}

bool UAC_Inventory::BuyTape(FName Row, int32 Count)
{
	const ADCGameMode* GM = Cast<ADCGameMode>(GetOwner());
	if (!CanBuyTape(Row, Count) || !GM->Economy->SpendMoney(GetTapePrice(Row) * Count))
	{
		return false;
	}
	AddTape(Row, Count); // new tapes appear instantly; floor boxes are cosmetic (TODO(art))
	return true;
}
