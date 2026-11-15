// Centralized FMOD audio configuration Data Asset. Every gameplay, environment, client and UI cue lives here.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FMODEvent.h"
#include "PDA_AudioConfig.generated.h"

USTRUCT(BlueprintType)
struct EPICMEGAJAM2026_API FDCAudioCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<UFMODEvent> Event;

	// Optional FMOD parameters (Name -> Value) applied when this cue plays.
	// Automatically routed to FMOD Studio System (global) or EventInstance (local).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TMap<FName, float> Parameters;

	FDCAudioCue() = default;
	FDCAudioCue(UFMODEvent* InEvent) : Event(InEvent) {}

	UFMODEvent* Get() const { return Event.Get(); }
	bool IsValid() const { return Event != nullptr; }
	explicit operator bool() const { return Event != nullptr; }
};

UCLASS(BlueprintType)
class EPICMEGAJAM2026_API UPDA_AudioConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- Ambience & Music
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue Music;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue MainMenuMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue DayMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue NightMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue CounterAmbience;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue WarehouseAmbience;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue LightBuzz;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") FDCAudioCue PauseOrDialogue;

	// --- Environment & Doors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue DoorbellRing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue StoreDoorOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue StoreDoorClose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue WarehouseDoorOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue WarehouseDoorClose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue OpenSupplier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue CloseSupplier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue TurnOnComputer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") FDCAudioCue TurnOffComputer;

	// --- Clients
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue Footsteps;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue FemaleCustomerTalk;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue MaleCustomerTalk;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue ClientAtCounter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue DialogueOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue OrderAccepted;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue OrderRejected;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue Delivered;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") FDCAudioCue ClientAngryLeave;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients", meta = (ClampMin = "0.1")) float FootstepIntervalSec = 0.48f;

	// --- Crafting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") FDCAudioCue FilmSelect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") FDCAudioCue FilmPlace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") FDCAudioCue FilmRemove;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") FDCAudioCue FilmCreated;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") FDCAudioCue FilmDiscard;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") FDCAudioCue SlotFull;

	// --- Economy & Computer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue MoneyUp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue MoneyDown;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue BuyFilm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue SellFilm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue TapePurchased;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue UnlockPurchased;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue ComputerNotification;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue RentCharged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue GoodReview;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue BadReview;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue ReviewLoved;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue ReviewLiked;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue ReviewNeutral;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") FDCAudioCue ReviewHated;

	// --- UI & Flow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue MenuClick;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue MenuHover;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue Notification;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue TabSwitch;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue PauseOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue ScreenClose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue MainMenuStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue IntroStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue IntroSkip;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue DayStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue NightStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue DaySummaryOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue OpenStore;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue TutorialStep;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue TutorialComplete;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue GameOver;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") FDCAudioCue Victory;

	// --- Custom fallback cues by name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom") TMap<FName, FDCAudioCue> CustomCues;

	UPDA_AudioConfig();
	virtual void PostLoad() override;

	/** Resolves a cue name (including legacy aliases) to its FMOD event, falling back to CustomCues. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	UFMODEvent* ResolveCue(FName Name) const;

	/** Resolves a cue name to its FDCAudioCue struct (Event + default Parameters), or nullptr if not found. */
	const FDCAudioCue* FindCue(FName Name) const;

	/** Blueprint-friendly helper to resolve a cue's FDCAudioCue struct (Event + default Parameters). */
	UFUNCTION(BlueprintPure, Category = "Audio")
	bool ResolveCueConfig(FName Name, FDCAudioCue& OutCue) const;
};
