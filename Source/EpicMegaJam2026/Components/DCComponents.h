// Director's Cut rule components (SystemsDesigns.md section 2). Logic arrives in later steps; for now they only expose their dispatchers.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DCTypes.h"
#include "DCComponents.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoneyChanged, int32, NewMoney, int32, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRentCharged, int32, Amount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGameOver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVictory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPhaseChanged, EPhase, Phase, int32, DayIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseTimeTick, float, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClientReachedCounter, AActor*, Client);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnClientLeft, AActor*, Client, bool, bServed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReviewReceived, const FReview&, Review);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFilmCreated, const FCreatedFilm&, Film);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDayClosed, const FDaySummary&, Summary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUnlockPurchased, FName, UnlockId);

UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_Economy : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnMoneyChanged OnMoneyChanged;
	UPROPERTY(BlueprintAssignable) FOnRentCharged OnRentCharged;
	UPROPERTY(BlueprintAssignable) FOnGameOver OnGameOver;
	UPROPERTY(BlueprintAssignable) FOnVictory OnVictory;

	UPROPERTY(BlueprintReadOnly) int32 Money = 0;
	// Each Loved film adds 1 (section 6.4); never spent or lost
	UPROPERTY(BlueprintReadOnly) int32 Criterio = 0;

	virtual void BeginPlay() override;
	// Income and fines; counted in the day's "money earned"
	UFUNCTION(BlueprintCallable) void AddMoney(int32 Delta);
	// Purchases (not counted as earnings)
	UFUNCTION(BlueprintCallable) bool SpendMoney(int32 Amount);
	UPROPERTY(BlueprintReadOnly) int32 DayEarned = 0;
	UFUNCTION(BlueprintCallable) void AddCriterio(int32 Amount) { Criterio += Amount; }
	UPROPERTY(BlueprintReadOnly) bool bGameIsOver = false;
	UPROPERTY(BlueprintReadOnly) bool bWon = false;

	// Rent for a day from DT_DayProgression; after the last row it grows by RentIncreaseAfterLastDay per day.
	UFUNCTION(BlueprintCallable) int32 GetRentForDay(int32 DayIndex) const;
	// Charges the night's rent. Returns false (and fires OnGameOver) if Money < rent.
	UFUNCTION(BlueprintCallable) bool ChargeRent(int32 DayIndex);
};

UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_DayNightCycle : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnPhaseChanged OnPhaseChanged;
	UPROPERTY(BlueprintAssignable) FOnPhaseTimeTick OnPhaseTimeTick;

	UPROPERTY(BlueprintAssignable) FOnDayClosed OnDayClosed;

	UPROPERTY(BlueprintReadOnly) int32 DayIndex = 1;
	UPROPERTY(BlueprintReadOnly) EPhase CurrentPhase = EPhase::Day;
	UPROPERTY(BlueprintReadOnly) float PhaseElapsed = 0.f;
	// False while the day summary is open or the game has ended
	UPROPERTY(BlueprintReadOnly) bool bRunning = false;

	UAC_DayNightCycle() { PrimaryComponentTick.bCanEverTick = true; }
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable) void StartDay(int32 Day);
	// "Open the store" in WBP_DaySummary
	UFUNCTION(BlueprintCallable) void StartNextDay() { StartDay(DayIndex + 1); }
	UFUNCTION(BlueprintCallable) float GetPhaseDuration() const;
	UFUNCTION(BlueprintCallable) float GetSecondsLeft() const { return FMath::Max(0.f, GetPhaseDuration() - PhaseElapsed); }
	// Fictional store time, e.g. "14:30"
	UFUNCTION(BlueprintCallable) FText GetClockText() const;

private:
	void BeginNight();
	void CloseDay();
	void ApplyLighting(float NightAlpha);
};

// TODO(DESIGN): the doc doesn't say which component owns OnReviewReceived; placed here since the director schedules reviews.
UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_ClientDirector : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnClientReachedCounter OnClientReachedCounter;
	UPROPERTY(BlueprintAssignable) FOnClientLeft OnClientLeft;
	UPROPERTY(BlueprintAssignable) FOnReviewReceived OnReviewReceived;

	UAC_ClientDirector();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Called by the day cycle at the start of each phase: builds the spawn schedule (section 7.2).
	void BeginPhase(EPhase Phase, int32 DayIndex);
	// Store closing: everyone leaves with no penalty and pending reviews are applied immediately (section 8).
	void CloseStore();
	FDaySummary GetDayStats() const { return Stats; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<class ADCClient> ClientClass;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TutorialArchetypeRow = "TutorialClient";

	// Spawns at the door with the order of 5.2: the next DT_ScriptedOrders row if any is left (it also fixes the archetype),
	// otherwise a level and recipe drawn from what the player has unlocked.
	// Returns null if the store is full or the level lacks a door/queue.
	UFUNCTION(BlueprintCallable) class ADCClient* SpawnClient(FName ArchetypeRow);
	UFUNCTION(BlueprintCallable) class ADCClient* GetFrontClient() const;
	// Delivers the film in hand to the front client: base price now, tip/fine with the delayed review.
	UFUNCTION(BlueprintCallable) bool DeliverFilm(class ADCClient* Client);
	// Clears the "no repeated hint line today" memory.
	UFUNCTION(BlueprintCallable) void ResetDayLines() { UsedHintRows.Empty(); }
	// The next client gets DT_ScriptedOrders row 1 again (tests use it: the schedule may have spawned one before they start).
	UFUNCTION(BlueprintCallable) void ResetScriptedOrders() { ScriptedUsed = 0; }

	void ApplyLeavePenalty();
	void NotifyReachedCounter(class ADCClient* Client);
	void NotifyLeft(class ADCClient* Client, bool bServed);
	float ComputePatienceSec() const;
	int32 ActiveClientCount() const;

private:
	class ADCClientQueue* FindQueue() const;
	const struct FClientTierData* FindTier(int32 Tier) const;
	FText PickLine(const UDataTable* Table, ESpawnPhase Phase) const;
	// 5.2 steps 3 to 5: level, recipe and the Target it adds up to. Fills Tier, Target and HintedSkills.
	void GenerateOrder(const struct FClientArchetypeData& Arch, const struct FScriptedOrderData* Scripted, const struct FCustomerData*& OutPersona, FClientRuntime& Rt);
	// 5.4: which skills the dialogue hints at, and how strongly
	TArray<FHintedSkill> BuildHints(const FSkillVector& Target, int32 MaxHinted) const;
	FText BuildOrderText(const TArray<FHintedSkill>& Hints, ESpawnPhase Phase);

	bool TrySpawnScheduled();
	void ApplyReview(const FReview& Review);

	struct FPendingReview { int32 Id = 0; FTimerHandle Handle; FReview Review; };
	int32 NextReviewId = 0;

	TSet<FName> UsedHintRows;
	TMap<TWeakObjectPtr<class ADCClient>, FReview> PendingReviews;
	TArray<FPendingReview> PendingTimers;
	TArray<TWeakObjectPtr<class ADCClient>> Clients;

	TArray<float> SpawnTimes; // seconds from phase start, ascending
	int32 NextSpawn = 0;
	float PhaseTime = 0.f;
	EPhase SchedulePhase = EPhase::Day;
	int32 ScheduleDay = 1;
	int32 ScriptedUsed = 0; // DT_ScriptedOrders rows already given out this run
	FDaySummary Stats;
};

// What the player has unlocked from the computer (section 7.1): genres, VHS player slots and the star level.
// The client director and the supplier read it; every purchase is a DT_Unlocks row bought once.
UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_Unlocks : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnUnlockPurchased OnUnlockPurchased;

	UPROPERTY(BlueprintReadOnly) int32 UnlockedStarTier = 1;
	UPROPERTY(BlueprintReadOnly) TSet<EGenre> UnlockedGenres;
	// DT_Unlocks rows already bought
	UPROPERTY(BlueprintReadOnly) TSet<FName> OwnedUnlocks;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable) bool IsGenreUnlocked(EGenre Genre) const { return UnlockedGenres.Contains(Genre); }
	UFUNCTION(BlueprintCallable) bool IsOwned(FName UnlockId) const { return OwnedUnlocks.Contains(UnlockId); }
	// Genres unlocked, not counting the retired Paranormal one (the premium genres ask for PremiumMinGenres)
	UFUNCTION(BlueprintCallable) int32 GetGenreCount() const;
	// Not owned, requirements met (other unlocks, minimum genres) and enough money
	UFUNCTION(BlueprintCallable) bool CanBuyUnlock(FName UnlockId) const;
	// Spends the price and applies the effect: a genre, one more VHS player slot or the next star level
	UFUNCTION(BlueprintCallable) bool BuyUnlock(FName UnlockId);

private:
	const FUnlockData* FindUnlock(FName UnlockId) const;
};

UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_Inventory : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnInventoryChanged OnInventoryChanged;

	// v2 (section 4.3): copies of each DT_VHS row. Tapes are consumed when a film is combined.
	UPROPERTY(BlueprintReadOnly) TMap<FName, int32> Stock;
	// DEPRECATED v1 (step 8): tapes carried from the shelves. Nothing fills it any more; the HUD hides it.
	UPROPERTY(BlueprintReadOnly) TArray<FName> Hotbar;
	UPROPERTY(BlueprintReadOnly) int32 SelectedHotbar = 0;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable) int32 GetCount(FName Row) const { const int32* N = Stock.Find(Row); return N ? *N : 0; }
	UFUNCTION(BlueprintCallable) bool HasVHS(FName Row) const { return GetCount(Row) > 0; }
	UFUNCTION(BlueprintCallable) void AddTape(FName Row, int32 Count = 1);
	// Spends copies; false (and nothing spent) when there are fewer than Count.
	UFUNCTION(BlueprintCallable) bool ConsumeTape(FName Row, int32 Count = 1);
	// Rows with at least one copy, sorted by name
	UFUNCTION(BlueprintCallable) TArray<FName> GetOwnedVHS() const;
	UFUNCTION(BlueprintCallable) bool IsCarried(FName Row) const { return Hotbar.Contains(Row); }
	// DEPRECATED v1 (step 8): puts an owned tape in the first free hotbar slot.
	UFUNCTION(BlueprintCallable) bool TakeTape(FName Row);
	UFUNCTION(BlueprintCallable) void ReturnTape(FName Row);
	UFUNCTION(BlueprintCallable) TArray<FName> GetOwnedVHSByGenre(EGenre Genre) const;
	UFUNCTION(BlueprintCallable) bool GetVHSData(FName Row, FVHSData& Out) const;
	// Supplier (section 7.1): price of one copy by the tape's stars, TapePriceByStar[Rating - 1]
	UFUNCTION(BlueprintCallable) int32 GetTapePrice(FName Row) const;
	// For sale: its genre is unlocked and its stars are within UnlockedStarTier
	UFUNCTION(BlueprintCallable) bool IsTapeForSale(FName Row) const;
	// For sale and enough money for Count copies
	UFUNCTION(BlueprintCallable) bool CanBuyTape(FName Row, int32 Count = 1) const;
	UFUNCTION(BlueprintCallable) bool BuyTape(FName Row, int32 Count = 1);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFilmStateChanged);

UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_FilmCrafting : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnFilmCreated OnFilmCreated;
	// Fires on any slot or held-film change
	UPROPERTY(BlueprintAssignable) FOnFilmStateChanged OnFilmStateChanged;

	// Slots of the VHS player (section 4.4). The step 7 unlocks component raises UnlockedSlots with SetUnlockedSlots.
	UPROPERTY(BlueprintReadOnly) int32 UnlockedSlots = 3;
	// One DT_VHS row per slot, NAME_None = empty; always UnlockedSlots long
	UPROPERTY(BlueprintReadOnly) TArray<FName> SlotRows;
	UPROPERTY(BlueprintReadOnly) TArray<FCreatedFilm> HeldFilms;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable) void SetUnlockedSlots(int32 Count);
	// Copies of a tape still free to put in a slot: owned minus the ones already in slots
	UFUNCTION(BlueprintCallable) int32 GetAvailableCount(FName VHSRow) const;
	// Empty slots and repeated tapes are fine; false for a bad index or when no copy is left.
	UFUNCTION(BlueprintCallable) bool SetSlot(int32 Index, FName VHSRow);
	UFUNCTION(BlueprintCallable) void ClearSlot(int32 Index);
	UFUNCTION(BlueprintCallable) int32 GetFilledSlots() const;
	// Live sum of the tapes in the slots (4.4 rule 4)
	UFUNCTION(BlueprintCallable) FSkillVector GetProfile() const;
	UFUNCTION(BlueprintCallable) bool CanConfirm() const;
	// Spends one copy per filled slot and puts the film in hand.
	UFUNCTION(BlueprintCallable) bool ConfirmFilm(FText Title);
	// Gives the held film's tapes back to the inventory (4.4 rule 8).
	UFUNCTION(BlueprintCallable) void DiscardHeldFilm();
	// The film leaves the hand at delivery: the tapes stay spent.
	void ConsumeHeldFilm();

private:
	int32 FilmsCreated = 0;
};
