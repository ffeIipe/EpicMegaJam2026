#include "Core/DCInteractable.h"
#include "Core/DCGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/DCHudWidgets.h"
#include "Data/PDA_GameConfig.h"
#include "Engine/DataTable.h"
#include "UObject/ConstructorHelpers.h"

ADCInteractable::ADCInteractable()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAll")); // blocks the Visibility trace used for clicking
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	MoneyDisplay = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MoneyDisplay"));
	MoneyDisplay->SetupAttachment(Mesh);
	MoneyDisplay->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	MoneyDisplay->SetHorizontalAlignment(EHTA_Center);
	MoneyDisplay->SetWorldSize(40.f);
	MoneyDisplay->SetText(FText::GetEmpty());
	MoneyDisplay->SetVisibility(false);
	LabelWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("LabelWidget"));
	LabelWidget->SetupAttachment(MoneyDisplay);
	LabelWidget->SetWidgetClass(UDCWorldLabelWidget::StaticClass());
	LabelWidget->SetWidgetSpace(EWidgetSpace::World);
	LabelWidget->SetDrawAtDesiredSize(true);
	LabelWidget->SetRelativeScale3D(FVector(0.5f)); // widget pixels are world units: half size keeps neighbouring shelf labels apart
	LabelWidget->SetTwoSided(true);
	LabelWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // only while the computer has news
}

void ADCInteractable::BeginPlay()
{
	Super::BeginPlay();
	BaseRotation = GetActorRotation();
	// The money and the prop names live in the HUD now: no floating labels (the component stays so placed actors don't change)
	LabelWidget->SetVisibility(false);
	LabelWidget->SetHiddenInGame(true);
	if (InteractType == EInteractType::Computer)
	{
		if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
		{
			GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCInteractable::OnMoneyChanged);
			GM->Unlocks->OnUnlockPurchased.AddDynamic(this, &ADCInteractable::CheckNews);
		}
	}
	if (InteractType != EInteractType::CashRegister)
	{
		SetLabelText(GetLabel());
		return;
	}
	if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
	{
		GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCInteractable::OnMoneyChanged);
		OnMoneyChanged(GM->Economy->Money, 0);
	}
}

FText ADCInteractable::GetLabel() const
{
	// TODO(DESIGN): placeholder wording; the doc only asks for an outline on aimed props
	switch (InteractType)
	{
	case EInteractType::CashRegister: return FText::FromString(TEXT("Supplier: buy tapes"));
	case EInteractType::Computer: return FText::FromString(TEXT("Computer: unlocks"));
	default: return FText::GetEmpty(); // shelves and the VHS player are scenery since v2.1
	}
}

void ADCInteractable::OnMoneyChanged(int32 NewMoney, int32 Delta)
{
	if (InteractType == EInteractType::Computer)
	{
		CheckNews(NAME_None);
		return;
	}
	SetLabelText(FText::FromString(FString::Printf(TEXT("$ %d"), NewMoney)));
	if (Delta != 0)
	{
		if (UDCWorldLabelWidget* Widget = Cast<UDCWorldLabelWidget>(LabelWidget->GetUserWidgetObject()))
		{
			Widget->PunchMoney(Delta);
		}
		if (Delta > 0)
		{
			ShakeLeft = 0.5f;
			SetActorTickEnabled(true);
		}
	}
}

void ADCInteractable::CheckNews(FName UnlockId)
{
	const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>();
	if (!GM || !GM->Config || !GM->Config->UnlockTable)
	{
		return;
	}
	const bool bWasHasNews = bHasNews;
	for (const FName& Id : GM->Config->UnlockTable->GetRowNames())
	{
		if (GM->Unlocks->CanBuyUnlock(Id) && !NotifiedUnlocks.Contains(Id))
		{
			NotifiedUnlocks.Add(Id);
			bHasNews = true;
			SetActorTickEnabled(true);
		}
	}
	if (!bWasHasNews && bHasNews)
	{
		ADCAudioManager::PlayCueAtLocationStatic(this, TEXT("ComputerNotification"), GetActorLocation());
	}
}

void ADCInteractable::ClearNews()
{
	bHasNews = false;
	SetActorTickEnabled(false);
	SetActorRotation(BaseRotation);
	SetLabelText(GetLabel());
}

void ADCInteractable::SetHighlighted(bool bOn) const
{
	Mesh->SetCustomDepthStencilValue(1);
	Mesh->SetRenderCustomDepth(bOn);
}

void ADCInteractable::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float T = GetWorld()->GetTimeSeconds();
	if (ShakeLeft > 0.f)
	{
		ShakeLeft = FMath::Max(0.f, ShakeLeft - DeltaTime);
		const float Fade = ShakeLeft / 0.5f;
		SetActorRotation(BaseRotation + FRotator(Fade * FMath::Sin(T * 55.f) * 2.f, 0.f, Fade * FMath::Sin(T * 47.f) * 4.f));
		if (ShakeLeft <= 0.f)
		{
			SetActorRotation(BaseRotation);
			SetActorTickEnabled(false);
		}
		return;
	}
	if (!bHasNews)
	{
		SetActorTickEnabled(false);
		return;
	}
	// A 0.7 s shake every 2.2 s, fading in and out, so it reads as "hey" and not as a glitch
	const float Phase = FMath::Fmod(T, 2.2f);
	const float Envelope = Phase < 0.7f ? FMath::Sin(Phase / 0.7f * PI) : 0.f;
	SetActorRotation(BaseRotation + FRotator(Envelope * FMath::Sin(T * 40.f) * 1.5f, 0.f, Envelope * FMath::Sin(T * 33.f) * 3.f));
	// Sparkle: the label blinks between two star patterns
	SetLabelText(FText::FromString(FMath::Fmod(T, 0.5f) < 0.25f ? TEXT("★ NEW ★") : TEXT("* NEW *")));
}

void ADCInteractable::SetLabelText(const FText& Text) const
{
	if (UDCWorldLabelWidget* Widget = Cast<UDCWorldLabelWidget>(LabelWidget->GetUserWidgetObject()))
	{
		Widget->SetLabel(Text);
	}
}
