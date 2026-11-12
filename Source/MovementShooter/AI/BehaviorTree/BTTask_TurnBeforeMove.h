#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_TurnBeforeMove.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_TurnBeforeMove : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_TurnBeforeMove();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual FString GetStaticDescription() const override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetKey;

	// How far off-center the target must be to trigger a full turn-in-place
	UPROPERTY(EditAnywhere, Category = "Turn")
	float TurnThreshold = 60.f;

	// Safely clean up if the AI gets shot while turning
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
	UPROPERTY()
	class ABaseCharacter* CachedCharacter = nullptr;
};