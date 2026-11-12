#include "DestructiblePillar.h"
#include "Components/StaticMeshComponent.h"

ADestructiblePillar::ADestructiblePillar()
{
	PrimaryActorTick.bCanEverTick = false;

	PillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PillarMesh"));
	RootComponent = PillarMesh;
	PillarMesh->SetCollisionProfileName(TEXT("BlockAll"));

	RubbleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RubbleMesh"));
	RubbleMesh->SetupAttachment(RootComponent);
	RubbleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RubbleMesh->SetVisibility(false);
}

void ADestructiblePillar::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
}

float ADestructiblePillar::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDestroyed) return 0.f;

	CurrentHealth -= DamageAmount;
	if (CurrentHealth <= 0.f)
	{
		bDestroyed = true;
		PillarMesh->SetVisibility(false);
		PillarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		RubbleMesh->SetVisibility(true);
		RubbleMesh->SetCollisionProfileName(TEXT("BlockAll"));

		OnPillarDestroyed(); // Spawn dust/debris Niagara system and camera shake in BP
	}

	return DamageAmount;
}