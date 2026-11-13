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

	// Spawns at the door with attributes per 5.2. Category is chosen by the caller (day-based choice comes with step 7).
	// Returns null if the store is full or the level lacks a door/queue.
	UFUNCTION(BlueprintCallable) class ADCClient* SpawnClient(FName ArchetypeRow, EClientCategory Category);
	UFUNCTION(BlueprintCallable) class ADCClient* GetFrontClient() const;
	// Delivers the film in hand to the front client: base price now, tip/fine with the delayed review.
	UFUNCTION(BlueprintCallable) bool DeliverFilm(class ADCClient* Client);
	// Clears the "no repeated hint line today" memory.
	UFUNCTION(BlueprintCallable) void ResetDayLines() { UsedHintRows.Empty(); }

	void ApplyLeavePenalty();
	void NotifyReachedCounter(class ADCClient* Client);
	void NotifyLeft(class ADCClient* Client, bool bServed);
	float ComputePatienceSec() const;
	int32 ActiveClientCount() const;

private:
	class ADCClientQueue* FindQueue() const;
	float CategoryMultiplier(EClientCategory Category) const;
	FText PickLine(const UDataTable* Table, ESpawnPhase Phase) const;
	FText BuildOrderText(const TArray<FGenrePreference>& Prefs, ESpawnPhase Phase);

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
	bool bTutorialSpawned = false;
	FDaySummary Stats;
};

UCLASS(ClassGroup = DirectorsCut, Blueprintable, meta = (BlueprintSpawnableComponent))
class EPICMEGAJAM2026_API UAC_Inventory : public UActorComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FOnInventoryChanged OnInventoryChanged;

	// DT_VHS row names the player owns. Tapes are never consumed.
	UPROPERTY(BlueprintReadOnly) TArray<FName> OwnedVHS;
	UPROPERTY(BlueprintReadOnly) TArray<FName> OwnedCatalogs;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable) bool HasVHS(FName Row) const { return OwnedVHS.Contains(Row); }
	UFUNCTION(BlueprintCallable) TArray<FName> GetOwnedVHSByGenre(EGenre Genre) const;
	UFUNCTION(BlueprintCallable) bool GetVHSData(FName Row, FVHSData& Out) const;
	UFUNCTION(BlueprintCallable) void AddCatalog(FName CatalogRow);
	// Buys a catalog from the supplier: needs UnlockDay reached, not owned, enough money.
	UFUNCTION(BlueprintCallable) bool BuyCatalog(FName CatalogRow);
	UFUNCTION(BlueprintCallable) bool CanBuyCatalog(FName CatalogRow) const;
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

	UPROPERTY(BlueprintReadOnly) TMap<EFilmSlot, FName> Slots;
	UPROPERTY(BlueprintReadOnly) TArray<FCreatedFilm> HeldFilms;

	// Rejects tapes not owned or already used in another slot.
	UFUNCTION(BlueprintCallable) bool SetSlot(EFilmSlot Slot, FName VHSRow);
	UFUNCTION(BlueprintCallable) void ClearSlot(EFilmSlot Slot);
	UFUNCTION(BlueprintCallable) bool AreSlotsFull() const;
	UFUNCTION(BlueprintCallable) bool CanConfirm() const;
	UFUNCTION(BlueprintCallable) bool ConfirmFilm(FText Title);
	UFUNCTION(BlueprintCallable) void DiscardHeldFilm();

private:
	int32 FilmsCreated = 0;
};
