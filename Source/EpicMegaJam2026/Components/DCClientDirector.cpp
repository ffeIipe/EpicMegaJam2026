#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Engine/TargetPoint.h"
#include "Core/DCClient.h"
#include "Core/BPFL_Scoring.h"
#include "Data/DCCustomers.h"
#include "Kismet/GameplayStatics.h"

UAC_ClientDirector::UAC_ClientDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	ClientClass = ADCClient::StaticClass();
}

static ADCGameMode* GetGM(const UActorComponent* C) { return Cast<ADCGameMode>(C->GetOwner()); }

/// Weighted pick of a persona (DT_Customers, section 5.6) valid for this phase and client level. Null when none fits.
static const FCustomerData* PickCustomer(const UDataTable* Table, EPhase Phase, int32 Tier)
{
	if (!Table)
	{
		return nullptr;
	}
	TArray<const FCustomerData*> Pool;
	float Sum = 0.f;
	for (const TPair<FName, uint8*>& P : Table->GetRowMap())
	{
		const FCustomerData* C = reinterpret_cast<const FCustomerData*>(P.Value);
		const bool bPhaseOk = C->SpawnPhase == ESpawnPhase::Any || (C->SpawnPhase == ESpawnPhase::Day) == (Phase == EPhase::Day);
		if (bPhaseOk && Tier >= C->TierMin && Tier <= C->TierMax && C->SpawnWeight > 0.f)
		{
			Pool.Add(C);
			Sum += C->SpawnWeight;
		}
	}
	float Roll = FMath::FRandRange(0.f, Sum);
	for (const FCustomerData* C : Pool)
	{
		if ((Roll -= C->SpawnWeight) <= 0.f)
		{
			return C;
		}
	}
	return Pool.IsEmpty() ? nullptr : Pool.Last();
}

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

const FClientTierData* UAC_ClientDirector::FindTier(int32 Tier) const
{
	const UDataTable* T = GetGM(this)->Config->ClientTierTable;
	if (T)
	{
		for (const TPair<FName, uint8*>& P : T->GetRowMap())
		{
			const FClientTierData* Row = reinterpret_cast<const FClientTierData*>(P.Value);
			if (Row->Tier == Tier)
			{
				return Row;
			}
		}
	}
	return nullptr;
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

// 5.4: High skills first (largest first), then Some (largest first), then None at random; the level's cap decides how many
TArray<FHintedSkill> UAC_ClientDirector::BuildHints(const FSkillVector& Target, int32 MaxHinted) const
{
	const int32 HighMin = GetGM(this)->Config->HintHighMin;
	struct FEntry { FHintedSkill Hint; int32 Value; float Tie; };
	TArray<FEntry> Entries;
	for (int32 i = 0; i < DCNumSkills; ++i)
	{
		const int32 Value = Target.Get((ESkill)i);
		FEntry& E = Entries.AddDefaulted_GetRef();
		E.Hint.Skill = (ESkill)i;
		E.Hint.Level = Value >= HighMin ? EHintLevel::High : Value >= 1 ? EHintLevel::Some : EHintLevel::None;
		E.Value = Value;
		E.Tie = FMath::FRand();
	}
	Entries.Sort([](const FEntry& A, const FEntry& B)
	{
		if (A.Hint.Level != B.Hint.Level) return (uint8)A.Hint.Level > (uint8)B.Hint.Level;
		if (A.Hint.Level != EHintLevel::None && A.Value != B.Value) return A.Value > B.Value;
		return A.Tie < B.Tie;
	});
	TArray<FHintedSkill> Hints;
	for (int32 i = 0; i < FMath::Min(MaxHinted, Entries.Num()); ++i)
	{
		Hints.Add(Entries[i].Hint);
	}
	return Hints;
}

// Greeting plus one line per hinted skill (never naming the skill), no line repeated within the day
FText UAC_ClientDirector::BuildOrderText(const TArray<FHintedSkill>& Hints, ESpawnPhase Phase)
{
	const UPDA_GameConfig* Cfg = GetGM(this)->Config;
	FString Out = PickLine(Cfg->GreetingTable, Phase == ESpawnPhase::Night ? ESpawnPhase::Night : ESpawnPhase::Day).ToString();
	for (const FHintedSkill& Hint : Hints)
	{
		TArray<FName> Fresh, All;
		if (Cfg->HintLineTable)
		{
			for (const TPair<FName, uint8*>& P : Cfg->HintLineTable->GetRowMap())
			{
				const FHintLineData* Row = reinterpret_cast<const FHintLineData*>(P.Value);
				if (!Row->bUnused && Row->Skill == Hint.Skill && Row->Level == Hint.Level)
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
			Out += TEXT(" TODO(DESIGN): hint line"); // no line written for this skill and level yet
			continue;
		}
		const FName Pick = Pool[FMath::RandRange(0, Pool.Num() - 1)];
		UsedHintRows.Add(Pick);
		Out += TEXT(" ") + Cfg->HintLineTable->FindRow<FHintLineData>(Pick, TEXT("Hint"))->Text.ToString();
	}
	// TODO(DESIGN): closing phrase has no data source in the doc
	return FText::FromString(Out);
}

// 5.2 steps 3 to 5 (or the scripted row): client level, persona, a recipe of real tapes and the Target it adds up to
void UAC_ClientDirector::GenerateOrder(const FClientArchetypeData& Arch, const FScriptedOrderData* Scripted, const FCustomerData*& OutPersona, FClientRuntime& Rt)
{
	ADCGameMode* GM = GetGM(this);
	const UPDA_GameConfig* Cfg = GM->Config;

	// Level: scripted clients are level 1, the others are drawn by SpawnWeight among the levels the star license allows
	int32 Tier = 1;
	if (!Scripted && Cfg->ClientTierTable)
	{
		float Sum = 0.f;
		for (const TPair<FName, uint8*>& P : Cfg->ClientTierTable->GetRowMap())
		{
			const FClientTierData* T = reinterpret_cast<const FClientTierData*>(P.Value);
			Sum += T->Tier <= GM->Unlocks->UnlockedStarTier ? T->SpawnWeight : 0.f;
		}
		float Roll = FMath::FRandRange(0.f, Sum);
		for (const TPair<FName, uint8*>& P : Cfg->ClientTierTable->GetRowMap())
		{
			const FClientTierData* T = reinterpret_cast<const FClientTierData*>(P.Value);
			if (T->Tier <= GM->Unlocks->UnlockedStarTier && T->SpawnWeight > 0.f && (Roll -= T->SpawnWeight) <= 0.f)
			{
				Tier = T->Tier;
				break;
			}
		}
	}
	const FClientTierData* TierRow = FindTier(Tier);
	Rt.Tier = Tier;
	OutPersona = PickCustomer(Cfg->CustomerTable, SchedulePhase, Tier);
	Rt.TipMult = OutPersona ? OutPersona->TipMult : 1.f;

	if (Scripted)
	{
		Rt.Target = Scripted->Target;
		Rt.HintedSkills = Scripted->HintedSkills;
	}
	else
	{
		const int32 StarMin = TierRow ? TierRow->RecipeStarMin : 1;
		const int32 StarMax = FMath::Min(TierRow ? TierRow->RecipeStarMax : 1, GM->Unlocks->UnlockedStarTier);
		const int32 MinTapes = TierRow ? TierRow->RecipeTapesMin : 1;
		const int32 MaxTapes = TierRow ? TierRow->RecipeTapesMax : 2;
		const int32 Tapes = FMath::Clamp(FMath::RandRange(MinTapes, MaxTapes), 1, GM->FilmCrafting->UnlockedSlots); // never more than the slots

		// The archetype's genres, or every unlocked genre when that leaves none
		TSet<EGenre> Genres;
		for (EGenre G : Arch.AllowedGenres)
		{
			if (GM->Unlocks->IsGenreUnlocked(G)) Genres.Add(G);
		}
		if (Genres.IsEmpty())
		{
			Genres = GM->Unlocks->UnlockedGenres;
		}

		struct FCandidate { FSkillVector Skills; float Weight; };
		TArray<FCandidate> Candidates;
		if (Cfg->VHSTable)
		{
			for (const TPair<FName, uint8*>& P : Cfg->VHSTable->GetRowMap())
			{
				const FVHSData* Tape = reinterpret_cast<const FVHSData*>(P.Value);
				int32 Points = 0;
				float Taste = 0.f;
				for (int32 i = 0; i < DCNumSkills; ++i)
				{
					Points += Tape->Skills.Get((ESkill)i);
					Taste += OutPersona ? OutPersona->TasteBias.Get((ESkill)i) * Tape->Skills.Get((ESkill)i) : 0.f;
				}
				if (Points > 0 && Tape->Rating >= StarMin && Tape->Rating <= StarMax && Genres.Contains(Tape->PrimaryGenre)) // Points > 0 skips the v1 rows
				{
					Candidates.Add({ Tape->Skills, Cfg->GeneratorBiasFloor + Taste });
				}
			}
		}
		if (Candidates.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("GenerateOrder: no tapes for tier %d, the order has no target"), Tier);
		}
		TArray<FSkillVector> Recipe;
		TArray<bool> Used;
		Used.Init(false, Candidates.Num());
		for (int32 k = 0; k < Tapes && !Candidates.IsEmpty(); ++k)
		{
			float Sum = 0.f;
			for (int32 i = 0; i < Candidates.Num(); ++i) Sum += Used[i] ? 0.f : Candidates[i].Weight;
			if (Sum <= 0.f) // every distinct tape is in the recipe already: repeats are allowed when the group is small
			{
				Used.Init(false, Candidates.Num());
				for (int32 i = 0; i < Candidates.Num(); ++i) Sum += Candidates[i].Weight;
			}
			float Roll = FMath::FRandRange(0.f, Sum);
			int32 Pick = Candidates.Num() - 1;
			for (int32 i = 0; i < Candidates.Num(); ++i)
			{
				if (!Used[i] && (Roll -= Candidates[i].Weight) <= 0.f)
				{
					Pick = i;
					break;
				}
			}
			Used[Pick] = true;
			Recipe.Add(Candidates[Pick].Skills);
		}
		Rt.Target = UBPFL_Scoring::ComputeProfile(Recipe, Cfg);
		Rt.HintedSkills = BuildHints(Rt.Target, TierRow ? TierRow->MaxHintedSkills : 2);
	}
	Rt.OrderText = BuildOrderText(Rt.HintedSkills, Arch.SpawnPhase);
}

ADCClient* UAC_ClientDirector::SpawnClient(FName ArchetypeRow)
{
	ADCGameMode* GM = GetGM(this);
	const UPDA_GameConfig* Cfg = GM->Config;
	ADCClientQueue* Queue = FindQueue();
	TArray<AActor*> Doors;
	UGameplayStatics::GetAllActorsWithTag(this, "Client_Door", Doors);
	AActor* Door = Doors.Num() ? Doors[0] : nullptr;
	if (!Queue || !Door || !Cfg->ArchetypeTable || ActiveClientCount() >= Queue->MaxSize())
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

	// The first clients of a run come from DT_ScriptedOrders, in order; the row also fixes the archetype (5.2 step 2)
	const FScriptedOrderData* Scripted = nullptr;
	if (Cfg->ScriptedOrderTable)
	{
		for (const TPair<FName, uint8*>& P : Cfg->ScriptedOrderTable->GetRowMap())
		{
			const FScriptedOrderData* Row = reinterpret_cast<const FScriptedOrderData*>(P.Value);
			if (Row->Index == ScriptedUsed + 1)
			{
				Scripted = Row;
				if (!Row->ArchetypeRow.IsNone()) ArchetypeRow = Row->ArchetypeRow;
			}
		}
	}
	const FClientArchetypeData* Arch = Cfg->ArchetypeTable->FindRow<FClientArchetypeData>(ArchetypeRow, TEXT("SpawnClient"));
	if (!Arch)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnClient: archetype row '%s' not found"), *ArchetypeRow.ToString());
		return nullptr;
	}

	FClientRuntime Rt;
	Rt.ArchetypeRow = ArchetypeRow;
	const FCustomerData* Persona = nullptr;
	GenerateOrder(*Arch, Scripted, Persona, Rt);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ADCClient* Client = GetWorld()->SpawnActor<ADCClient>(ClientClass ? ClientClass.Get() : ADCClient::StaticClass(),
		Door->GetActorLocation() + FVector(0, 0, 90), Door->GetActorRotation(), Params);
	if (!Client)
	{
		return nullptr;
	}
	ScriptedUsed += Scripted ? 1 : 0;
	const float Browsing = FMath::RandRange(Cfg->BrowseTimeMinSec, Cfg->BrowseTimeMaxSec);
	// Persona (section 5.6) overrides the archetype's mesh; no persona or no mesh keeps the archetype's
	USkeletalMesh* Mesh = Persona ? Persona->Mesh.LoadSynchronous() : nullptr;
	float Patience = ComputePatienceSec();
	if (Persona)
	{
		Rt.DisplayName = Persona->DisplayName;
		Patience = FMath::Min(Patience * Persona->PatienceMult, Cfg->ClientPatienceMaxSec);
	}
	Client->ApplyMesh(Mesh ? Mesh : Arch->Mesh.LoadSynchronous());
	Client->Init(Rt, this, Queue, Browse, Door->GetActorLocation(), Patience, Browsing);
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
	GM->FilmCrafting->ConsumeHeldFilm(); // delivered: the tapes stay spent

	const FClientTierData* TierRow = FindTier(Client->Runtime.Tier);
	const float Mult = TierRow ? TierRow->PayMultiplier : 1.f;
	FReview R;
	R.FilmTitle = Film.Title;
	R.ErrorRatio = UBPFL_Scoring::ComputeErrorRatio(Film.SkillProfile, Client->Runtime.Target);
	R.Accuracy = UBPFL_Scoring::ComputeAccuracy(R.ErrorRatio);
	R.Reaction = UBPFL_Scoring::GetReaction(R.ErrorRatio, Cfg);
	R.MoneyDelta = UBPFL_Scoring::ComputeTip(R.Reaction, R.Accuracy, Mult * Client->Runtime.TipMult, Cfg) - UBPFL_Scoring::ComputeFine(R.Reaction, Mult, Cfg);

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
	ESkill FeedbackSkill;
	EFeedbackDirection FeedbackDirection;
	if (R.Reaction != EReaction::Loved && UBPFL_Scoring::GetFeedbackSkill(Film.SkillProfile, Client->Runtime.Target, FeedbackSkill, FeedbackDirection))
	{
		R.FeedbackLine = UBPFL_Scoring::PickFeedbackLine(Cfg->FeedbackLineTable, FeedbackSkill, FeedbackDirection); // shown only in the review (6.3)
	}

	GM->Economy->AddMoney(UBPFL_Scoring::ComputeRentalPrice(Mult, Cfg));
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
	// Archetype by SpawnWeight (a scripted order overrides it inside SpawnClient) among the ones valid for this phase
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
	return SpawnClient(Chosen) != nullptr;
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
