#include "BTTask_MinigunSweep.h"
#include "AIController.h"
#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/SpellAbility/Projectile/MicroMissileProjectile.h"

static void SetManualAim(AAIController* AICon, bool bEnable)
{
	if (AEnemyAIController* EnemyCon = Cast<AEnemyAIController>(AICon))
		EnemyCon->bManualControlRotation = bEnable;
}

UBTTask_MinigunSweep::UBTTask_MinigunSweep()
{
	NodeName = TEXT("Boss Minigun Sweep");
	bNotifyTick = true;
	bCreateNodeInstance = true;
	TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_MinigunSweep, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_MinigunSweep::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!BB || !AICon) return EBTNodeResult::Failed;

	CachedBoss = Cast<ABoss_Jetpack>(AICon->GetPawn());
	if (!CachedBoss || !CachedBoss->GetCombatComponent()) return EBTNodeResult::Failed;

	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!Target) Target = Cast<AActor>(BB->GetValueAsObject(AEnemyAIController::BB_TargetActor));
	if (!Target) return EBTNodeResult::Failed;

	AICon->StopMovement();

	// Initialize tracking
	FVector TargetLocation = Target->GetActorLocation() + FVector(0.f, 0.f, CachedBoss->AimZOffset);
	CachedBoss->GetCombatComponent()->SetHitTarget(TargetLocation);

	FVector Direction = (TargetLocation - CachedBoss->GetActorLocation()).GetSafeNormal();
	CurrentSweepRotation = Direction.Rotation();
	AICon->SetControlRotation(CurrentSweepRotation);
	AICon->SetFocalPoint(TargetLocation);

	SetManualAim(AICon, true);

	SweepTimeRemaining = SweepDuration;
	MissileTimer = TimeBetweenMissiles; // First missile fires after this delay

	// Start firing immediately!
	CachedBoss->GetCombatComponent()->SetAiming(true);
	CachedBoss->GetCombatComponent()->HandleFire(true);

	return EBTNodeResult::InProgress;
}

void UBTTask_MinigunSweep::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!CachedBoss || !CachedBoss->GetCombatComponent() || !AICon)
	{
		SetManualAim(AICon, false);
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!Target) Target = Cast<AActor>(BB->GetValueAsObject(AEnemyAIController::BB_TargetActor));

	if (!Target)
	{
		CachedBoss->GetCombatComponent()->HandleFire(false);
		CachedBoss->GetCombatComponent()->SetAiming(false);
		SetManualAim(AICon, false);
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 1. Sweep tracking math
	FVector TargetLocation = Target->GetActorLocation() + FVector(0.f, 0.f, CachedBoss->AimZOffset);
	FVector Direction = (TargetLocation - CachedBoss->GetActorLocation()).GetSafeNormal();
	float DistToPlayer = FVector::Dist(CachedBoss->GetActorLocation(), TargetLocation);

	CurrentSweepRotation = FMath::RInterpTo(CurrentSweepRotation, Direction.Rotation(), DeltaSeconds, SweepTrackingSpeed);
	FVector SweepHitTarget = CachedBoss->GetActorLocation() + FVector(0.f, 0.f, CachedBoss->AimZOffset) + (CurrentSweepRotation.Vector() * DistToPlayer);

	CachedBoss->GetCombatComponent()->SetHitTarget(SweepHitTarget);
	AICon->SetControlRotation(CurrentSweepRotation);
	AICon->SetFocalPoint(SweepHitTarget);

	// 2. Fire single missile
	if (MissileClass)
	{
		MissileTimer -= DeltaSeconds;
		if (MissileTimer <= 0.f)
		{
			FireSingleMissile(OwnerComp);
			MissileTimer = TimeBetweenMissiles;
		}
	}

	// 3. Duration check
	SweepTimeRemaining -= DeltaSeconds;
	if (SweepTimeRemaining <= 0.f)
	{
		CachedBoss->GetCombatComponent()->HandleFire(false);
		CachedBoss->GetCombatComponent()->SetAiming(false);
		SetManualAim(AICon, false);
		AICon->SetFocus(Target);
		
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

EBTNodeResult::Type UBTTask_MinigunSweep::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	SetManualAim(OwnerComp.GetAIOwner(), false);
	if (CachedBoss && CachedBoss->GetCombatComponent())
	{
		CachedBoss->GetCombatComponent()->HandleFire(false);
		CachedBoss->GetCombatComponent()->SetAiming(false);
	}
	return EBTNodeResult::Aborted;
}

void UBTTask_MinigunSweep::FireSingleMissile(UBehaviorTreeComponent& OwnerComp)
{
	if (!CachedBoss || !MissileClass) return;

	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	
	// FIX: Cast directly to ABaseCharacter instead of AActor
	ABaseCharacter* Target = BB ? Cast<ABaseCharacter>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
	if (!Target) Target = Cast<ABaseCharacter>(BB->GetValueAsObject(AEnemyAIController::BB_TargetActor));
	
	if (!Target) return;

	// Spawn it slightly above and to the right of the boss
	FVector SpawnLoc = CachedBoss->GetActorLocation() + FVector(0.f, 0.f, 80.f) + (CachedBoss->GetActorRightVector() * 50.f);
	FTransform SpawnTransform(CachedBoss->GetActorRotation(), SpawnLoc);

	AMicroMissileProjectile* Missile = CachedBoss->GetWorld()->SpawnActorDeferred<AMicroMissileProjectile>(MissileClass, SpawnTransform);
	if (Missile)
	{
		Missile->Initialize(CachedBoss, Target);
		Missile->FinishSpawning(SpawnTransform);
	}
}