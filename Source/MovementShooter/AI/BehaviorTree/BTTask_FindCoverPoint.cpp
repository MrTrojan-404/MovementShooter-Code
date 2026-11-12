#include "BTTask_FindCoverPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/AI/Actor/CoverPoint.h"

UBTTask_FindCoverPoint::UBTTask_FindCoverPoint()
{
	NodeName = TEXT("Find Cover Point");
}

EBTNodeResult::Type UBTTask_FindCoverPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AAIController* AICon = OwnerComp.GetAIOwner();
	
	if (!BB || !AICon || !AICon->GetPawn()) return EBTNodeResult::Failed;

	AActor* TargetActor = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
	if (!TargetActor) return EBTNodeResult::Failed;

	TArray<AActor*> AllCoverPoints;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACoverPoint::StaticClass(), AllCoverPoints);

	ACoverPoint* BestCover = nullptr;
	float MinDistance = SearchRadius;

	for (AActor* Actor : AllCoverPoints)
	{
		ACoverPoint* Cover = Cast<ACoverPoint>(Actor);
		if (!Cover) continue;

		float DistanceToCover = FVector::Dist(AICon->GetPawn()->GetActorLocation(), Cover->GetActorLocation());
		
		if (DistanceToCover < MinDistance)
		{
			FVector DirToPlayer = (TargetActor->GetActorLocation() - Cover->GetActorLocation()).GetSafeNormal();
			float Dot = FVector::DotProduct(Cover->GetActorForwardVector(), DirToPlayer);

			// FIX: Dot < -0.1f ensures the player is on the OPPOSITE side of the wall!
			if (Dot < -0.1f)
			{
				MinDistance = DistanceToCover;
				BestCover = Cover;
			}
		}
	}

	if (BestCover)
	{
		BB->SetValueAsObject(CoverPointKey.SelectedKeyName, BestCover);
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed; // No valid cover found nearby!
}