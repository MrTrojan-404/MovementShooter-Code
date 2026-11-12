#include "BTTask_JetpackGlide.h"
#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"

UBTTask_JetpackGlide::UBTTask_JetpackGlide()
{
	NodeName = TEXT("Jetpack Glide");
	bNotifyTick = true;
	bCreateNodeInstance = true;

	TargetLocationKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_JetpackGlide, TargetLocationKey));
}

EBTNodeResult::Type UBTTask_JetpackGlide::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	CachedBoss = AICon ? Cast<ABoss_Jetpack>(AICon->GetPawn()) : nullptr;
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();

	if (!CachedBoss || !BB) return EBTNodeResult::Failed;

	TargetLocation = BB->GetValueAsVector(TargetLocationKey.SelectedKeyName);

	// THE FIX (height): FindJetpackPlatform puts the hover point above the chosen jump point, but this
	// task used to throw that height away and lock Z to wherever he took off. With platforms at
	// different heights he'd arrive inside or under the destination, and Land would then trace down
	// from the wrong place. Never descend here (Land does that), but DO climb if the destination
	// hover point is higher than he is now.
	TargetLocation.Z = FMath::Max(TargetLocation.Z, CachedBoss->GetActorLocation().Z);

	return EBTNodeResult::InProgress;
}

void UBTTask_JetpackGlide::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!CachedBoss)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	const FVector CurrentLoc = CachedBoss->GetActorLocation();

	// 1. Climb first if the destination is higher, so he doesn't clip through the edge of the platform
	if (CurrentLoc.Z < TargetLocation.Z - 5.f)
	{
		const float NewZ = FMath::FInterpConstantTo(CurrentLoc.Z, TargetLocation.Z, DeltaSeconds, ClimbSpeed);
		CachedBoss->SetActorLocation(FVector(CurrentLoc.X, CurrentLoc.Y, NewZ));
		return;
	}

	// 2. Then move horizontally toward the point above the platform
	const FVector Goal(TargetLocation.X, TargetLocation.Y, CurrentLoc.Z);
	const FVector NewLoc = FMath::VInterpConstantTo(CurrentLoc, Goal, DeltaSeconds, FlightSpeed);
	CachedBoss->SetActorLocation(NewLoc);

	// 3. Smoothly face the direction of travel
	const FVector Direction = (Goal - CurrentLoc).GetSafeNormal();
	if (!Direction.IsNearlyZero())
	{
		if (AController* Controller = CachedBoss->GetController())
		{
			const FRotator NewRot = FMath::RInterpTo(CachedBoss->GetActorRotation(), Direction.Rotation(), DeltaSeconds, 5.f);
			Controller->SetControlRotation(NewRot);
		}
	}

	// 4. Arrive and finish
	if (FVector::DistSquared(NewLoc, Goal) < 100.f)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}

void UBTTask_JetpackGlide::OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult)
{
	Super::OnTaskFinished(OwnerComp, NodeMemory, TaskResult);

	// If the hop is cut short mid-glide, don't leave him floating in MOVE_Flying (no gravity)
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