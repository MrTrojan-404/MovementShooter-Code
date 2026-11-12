
#include "MovementShooter/AI/Character/HeavyEnemy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"

AHeavyEnemy::AHeavyEnemy()
{
	// 1. FLIP THE HEAVY FLAG! This tells all your spells to use the "Heavy" math
	bIsHeavyEnemy = true;

	// 2. Heavy Stats
	MaxHealth = 300.f;
	Health = 300.f;

	SightRadius = 3000.f;
	HearingRange = 4000.f;

	// Heavies usually just walk straight at you slowly and fire
	CombatStyle = 3; // Heavy
}

void AHeavyEnemy::BeginPlay()
{
	Super::BeginPlay();

	if (AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AIController->GetBlackboardComponent())
		{
			BB->SetValueAsInt(FName("EnemyType"), GetEnemyType());
			BB->SetValueAsFloat(FName("PreferredRange"), HeavyEngagementRange);
		}
	}
}

