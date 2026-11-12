#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_EnterCover.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_EnterCover : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_EnterCover();
protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};