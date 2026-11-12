#include "BTTask_FindStrafeLocation.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"

UBTTask_FindStrafeLocation::UBTTask_FindStrafeLocation()
{
	NodeName = TEXT("Find Strafe Location");
}

EBTNodeResult::Type UBTTask_FindStrafeLocation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	APawn* AIPawn = AICon ? AICon->GetPawn() : nullptr;
	if (!AIPawn) return EBTNodeResult::Failed;

	// Grab the Player from the Blackboard
	AActor* TargetActor = Cast<AActor>(OwnerComp.GetBlackboardComponent()->GetValueAsObject(FName("TargetActor")));
	if (!TargetActor) return EBTNodeResult::Failed;

	// 1. Find the left/right vector relative to the player
	FVector DirToPlayer = (TargetActor->GetActorLocation() - AIPawn->GetActorLocation()).GetSafeNormal();
	FVector RightVector = FVector::CrossProduct(DirToPlayer, FVector::UpVector);

	// 2. Randomly pick Left or Right
	float DirectionMultiplier = FMath::RandBool() ? 1.f : -1.f;
	FVector TargetStrafeLoc = AIPawn->GetActorLocation() + (RightVector * DirectionMultiplier * StrafeDistance);

	// 3. Project it onto the NavMesh so they don't try to walk off a cliff or into a wall
	UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation NavLoc;
	if (NavSys && NavSys->GetRandomPointInNavigableRadius(TargetStrafeLoc, 150.f, NavLoc))
	{
		// Save the location to the blackboard key you selected in the BT
		OwnerComp.GetBlackboardComponent()->SetValueAsVector(GetSelectedBlackboardKey(), NavLoc.Location);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}