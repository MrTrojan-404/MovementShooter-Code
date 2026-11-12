#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MinigunSweep.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_MinigunSweep : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_MinigunSweep();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Sweep")
	float SweepTrackingSpeed = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Sweep")
	float SweepDuration = 4.0f; 

	UPROPERTY(EditAnywhere, Category = "Sweep|Missiles")
	TSubclassOf<class AMicroMissileProjectile> MissileClass;

	UPROPERTY(EditAnywhere, Category = "Sweep|Missiles")
	float TimeBetweenMissiles = 1.5f;

private:
	float SweepTimeRemaining = 0.f;
	float MissileTimer = 0.f;

	FRotator CurrentSweepRotation = FRotator::ZeroRotator;
	UPROPERTY() class ABoss_Jetpack* CachedBoss = nullptr;

	void FireSingleMissile(UBehaviorTreeComponent& OwnerComp);
};