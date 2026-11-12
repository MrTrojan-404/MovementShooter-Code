#include "MovementShooter/AI/BehaviorTree/BTTask_TurnBeforeMove.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "MovementShooter/Character/BaseCharacter.h"

UBTTask_TurnBeforeMove::UBTTask_TurnBeforeMove()
{
	NodeName = TEXT("Turn Before Move");
	bNotifyTick = true;
	bCreateNodeInstance = true;

	// Accept both Actors (Player) and Vectors (Waypoints/Gunshot Locations)
	TargetKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_TurnBeforeMove, TargetKey), AActor::StaticClass());
	TargetKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_TurnBeforeMove, TargetKey));
}

EBTNodeResult::Type UBTTask_TurnBeforeMove::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	if (!BB || !AICon) return EBTNodeResult::Failed;

	CachedCharacter = Cast<ABaseCharacter>(AICon->GetPawn());
	if (!CachedCharacter) return EBTNodeResult::Failed;

	FVector TargetLocation = FVector::ZeroVector;

	// --- FIX: Tell the AI to explicitly "Focus" on the target so Unreal stops fighting the rotation! ---
	if (UObject* TargetObject = BB->GetValueAsObject(TargetKey.SelectedKeyName))
	{
		if (AActor* TargetActor = Cast<AActor>(TargetObject))
		{
			TargetLocation = TargetActor->GetActorLocation();
			AICon->SetFocus(TargetActor); // Lock eyes on Actor
		}
	}
	else
	{
		TargetLocation = BB->GetValueAsVector(TargetKey.SelectedKeyName);
		AICon->SetFocalPoint(TargetLocation); // Lock eyes on Vector
	}

	if (TargetLocation.IsNearlyZero()) return EBTNodeResult::Succeeded;

	FVector Direction = (TargetLocation - CachedCharacter->GetActorLocation()).GetSafeNormal();
	FRotator TargetRot = Direction.Rotation();
	float YawDiff = FRotator::NormalizeAxis(TargetRot.Yaw - CachedCharacter->GetActorRotation().Yaw);

	// If the angle is severe enough, keep the Focus locked and wait in TickTask!
	if (FMath::Abs(YawDiff) > TurnThreshold)
	{
		return EBTNodeResult::InProgress; 
	}

	// If they are already facing the target, clear focus and succeed instantly
	AICon->ClearFocus(EAIFocusPriority::Gameplay);
	return EBTNodeResult::Succeeded;
}

void UBTTask_TurnBeforeMove::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!CachedCharacter)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	AAIController* AICon = OwnerComp.GetAIOwner();
	if (AICon)
	{
		// Monitor the C++ turn-in-place logic. Once the physical body has smoothly rotated 
		// to match the AI Controller's eyes, release the lock!
		float YawDiff = FRotator::NormalizeAxis(AICon->GetControlRotation().Yaw - CachedCharacter->GetActorRotation().Yaw);
		
		if (FMath::Abs(YawDiff) < 10.f)
		{
			AICon->ClearFocus(EAIFocusPriority::Gameplay); // Free their eyes so MoveTo works normally
			FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		}
	}
}

EBTNodeResult::Type UBTTask_TurnBeforeMove::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (AAIController* AICon = OwnerComp.GetAIOwner())
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
	}
	return EBTNodeResult::Aborted;
}
 
FString UBTTask_TurnBeforeMove::GetStaticDescription() const
{
	return FString::Printf(TEXT("Forces AI to play Turn-In-Place towards %s before walking."), *TargetKey.SelectedKeyName.ToString());
}