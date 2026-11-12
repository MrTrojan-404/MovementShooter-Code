#include "BTTask_ResetCoverState.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ResetCoverState::UBTTask_ResetCoverState()
{
	NodeName = TEXT("Reset Cover State");
}

EBTNodeResult::Type UBTTask_ResetCoverState::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent())
	{
		// Force the animation off immediately
		BB->SetValueAsBool(FName("IsInCover"), false);
	}
	return EBTNodeResult::Succeeded;
}