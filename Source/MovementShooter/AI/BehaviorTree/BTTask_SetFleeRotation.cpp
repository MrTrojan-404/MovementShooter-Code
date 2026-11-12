#include "BTTask_SetFleeRotation.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UBTTask_SetFleeRotation::UBTTask_SetFleeRotation()
{
	NodeName = TEXT("Set Flee Rotation");
}

EBTNodeResult::Type UBTTask_SetFleeRotation::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	ACharacter* AIChar = AICon ? Cast<ACharacter>(AICon->GetPawn()) : nullptr;

	if (AIChar && AIChar->GetCharacterMovement())
	{
		if (bIsFleeing)
		{
			// Unhook from the camera focus and physically face the escape path
			AIChar->GetCharacterMovement()->bOrientRotationToMovement = true;
			AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = false;
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
			AICon->ClearFocus(EAIFocusPriority::Default);
		}
		else
		{
			// Lock back into combat strafe mode
			AIChar->GetCharacterMovement()->bOrientRotationToMovement = false;
			AIChar->GetCharacterMovement()->bUseControllerDesiredRotation = true;
		}
		return EBTNodeResult::Succeeded;
	}
	return EBTNodeResult::Failed;
}