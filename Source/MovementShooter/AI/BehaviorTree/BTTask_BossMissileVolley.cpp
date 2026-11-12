#include "MovementShooter/AI/BehaviorTree/BTTask_BossMissileVolley.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/SpellAbility/Projectile/MicroMissileProjectile.h"

UBTTask_BossMissileVolley::UBTTask_BossMissileVolley()
{
	NodeName = TEXT("Boss Missile Volley");
	bCreateNodeInstance = true; 
}

EBTNodeResult::Type UBTTask_BossMissileVolley::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!BB || !AICon) return EBTNodeResult::Failed;

	CachedBoss = Cast<ABaseCharacter>(AICon->GetPawn());
	CachedTarget = Cast<ABaseCharacter>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	// Same fallback the Minigun Sweep uses: if the key isn't set on the node, use the controller's TargetActor
	if (!CachedTarget)
	{
		CachedTarget = Cast<ABaseCharacter>(BB->GetValueAsObject(AEnemyAIController::BB_TargetActor));
	}
	CachedOwnerComp = &OwnerComp;

	if (!CachedBoss || !CachedTarget) return EBTNodeResult::Failed;

	CurrentMissileIndex = 0;

	// 1. Instantly face the Target
	FVector Direction = (CachedTarget->GetActorLocation() - CachedBoss->GetActorLocation()).GetSafeNormal();
	AICon->SetControlRotation(Direction.Rotation());

	if (ABoss_Jetpack* JetpackBoss = Cast<ABoss_Jetpack>(CachedBoss))
	{
		JetpackBoss->OnMissileLockWarning(CachedTarget);
	}
	
	// 2. Start the firing sequence
	CachedBoss->GetWorldTimerManager().SetTimer(LaunchTimer, this, &UBTTask_BossMissileVolley::FireNextMissile, InitialLaunchDelay, false);

	return EBTNodeResult::InProgress;
}

void UBTTask_BossMissileVolley::FireNextMissile()
{
	if (!CachedBoss || !CachedTarget || !MissileClass)
	{
		FinishVolley();
		return;
	}

	float HorizontalOffset = (CurrentMissileIndex % 2 == 0) ? -40.f : 40.f;
	FVector SpawnLoc = CachedBoss->GetActorLocation() + FVector(0.f, 0.f, 100.f) + (CachedBoss->GetActorForwardVector() * 30.f) + (CachedBoss->GetActorRightVector() * HorizontalOffset);
	
	FTransform SpawnTransform(CachedBoss->GetActorRotation(), SpawnLoc);
	AMicroMissileProjectile* Missile = CachedBoss->GetWorld()->SpawnActorDeferred<AMicroMissileProjectile>(MissileClass, SpawnTransform);

	if (Missile)
	{
		Missile->Initialize(CachedBoss, CachedTarget);
		Missile->FinishSpawning(SpawnTransform);
	}

	CurrentMissileIndex++;

	if (CurrentMissileIndex >= MissilesToFire)
	{
		CachedBoss->GetWorldTimerManager().SetTimer(LaunchTimer, this, &UBTTask_BossMissileVolley::FinishVolley, 1.0f, false);
	}
	else
	{
		CachedBoss->GetWorldTimerManager().SetTimer(LaunchTimer, this, &UBTTask_BossMissileVolley::FireNextMissile, TimeBetweenMissiles, false);
	}
}

void UBTTask_BossMissileVolley::FinishVolley()
{
	if (CachedOwnerComp)
	{
		FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
	}
	CachedOwnerComp = nullptr;
	CachedBoss = nullptr;
	CachedTarget = nullptr;
}

EBTNodeResult::Type UBTTask_BossMissileVolley::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (CachedBoss)
	{
		CachedBoss->GetWorldTimerManager().ClearTimer(LaunchTimer);
	}
	CachedOwnerComp = nullptr;
	CachedBoss = nullptr;
	CachedTarget = nullptr;
	return EBTNodeResult::Aborted;
}

void UBTTask_BossMissileVolley::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	if (TaskResult == EBTNodeResult::Succeeded) return;

	// The volley sits between Takeoff and Glide/Land. If it is aborted or fails (tree restart, target lost),
	// the Land task never runs and the boss would stay frozen in MOVE_Flying (no gravity). Drop him.
	// Uses the controller's pawn because AbortTask/FinishVolley clear CachedBoss before this runs.
	AAIController* AICon = OwnerComp.GetAIOwner();
	ABoss_Jetpack* JetpackBoss = AICon ? Cast<ABoss_Jetpack>(AICon->GetPawn()) : nullptr;
	if (JetpackBoss && !JetpackBoss->IsHovering())
	{
		UCharacterMovementComponent* Move = JetpackBoss->GetCharacterMovement();
		if (Move && Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Falling);
			JetpackBoss->OnHoverStateChanged(false, false);
		}
	}
}