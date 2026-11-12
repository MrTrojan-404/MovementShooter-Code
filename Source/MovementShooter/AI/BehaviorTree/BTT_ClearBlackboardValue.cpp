
#include "MovementShooter/AI/BehaviorTree/BTT_ClearBlackboardValue.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTT_ClearBlackboardValue::UBTT_ClearBlackboardValue()
{
	// This is the name that will appear in your Behavior Tree dropdown!
	NodeName = TEXT("Clear Blackboard Value"); 
}

EBTNodeResult::Type UBTT_ClearBlackboardValue::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	
	if (BlackboardComp)
	{
		// BlackboardKey is a built-in variable from UBTTask_BlackboardBase
		BlackboardComp->ClearValue(BlackboardKey.SelectedKeyName);
		
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}

