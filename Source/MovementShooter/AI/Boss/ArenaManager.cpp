
#include "MovementShooter/AI/Boss/ArenaManager.h"

#include "Components/BoxComponent.h"
#include "MovementShooter/AI/Boss/BossBase.h"
#include "MovementShooter/AI/Boss/BossController.h"

#include "MovementShooter/Actor/Turret/LaserTurret.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/Character/PlayerCharacter.h"

AArenaManager::AArenaManager()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentPhase = 0;

	ArenaTriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaTriggerBox"));
	RootComponent = ArenaTriggerBox;
	ArenaTriggerBox->SetBoxExtent(FVector(500.f, 500.f, 250.f));
	ArenaTriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ArenaTriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ArenaTriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AArenaManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && ArenaTriggerBox)
	{
		ArenaTriggerBox->OnComponentBeginOverlap.AddDynamic(this, &AArenaManager::OnArenaTriggerOverlap);
	}
}


void AArenaManager::StartBossFight(AActor* InstigatorPlayer)
{
	if (!InstigatorPlayer) return;

	// Lock Bosses onto the player
	for (ABossBase* Boss : ArenaBosses)
	{
		if (Boss)
		{
			if (ABossController* BossController = Cast<ABossController>(Boss->GetController()))
			{
				BossController->LockOntoTarget(InstigatorPlayer);
			}
		}
	}

	// Grab EVERY character in the world (Players AND AI Companions)
	TArray<AActor*> FoundCharacters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseCharacter::StaticClass(), FoundCharacters);
	
	TArray<ABaseCharacter*> ValidArenaTargets;
	for (AActor* Actor : FoundCharacters)
	{
		// Add them to the target list as long as they aren't the boss!
		if (!Actor->IsA(ABossBase::StaticClass()))
		{
			if (ABaseCharacter* Char = Cast<ABaseCharacter>(Actor))
			{
				ValidArenaTargets.Add(Char);
			}
		}
	}

	// Hand the complete list of targets to every Turret!
	if (ValidArenaTargets.Num() > 0)
	{
		for (ALaserTurret* Turret : InnerTurrets)
		{
			if (Turret)
			{
				Turret->ActivateTurret(ValidArenaTargets);
			}
		}
	}

	BlueprintStartFight();
}

void AArenaManager::AdvancePhase()
{
	CurrentPhase++;
	
	// Broadcast to anyone listening (e.g., UI Healthbars)
	OnPhaseChanged.Broadcast();

	// Trigger the Blueprint event for specific phase logic (e.g., dropping Commander 2)
	BlueprintTriggerPhase(CurrentPhase);
}

void AArenaManager::OnArenaTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor))
	{
		// Disable collision immediately so the fight only starts once
		ArenaTriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		StartBossFight(Player);
	}
}

