#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindJetpackPlatform.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_FindJetpackPlatform : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_FindJetpackPlatform();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;
};