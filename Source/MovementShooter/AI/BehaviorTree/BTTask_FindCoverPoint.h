#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindCoverPoint.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_FindCoverPoint : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_FindCoverPoint();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector CoverPointKey; // Stores the found ACoverPoint as an Object

	UPROPERTY(EditAnywhere, Category = "Cover")
	float SearchRadius = 2000.f;
};