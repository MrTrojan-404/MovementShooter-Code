#include "BTTask_ReloadWeapon.h"
#include "AIController.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/GameVariables/CombatState.h"

UBTTask_ReloadWeapon::UBTTask_ReloadWeapon()
{
	NodeName = TEXT("Reload Weapon");
	bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_ReloadWeapon::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	ABaseCharacter* AIChar = AICon ? Cast<ABaseCharacter>(AICon->GetPawn()) : nullptr;
	
	if (!AIChar || !AIChar->GetCombatComponent()) return EBTNodeResult::Failed;
	
	CachedCombat = AIChar->GetCombatComponent();

	// 1. If the weapon is full, skip the reload and succeed instantly
	if (!CachedCombat->GetPrimaryWeapon() || !CachedCombat->GetPrimaryWeapon()->IsEmpty())
	{
		return EBTNodeResult::Succeeded;
	}

	// 2. The weapon is empty! Trigger the reload.
	CachedCombat->Reload();
	
	// Return InProgress so the tree pauses here while the montage plays
	return EBTNodeResult::InProgress;
}

void UBTTask_ReloadWeapon::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!CachedCombat)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	// 3. The CombatComponent automatically returns to ECS_Unoccupied when the reload animation ends.
	if (CachedCombat->GetCombatState() == ECombatState::ECS_Unoccupied)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
	}
}