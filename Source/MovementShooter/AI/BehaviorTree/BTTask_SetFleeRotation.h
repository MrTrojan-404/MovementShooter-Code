#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_SetFleeRotation.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_SetFleeRotation : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetFleeRotation();

protected:
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	UPROPERTY(EditAnywhere, Category = "Flee")
	bool bIsFleeing = true;
};