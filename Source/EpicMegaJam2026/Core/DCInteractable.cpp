#include "Core/DCInteractable.h"
#include "Core/DCGameMode.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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
}

void ADCInteractable::BeginPlay()
{
	Super::BeginPlay();
	if (InteractType != EInteractType::CashRegister)
	{
		MoneyDisplay->DestroyComponent();
		MoneyDisplay = nullptr;
		return;
	}
	if (const ADCGameMode* GM = GetWorld()->GetAuthGameMode<ADCGameMode>())
	{
		GM->Economy->OnMoneyChanged.AddDynamic(this, &ADCInteractable::OnMoneyChanged);
		OnMoneyChanged(GM->Economy->Money, 0);
	}
}

void ADCInteractable::OnMoneyChanged(int32 NewMoney, int32 Delta)
{
	if (MoneyDisplay)
	{
		MoneyDisplay->SetText(FText::FromString(FString::Printf(TEXT("$%d"), NewMoney)));
	}
}
