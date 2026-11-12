
#include "MovementShooter/AI/Character/RangedEnemy.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"

ARangedEnemy::ARangedEnemy()
{
	SightRadius = 4500.f;
	HearingRange = 5500.f;
	EngagementRange = 3500.f;

	MaxHealth = 100.f;
	Health = 100.f;
}

void ARangedEnemy::BeginPlay()
{
	Super::BeginPlay();

	AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController());
	if (AIController && AIController->GetBlackboardComponent())
	{
		AIController->GetBlackboardComponent()->SetValueAsInt(FName("EnemyType"), 1);
		
		// Push EngagementRange to the Blackboard
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("PreferredRange"), EngagementRange);
	}
}

