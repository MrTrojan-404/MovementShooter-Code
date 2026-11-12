
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "MovementShooter/AI/EnemyAITypes.h"
#include "BTTask_SetMovementSpeed.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_SetMovementSpeed : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_SetMovementSpeed();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Movement")
	EAIMovementSpeed DesiredSpeed = EAIMovementSpeed::Walk;
};

