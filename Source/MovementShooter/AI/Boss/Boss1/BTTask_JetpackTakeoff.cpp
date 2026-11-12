#include "BTTask_JetpackTakeoff.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"

UBTTask_JetpackTakeoff::UBTTask_JetpackTakeoff()
{
	NodeName = TEXT("Jetpack Takeoff");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_JetpackTakeoff::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	CachedBoss = AICon ? Cast<ABoss_Jetpack>(AICon->GetPawn()) : nullptr;
	if (!CachedBoss) return EBTNodeResult::Failed;

	const FVector Loc = CachedBoss->GetActorLocation();
	const float CapHalfHeight = CachedBoss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// THE FIX (stacking): measure the hover altitude from the FLOOR under the boss, not from his
	// current Z. Before, every Takeoff added another HoverAltitudeOffset on top of wherever he
	// already was, so a hop that got interrupted made the next one climb even higher.
	float StandingZ = Loc.Z; // fallback if no floor is found
	{
		FCollisionObjectQueryParams ObjParams;
		ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

		FCollisionQueryParams Params;
		Params.AddIgnoredActor(CachedBoss);
		TArray<AActor*> Attached;
		CachedBoss->GetAttachedActors(Attached);
		Params.AddIgnoredActors(Attached);

		FHitResult Hit;
		if (CachedBoss->GetWorld()->LineTraceSingleByObjectType(Hit, Loc, Loc - FVector(0.f, 0.f, 5000.f), ObjParams, Params))
		{
			StandingZ = Hit.ImpactPoint.Z + CapHalfHeight;
		}
	}

	TargetLocation = FVector(Loc.X, Loc.Y, StandingZ + CachedBoss->HoverAltitudeOffset);

	UCharacterMovementComponent* Move = CachedBoss->GetCharacterMovement();
	const bool bWasFlying = (Move->MovementMode == MOVE_Flying);
	Move->SetMovementMode(MOVE_Flying);

	// Only fire the "jetpack on" VFX if he wasn't already airborne
	if (!bWasFlying)
	{
		CachedBoss->OnHoverStateChanged(true, false);
	}

	// Already at (or above) hover altitude: nothing to do
	if (Loc.Z >= TargetLocation.Z - 10.f)
	{
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

void UBTTask_JetpackTakeoff::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!CachedBoss)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const FVector CurrentLoc = CachedBoss->GetActorLocation();
	const FVector NewLoc = FMath::VInterpConstantTo(CurrentLoc, TargetLocation, DeltaSeconds, ClimbSpeed);
	CachedBoss->SetActorLocation(NewLoc);

	if (FVector::DistSquared(NewLoc, TargetLocation) < 25.f)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

void UBTTask_JetpackTakeoff::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	// THE FIX (stuck in the air): MOVE_Flying has no gravity, and only the Land task puts him back
	// on the ground. If the tree aborts or fails this hop (e.g. a decorator aborting the branch),
	// he used to stay frozen in the air, run the attack branch from there, and climb even higher
	// on the next hop. Drop him whenever the hop didn't complete.
	if (TaskResult != EBTNodeResult::Succeeded && CachedBoss && !CachedBoss->IsHovering())
	{
		UCharacterMovementComponent* Move = CachedBoss->GetCharacterMovement();
		if (Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Falling);
			CachedBoss->OnHoverStateChanged(false, false);
		}
	}
}