#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ResetCoverState.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_ResetCoverState : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_ResetCoverState();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};