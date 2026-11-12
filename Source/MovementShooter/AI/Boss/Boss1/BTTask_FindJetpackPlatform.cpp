#include "BTTask_FindJetpackPlatform.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"

UBTTask_FindJetpackPlatform::UBTTask_FindJetpackPlatform() 
{ 
	NodeName = TEXT("Find Jetpack Platform"); 
}

EBTNodeResult::Type UBTTask_FindJetpackPlatform::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	ABoss_Jetpack* Boss = AICon ? Cast<ABoss_Jetpack>(AICon->GetPawn()) : nullptr;
	if (!Boss || Boss->JumpPoints.Num() == 0) return EBTNodeResult::Failed;

	const FVector BossLoc = Boss->GetActorLocation();

	// THE FIX (height): use the full 3D distance when deciding which platform he is "already on".
	// The old 2D check treated a platform directly above or below him as the one he's standing on,
	// so stacked platforms could never be chosen.
	TArray<AActor*> ValidPoints;
	AActor* FarthestPoint = nullptr;
	float FarthestDist = -1.f;

	for (AActor* Point : Boss->JumpPoints)
	{
		if (!Point) continue;

		const float Dist = FVector::Dist(BossLoc, Point->GetActorLocation());
		if (Dist > 500.f)
		{
			ValidPoints.Add(Point);
		}
		if (Dist > FarthestDist)
		{
			FarthestDist = Dist;
			FarthestPoint = Point;
		}
	}

	// If every point counts as "too close", hop to the farthest one instead of failing the sequence
	// (a failure here could leave him airborne mid-hop depending on the tree order).
	AActor* ChosenPoint = nullptr;
	if (ValidPoints.Num() > 0)
	{
		ChosenPoint = ValidPoints[FMath::RandRange(0, ValidPoints.Num() - 1)];
	}
	else
	{
		ChosenPoint = FarthestPoint;
	}

	if (!ChosenPoint) return EBTNodeResult::Failed;

	// Hover position directly ABOVE the chosen platform. Glide now honours this height.
	const FVector HoverLocation = ChosenPoint->GetActorLocation() + FVector(0.f, 0.f, Boss->HoverAltitudeOffset);

	OwnerComp.GetBlackboardComponent()->SetValueAsVector(TargetLocationKey.SelectedKeyName, HoverLocation);
	return EBTNodeResult::Succeeded;
}