
#include "MovementShooter/AI/Character/SniperEnemy.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"

ASniperEnemy::ASniperEnemy()
{
	// Extended perception
	SightRadius = 5000.f;
	HearingRange = 6000.f;
	EngagementRange = 5000.f;
	PreferredCombatDistance = 3000.f;
	MinCombatDistance = 1000.f;

	// Lower health — fragile at range
	MaxHealth = 75.f;
	Health = 75.f;

	// Slower attack
	AttackCooldown = 3.f;
}

void ASniperEnemy::BeginPlay()
{
	Super::BeginPlay();

	AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController());
	if (AIController && AIController->GetBlackboardComponent())
	{
		AIController->GetBlackboardComponent()->SetValueAsInt(FName("EnemyType"), 2);
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("AimDuration"), AimDuration);
		
		//Push PreferredCombatDistance to the Blackboard
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("PreferredRange"), PreferredCombatDistance);
	}
}

