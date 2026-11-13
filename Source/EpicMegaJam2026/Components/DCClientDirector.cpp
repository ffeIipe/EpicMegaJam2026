#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Engine/TargetPoint.h"
#include "Core/DCClient.h"
#include "Core/BPFL_Scoring.h"
#include "Kismet/GameplayStatics.h"

UAC_ClientDirector::UAC_ClientDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	ClientClass = ADCClient::StaticClass();
}

static ADCGameMode* GetGM(const UActorComponent* C) { return Cast<ADCGameMode>(C->GetOwner()); }

ADCClientQueue* UAC_ClientDirector::FindQueue() const
{
	return Cast<ADCClientQueue>(UGameplayStatics::GetActorOfClass(this, ADCClientQueue::StaticClass()));
}

ADCClient* UAC_ClientDirector::GetFrontClient() const
{
	const ADCClientQueue* Q = FindQueue();
	return Q ? Q->GetFront() : nullptr;
}

int32 UAC_ClientDirector::ActiveClientCount() const
{
	int32 N = 0;
	for (const TWeakObjectPtr<ADCClient>& C : Clients)
	{
		if (C.IsValid() && C->Runtime.State != EClientState::Leaving && C->Runtime.State != EClientState::Served)
		{
			++N;
		}
	}
	return N;
}

float UAC_ClientDirector::ComputePatienceSec() const
{
	const UPDA_GameConfig* Cfg = GetGM(this)->Config;
	const int32 Levels = Cfg->CriterioPerLevel > 0 ? GetGM(this)->Economy->Criterio / Cfg->CriterioPerLevel : 0;
	return FMath::Min(Cfg->ClientPatienceBaseSec + Levels * Cfg->CriterioPatienceBonusSec, Cfg->ClientPatienceMaxSec);
}

float UAC_ClientDirector::CategoryMultiplier(EClientCategory Category) const
{
	const UDataTable* T = GetGM(this)->Config->CategoryTable;
	if (T)
	{
		for (const TPair<FName, uint8*>& P : T->GetRowMap())
		{
			const FClientCategoryData* Row = reinterpret_cast<const FClientCategoryData*>(P.Value);
			if (Row->Category == Category)
			{
				return Row->PayMultiplier;
			}
		}
	}
	return 1.f;
}

FText UAC_ClientDirector::PickLine(const UDataTable* Table, ESpawnPhase Phase) const
{
	TArray<const FLineData*> Candidates;
	if (Table)
	{
		for (const TPair<FName, uint8*>& P : Table->GetRowMap())
		{
			const FLineData* Row = reinterpret_cast<const FLineData*>(P.Value);
			if (Row->SpawnPhase == Phase || Row->SpawnPhase == ESpawnPhase::Any)
			{
				Candidates.Add(Row);
			}
		}
	}
	// TODO(DESIGN): greeting/farewell lines pending from the team
	return Candidates.Num() ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)]->Text : FText::FromString(TEXT("TODO(DESIGN): line"));
}

FText UAC_ClientDirector::BuildOrderText(const TArray<FGenrePreference>& Prefs, ESpawnPhase Phase)
{
	const UPDA_GameConfig* Cfg = GetGM(this)->Config;
	FString Out = PickLine(Cfg->GreetingTable, Phase == ESpawnPhase::Night ? ESpawnPhase::Night : ESpawnPhase::Day).ToString();
	for (const FGenrePreference& Pref : Prefs)
	{
		const EPolarity Pol = Pref.Weight > 0 ? EPolarity::Likes : EPolarity::Dislikes;
		TArray<FName> Fresh, All;
		if (Cfg->HintLineTable)
		{
			for (const TPair<FName, uint8*>& P : Cfg->HintLineTable->GetRowMap())
			{
				const FHintLineData* Row = reinterpret_cast<const FHintLineData*>(P.Value);
				if (Row->Genre == Pref.Genre && Row->Polarity == Pol)
				{
					All.Add(P.Key);
					if (!UsedHintRows.Contains(P.Key))
					{
						Fresh.Add(P.Key);
					}
				}
			}
		}
		const TArray<FName>& Pool = Fresh.Num() ? Fresh : All; // reuse only when every line was used today
		if (Pool.Num() == 0)
		{
			Out += TEXT(" TODO(DESIGN): hint line"); // no line loaded for this genre/polarity
			continue;
		}
		const FName Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		UsedHintRows.Add(Pick);
		Out += TEXT(" ") + Cfg->HintLineTable->FindRow<FHintLineData>(Pick, TEXT("Hint"))->Text.ToString();
	}
	// TODO(DESIGN): closing phrase has no data source in the doc
	return FText::FromString(Out);
}

ADCClient* UAC_ClientDirector::SpawnClient(FName ArchetypeRow, EClientCategory Category)
{
	ADCGameMode* GM = GetGM(this);
	const UPDA_GameConfig* Cfg = GM->Config;
	ADCClientQueue* Queue = FindQueue();
	TArray<AActor*> Doors;
	UGameplayStatics::GetAllActorsWithTag(this, "Client_Door", Doors);
	AActor* Door = Doors.Num() ? Doors[0] : nullptr;
	if (!Queue || !Door || !Cfg->ArchetypeTable || !Cfg->CategoryTable || ActiveClientCount() >= Queue->MaxSize())
	{
		return nullptr; // full store (5.5) or level not set up
	}

	ADCBrowsePoint* Browse = nullptr;
	{
		TArray<AActor*> Points;
		UGameplayStatics::GetAllActorsOfClass(this, ADCBrowsePoint::StaticClass(), Points);
		TArray<ADCBrowsePoint*> Free;
		for (AActor* A : Points)
		{
			if (!Cast<ADCBrowsePoint>(A)->bOccupied) Free.Add(Cast<ADCBrowsePoint>(A));
		}
		if (Free.Num() == 0) return nullptr;
		Browse = Free[FMath::RandRange(0, Free.Num() - 1)];
	}

	const FClientArchetypeData* Arch = Cfg->ArchetypeTable->FindRow<FClientArchetypeData>(ArchetypeRow, TEXT("SpawnClient"));
	if (!Arch)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnClient: archetype row '%s' not found"), *ArchetypeRow.ToString());
		return nullptr;
	}

	FClientRuntime Rt;
	Rt.ArchetypeRow = ArchetypeRow;
	if (ArchetypeRow == TutorialArchetypeRow)
	{
		bTutorialSpawned = true;
	}
	Rt.Category = Category;
	if (Arch->FixedPreferences.Num() > 0)
	{
		Rt.Preferences = Arch->FixedPreferences;
	}
	else
	{
		const FClientCategoryData* Cat = nullptr;
		for (const TPair<FName, uint8*>& P : Cfg->CategoryTable->GetRowMap())
		{
			const FClientCategoryData* Row = reinterpret_cast<const FClientCategoryData*>(P.Value);
			if (Row->Category == Category) Cat = Row;
		}
		if (!Cat) return nullptr;
		TArray<EGenre> Genres = Arch->AllowedGenres;
		if (Genres.IsEmpty())
		{
			for (int32 i = 0; i <= (int32)EGenre::Paranormal; ++i) Genres.Add((EGenre)i);
		}
		for (int32 n = 0; n < Cat->NumAttributes && Genres.Num() > 0; ++n) // distinct genres: never two attributes on one genre
		{
			const int32 Idx = FMath::RandRange(0, Genres.Num() - 1);
			FGenrePreference Pref;
			Pref.Genre = Genres[Idx];
			Genres.RemoveAtSwap(Idx);
			const int32 W = FMath::RandRange(Cat->WeightMin, Cat->WeightMax);
			Pref.Weight = FMath::FRand() < Cat->PositiveChance ? W : -W;
			Rt.Preferences.Add(Pref);
		}
	}
	Rt.OrderText = BuildOrderText(Rt.Preferences, Arch->SpawnPhase);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADCClient* Client = GetWorld()->SpawnActor<ADCClient>(ClientClass ? ClientClass.Get() : ADCClient::StaticClass(),
		Door->GetActorLocation() + FVector(0, 0, 90), Door->GetActorRotation(), Params);
	if (!Client)
	{
		return nullptr;
	}
	const float Browsing = FMath::RandRange(Cfg->BrowseTimeMinSec, Cfg->BrowseTimeMaxSec);
	Client->ApplyMesh(Arch->Mesh.LoadSynchronous());
	Client->Init(Rt, this, Queue, Browse, Door->GetActorLocation(), ComputePatienceSec(), Browsing);
	Clients.Add(Client);
	return Client;
}

void UAC_ClientDirector::NotifyReachedCounter(ADCClient* Client)
{
	OnClientReachedCounter.Broadcast(Client);
}

void UAC_ClientDirector::ApplyLeavePenalty()
{
	++Stats.ClientsLost;
	const int32 Penalty = GetGM(this)->Config->LeavePenalty;
	if (Penalty > 0)
	{
		GetGM(this)->Economy->AddMoney(-Penalty);
	}
}

void UAC_ClientDirector::NotifyLeft(ADCClient* Client, bool bServed)
{
	OnClientLeft.Broadcast(Client, bServed);
	const FReview* Pending = PendingReviews.Find(Client);
	if (!bServed || !Pending)
	{
		return;
	}
	const FReview Review = *Pending;
	PendingReviews.Remove(Client);
	FPendingReview& P = PendingTimers.AddDefaulted_GetRef();
	P.Review = Review;
	P.Id = ++NextReviewId;
	const FReview Copy = Review;
	const int32 Id = P.Id;
	GetWorld()->GetTimerManager().SetTimer(P.Handle, FTimerDelegate::CreateWeakLambda(this, [this, Copy, Id]()
	{
		PendingTimers.RemoveAll([Id](const FPendingReview& X) { return X.Id == Id; });
		ApplyReview(Copy);
	}), GetGM(this)->Config->ReviewDelaySec, false);
}

bool UAC_ClientDirector::DeliverFilm(ADCClient* Client)
{
	ADCGameMode* GM = GetGM(this);
	const UPDA_GameConfig* Cfg = GM->Config;
	if (!Client || !Client->IsAtFront() || Client != GetFrontClient() || GM->FilmCrafting->HeldFilms.IsEmpty())
	{
		return false;
	}
	const FCreatedFilm Film = GM->FilmCrafting->HeldFilms[0];
	GM->FilmCrafting->DiscardHeldFilm();

	const float Mult = CategoryMultiplier(Client->Runtime.Category);
	FReview R;
	R.FilmTitle = Film.Title;
	R.Score = UBPFL_Scoring::ComputeScore(Client->Runtime.Preferences, Film, Cfg);
	R.Reaction = UBPFL_Scoring::GetReaction(R.Score, Cfg);
	R.MoneyDelta = UBPFL_Scoring::ComputeTip(R.Reaction, R.Score, Mult, Cfg) - UBPFL_Scoring::ComputeFine(R.Reaction, Mult, Cfg);

	TArray<const FReviewLineData*> Lines;
	if (Cfg->ReviewLineTable)
	{
		for (const TPair<FName, uint8*>& P : Cfg->ReviewLineTable->GetRowMap())
		{
			const FReviewLineData* Row = reinterpret_cast<const FReviewLineData*>(P.Value);
			if (Row->Reaction == R.Reaction) Lines.Add(Row);
		}
	}
	// TODO(DESIGN): review lines pending from the team
	R.Line = Lines.Num() ? Lines[FMath::RandRange(0, Lines.Num() - 1)]->Text : FText::FromString(TEXT("TODO(DESIGN): review line"));

	GM->Economy->AddMoney(Cfg->BaseRentalPrice);
	++Stats.ClientsServed;
	switch (R.Reaction)
	{
	case EReaction::Loved: ++Stats.Loved; break;
	case EReaction::Liked: ++Stats.Liked; break;
	case EReaction::Neutral: ++Stats.Neutral; break;
	default: ++Stats.Hated; break;
	}
	if (R.Reaction == EReaction::Loved)
	{
		GM->Economy->AddCriterio(1);
	}
	PendingReviews.Add(Client, R);
	Client->MarkServed(PickLine(Cfg->FarewellTable, ESpawnPhase::Day));
	return true; // no reaction shown at delivery: the result only arrives with the review
}

// ---------------------------------------------------------------- Day schedule, closing, reviews

void UAC_ClientDirector::ApplyReview(const FReview& Review)
{
	GetGM(this)->Economy->AddMoney(Review.MoneyDelta); // tip or fine arrives with the online review
	OnReviewReceived.Broadcast(Review);
}

static const FDayData* FindDayRow(const UDataTable* Table, int32 Day)
{
	const FDayData* Best = nullptr;
	if (Table)
	{
		for (const TPair<FName, uint8*>& P : Table->GetRowMap())
		{
			const FDayData* Row = reinterpret_cast<const FDayData*>(P.Value);
			if (Row->DayIndex == Day) return Row;
			if (!Best || Row->DayIndex > Best->DayIndex) Best = Row;
		}
	}
	return Best; // days past the last row repeat the last one
}

void UAC_ClientDirector::BeginPhase(EPhase Phase, int32 DayIndex)
{
	const UPDA_GameConfig* Cfg = GetGM(this)->Config;
	SchedulePhase = Phase;
	ScheduleDay = DayIndex;
	PhaseTime = 0.f;
	NextSpawn = 0;
	SpawnTimes.Empty();
	if (Phase == EPhase::Day)
	{
		Stats = FDaySummary();
	}
	const FDayData* Row = FindDayRow(Cfg->DayTable, DayIndex);
	const int32 N = Row ? (Phase == EPhase::Day ? Row->DayClients : Row->NightClients) : 0;
	const float Duration = Phase == EPhase::Day ? Cfg->DayDurationSec : Cfg->NightDurationSec;
	for (int32 k = 1; k <= N; ++k)
	{
		// evenly spread: one spawn every Duration / (N + 1) seconds, plus jitter
		const float T = Duration / (N + 1) * k + FMath::FRandRange(-Cfg->SpawnJitterSec, Cfg->SpawnJitterSec);
		SpawnTimes.Add(FMath::Clamp(T, 0.f, Duration));
	}
	if (SpawnTimes.Num() > 0 && Phase == EPhase::Day && DayIndex == 1)
	{
		SpawnTimes[0] = Cfg->FirstClientDelaySec; // hook: first client shows up almost at once
	}
	SpawnTimes.Sort();
}

void UAC_ClientDirector::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetGM(this)->DayNightCycle->bRunning)
	{
		return;
	}
	PhaseTime += DeltaTime;
	while (NextSpawn < SpawnTimes.Num() && PhaseTime >= SpawnTimes[NextSpawn])
	{
		if (!TrySpawnScheduled())
		{
			break; // store full: retry next tick until a place frees up
		}
		++NextSpawn;
	}
}

bool UAC_ClientDirector::TrySpawnScheduled()
{
	const UPDA_GameConfig* Cfg = GetGM(this)->Config;
	if (!Cfg->ArchetypeTable || !Cfg->DayTable)
	{
		return true; // nothing configured: drop the slot instead of retrying forever
	}
	// First client of the game: always the tutorial one
	if (!bTutorialSpawned)
	{
		return SpawnClient(TutorialArchetypeRow, EClientCategory::Casual) != nullptr;
	}
	// Category from this day's weights
	const FDayData* Day = FindDayRow(Cfg->DayTable, ScheduleDay);
	EClientCategory Category = EClientCategory::Casual;
	if (Day)
	{
		const float Total = Day->CasualWeight + Day->EnthusiastWeight + Day->CinephileWeight;
		float Roll = FMath::FRandRange(0.f, FMath::Max(Total, KINDA_SMALL_NUMBER));
		if ((Roll -= Day->CasualWeight) >= 0.f)
		{
			Category = (Roll -= Day->EnthusiastWeight) >= 0.f ? EClientCategory::Cinephile : EClientCategory::Enthusiast;
		}
	}
	// Archetype by SpawnWeight among the ones valid for this phase
	TArray<TPair<FName, float>> Pool;
	float Sum = 0.f;
	for (const TPair<FName, uint8*>& P : Cfg->ArchetypeTable->GetRowMap())
	{
		const FClientArchetypeData* A = reinterpret_cast<const FClientArchetypeData*>(P.Value);
		const bool bPhaseOk = A->SpawnPhase == ESpawnPhase::Any
			|| (A->SpawnPhase == ESpawnPhase::Day) == (SchedulePhase == EPhase::Day);
		if (!A->bTutorialOnly && bPhaseOk && A->SpawnWeight > 0.f)
		{
			Pool.Emplace(P.Key, A->SpawnWeight);
			Sum += A->SpawnWeight;
		}
	}
	if (Pool.IsEmpty())
	{
		return true; // TODO(DESIGN): no archetype rows for this phase yet
	}
	float Roll = FMath::FRandRange(0.f, Sum);
	FName Chosen = Pool.Last().Key;
	for (const TPair<FName, float>& E : Pool)
	{
		if ((Roll -= E.Value) <= 0.f)
		{
			Chosen = E.Key;
			break;
		}
	}
	return SpawnClient(Chosen, Category) != nullptr;
}

void UAC_ClientDirector::CloseStore()
{
	SpawnTimes.Empty();
	for (const TWeakObjectPtr<ADCClient>& C : Clients)
	{
		if (C.IsValid())
		{
			C->ForceLeave(); // no penalty
		}
	}
	// Pending reviews resolve now
	TArray<FPendingReview> Copy = PendingTimers;
	PendingTimers.Empty();
	for (const FPendingReview& P : Copy)
	{
		GetWorld()->GetTimerManager().ClearTimer(const_cast<FPendingReview&>(P).Handle);
		ApplyReview(P.Review);
	}
}
