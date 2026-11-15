// Centralized FMOD audio configuration Data Asset. Every gameplay, environment, client and UI cue lives here.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FMODEvent.h"
#include "PDA_AudioConfig.generated.h"

UCLASS(BlueprintType)
class EPICMEGAJAM2026_API UPDA_AudioConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// --- Ambience & Music
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> Music;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> MainMenuMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> DayMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> NightMusic;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> CounterAmbience;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> WarehouseAmbience;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambience & Music") TObjectPtr<UFMODEvent> LightBuzz;

	// --- Environment & Doors
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> DoorbellRing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> StoreDoorOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> StoreDoorClose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> WarehouseDoorOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> WarehouseDoorClose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> OpenSupplier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> CloseSupplier;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> TurnOnComputer;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment & Doors") TObjectPtr<UFMODEvent> TurnOffComputer;

	// --- Clients
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> Footsteps;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> FemaleCustomerTalk;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> MaleCustomerTalk;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> ClientAtCounter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> DialogueOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> OrderAccepted;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> OrderRejected;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> Delivered;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients") TObjectPtr<UFMODEvent> ClientAngryLeave;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clients", meta = (ClampMin = "0.1")) float FootstepIntervalSec = 0.48f;

	// --- Crafting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TObjectPtr<UFMODEvent> FilmSelect;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TObjectPtr<UFMODEvent> FilmPlace;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TObjectPtr<UFMODEvent> FilmRemove;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TObjectPtr<UFMODEvent> FilmCreated;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TObjectPtr<UFMODEvent> FilmDiscard;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crafting") TObjectPtr<UFMODEvent> SlotFull;

	// --- Economy & Computer
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> MoneyUp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> MoneyDown;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> BuyFilm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> SellFilm;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> TapePurchased;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> UnlockPurchased;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> ComputerNotification;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> RentCharged;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> GoodReview;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> BadReview;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> ReviewLoved;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> ReviewLiked;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> ReviewNeutral;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy & Computer") TObjectPtr<UFMODEvent> ReviewHated;

	// --- UI & Flow
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> MenuClick;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> MenuHover;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> Notification;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> TabSwitch;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> PauseOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> ScreenClose;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> MainMenuStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> IntroStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> IntroSkip;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> DayStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> NightStart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> DaySummaryOpen;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> OpenStore;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> TutorialStep;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> TutorialComplete;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> GameOver;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI & Flow") TObjectPtr<UFMODEvent> Victory;

	// --- Custom fallback cues by name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Custom") TMap<FName, TObjectPtr<UFMODEvent>> CustomCues;

	UPDA_AudioConfig();
	virtual void PostLoad() override;

	/** Resolves a cue name (including legacy aliases) to its FMOD event, falling back to CustomCues. */
	UFUNCTION(BlueprintPure, Category = "Audio")
	UFMODEvent* ResolveCue(FName Name) const;
};
