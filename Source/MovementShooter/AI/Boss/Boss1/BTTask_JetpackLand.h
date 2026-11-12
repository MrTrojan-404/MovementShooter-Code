#pragma once
#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_JetpackLand.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_JetpackLand : public UBTTaskNode
{
	GENERATED_BODY()
public:
	UBTTask_JetpackLand();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

protected:
	// Initial downward speed when the boss drops (gravity keeps accelerating him after this)
	UPROPERTY(EditAnywhere, Category = "Landing")
	float DropSpeed = 1200.f;

	// Fail-safe: if he hasn't touched down by then, snap him to the floor
	UPROPERTY(EditAnywhere, Category = "Landing")
	float MaxLandTime = 3.f;

private:
	bool SnapToFloor();

	UPROPERTY() class ABoss_Jetpack* CachedBoss = nullptr;
	float ElapsedTime = 0.f;
	bool bNeedsVFXOff = false;
};