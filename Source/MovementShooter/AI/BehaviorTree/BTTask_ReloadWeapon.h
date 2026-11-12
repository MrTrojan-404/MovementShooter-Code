#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ReloadWeapon.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UBTTask_ReloadWeapon : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	UBTTask_ReloadWeapon();
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

private:
	UPROPERTY()
	class UCombatComponent* CachedCombat = nullptr;
};