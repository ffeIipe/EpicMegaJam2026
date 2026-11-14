#include "Core/DCInteractable.h"
#include "Core/DCGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "UI/DCHudWidgets.h"
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
}

void ADCInteractable::BeginPlay()
{
	Super::BeginPlay();
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
	SetLabelText(FText::FromString(FString::Printf(TEXT("$ %d"), NewMoney)));
}

void ADCInteractable::SetLabelText(const FText& Text) const
{
	if (UDCWorldLabelWidget* Widget = Cast<UDCWorldLabelWidget>(LabelWidget->GetUserWidgetObject()))
	{
		Widget->SetLabel(Text);
	}
}
