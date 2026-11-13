#include "Components/DCComponents.h"
#include "Core/DCGameMode.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/LightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Kismet/GameplayStatics.h"

static ADCGameMode* GM_(const UActorComponent* C) { return Cast<ADCGameMode>(C->GetOwner()); }

void UAC_DayNightCycle::BeginPlay()
{
	Super::BeginPlay();
	StartDay(1);
}

float UAC_DayNightCycle::GetPhaseDuration() const
{
	const UPDA_GameConfig* Cfg = GM_(this)->Config;
	return CurrentPhase == EPhase::Day ? Cfg->DayDurationSec : Cfg->NightDurationSec;
}

void UAC_DayNightCycle::StartDay(int32 Day)
{
	ADCGameMode* GM = GM_(this);
	if (!GM || !GM->Config)
	{
		return;
	}
	DayIndex = Day;
	CurrentPhase = EPhase::Day;
	PhaseElapsed = 0.f;
	bRunning = true;
	GM->Economy->DayEarned = 0;
	GM->ClientDirector->ResetDayLines();
	GM->ClientDirector->BeginPhase(EPhase::Day, DayIndex);
	ApplyLighting(0.f);
	OnPhaseChanged.Broadcast(EPhase::Day, DayIndex);
}

void UAC_DayNightCycle::BeginNight()
{
	CurrentPhase = EPhase::Night;
	PhaseElapsed = 0.f;
	GM_(this)->ClientDirector->BeginPhase(EPhase::Night, DayIndex);
	OnPhaseChanged.Broadcast(EPhase::Night, DayIndex);
}

void UAC_DayNightCycle::CloseDay()
{
	ADCGameMode* GM = GM_(this);
	bRunning = false;
	GM->ClientDirector->CloseStore();
	const int32 Rent = GM->Economy->GetRentForDay(DayIndex);
	if (!GM->Economy->ChargeRent(DayIndex))
	{
		return; // game over (or victory reached by a flushed tip): the end screen takes over
	}
	FDaySummary S = GM->ClientDirector->GetDayStats();
	S.DayIndex = DayIndex;
	S.MoneyEarned = GM->Economy->DayEarned;
	S.RentPaid = Rent;
	S.MoneyNow = GM->Economy->Money;
	S.Goal = GM->Config->MovieBudgetGoal;
	OnDayClosed.Broadcast(S); // the summary screen pauses the game; StartNextDay resumes the cycle
}

void UAC_DayNightCycle::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const ADCGameMode* GM = GM_(this);
	if (!bRunning || !GM || !GM->Config)
	{
		return;
	}
	PhaseElapsed += DeltaTime;
	OnPhaseTimeTick.Broadcast(GetSecondsLeft());
	if (CurrentPhase == EPhase::Night)
	{
		const float T = GM->Config->NightTransitionSec;
		ApplyLighting(T > 0.f ? FMath::Clamp(PhaseElapsed / T, 0.f, 1.f) : 1.f);
	}
	if (PhaseElapsed >= GetPhaseDuration())
	{
		if (CurrentPhase == EPhase::Day)
		{
			BeginNight();
		}
		else
		{
			CloseDay();
		}
	}
}

void UAC_DayNightCycle::ApplyLighting(float NightAlpha)
{
	const UPDA_GameConfig* Cfg = GM_(this)->Config;
	const FLinearColor Color = FMath::Lerp(Cfg->DayLightColor, Cfg->NightLightColor, NightAlpha);
	const float Intensity = FMath::Lerp(Cfg->DayLightIntensity, Cfg->NightLightIntensity, NightAlpha);
	if (const ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(this, ADirectionalLight::StaticClass())))
	{
		Sun->GetLightComponent()->SetLightColor(Color);
		Sun->GetLightComponent()->SetIntensity(Intensity);
	}
	if (const ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(this, ASkyLight::StaticClass())))
	{
		Sky->GetLightComponent()->SetLightColor(Color.ToFColor(true));
		Sky->GetLightComponent()->SetIntensity(Intensity * Cfg->SkyLightIntensityRatio);
	}
}

FText UAC_DayNightCycle::GetClockText() const
{
	const UPDA_GameConfig* Cfg = GM_(this)->Config;
	const float Duration = GetPhaseDuration();
	const float F = Duration > 0.f ? FMath::Clamp(PhaseElapsed / Duration, 0.f, 1.f) : 0.f;
	const float Start = CurrentPhase == EPhase::Day ? Cfg->ClockDayStartHour : Cfg->ClockDayEndHour;
	float End = CurrentPhase == EPhase::Day ? Cfg->ClockDayEndHour : Cfg->ClockNightEndHour;
	if (End <= Start)
	{
		End += 24.f; // night runs past midnight
	}
	const int32 Minutes = FMath::FloorToInt(FMath::Lerp(Start, End, F) * 60.f) % (24 * 60);
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes / 60, Minutes % 60));
}
