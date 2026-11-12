#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_JetpackGlide.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_JetpackGlide : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_JetpackGlide();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	// Called for every way a task can end (success, fail, abort)
	virtual void OnTaskFinished(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTNodeResult::Type TaskResult) override;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetLocationKey;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float FlightSpeed = 1000.f;

	// Used when the destination hover point is higher than the boss currently is
	UPROPERTY(EditAnywhere, Category = "Flight")
	float ClimbSpeed = 800.f;

private:
	UPROPERTY()
	class ABoss_Jetpack* CachedBoss = nullptr;

	FVector TargetLocation;
};