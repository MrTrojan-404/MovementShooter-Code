#include "MovementShooter/AI/Boss/BossController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MovementShooter/AI/Boss/BossBase.h"
#include "TimerManager.h"
#include "Perception/AIPerceptionComponent.h"

void ABossController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		// Prevent the player from shooting the boss to death from outside the arena!
		InPawn->SetCanBeDamaged(false);
	}

	// 1. Put the Boss's eyes and ears to sleep so it cannot detect the player early
	if (AIPerception)
	{
		AIPerception->Deactivate();
	}

	// 2. Stop the manual target sweeping timer from running in the background
	GetWorldTimerManager().ClearTimer(TargetSweepTimer);
}

void ABossController::LockOntoTarget(AActor* NewTarget)
{
	if (!NewTarget) return;

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->SetCanBeDamaged(true);
	}

	// WAKE UP: Reactivate perception and sweeping!
	if (AIPerception)
	{
		AIPerception->Activate();
	}

	if (!GetWorldTimerManager().IsTimerActive(TargetSweepTimer))
	{
		GetWorldTimerManager().SetTimer(TargetSweepTimer, this, &AEnemyAIController::PerformTargetSweep, 0.2f, true);
	}

	if (!GetBlackboardComponent())
	{
		if (ABossBase* BossPawn = Cast<ABossBase>(GetPawn()))
		{
			if (BossPawn->GetBossBehaviorTree())
			{
				RunBehaviorTree(BossPawn->GetBossBehaviorTree());
			}
		}
	}

	if (GetBlackboardComponent())
	{
		SetAlertPhase(EAlertPhase::Alert);
		SetTargetActor(NewTarget);
		SetAIState(EAIState::Engage);
	}
	else
	{
		FTimerHandle RetryTimer;
		GetWorldTimerManager().SetTimer(RetryTimer, [this, NewTarget]()
		{
			LockOntoTarget(NewTarget);
		}, 0.1f, false);
	}
}