
#include "MovementShooter/Objective/ObjectiveTriggerComponent.h"

#include "MovementShooter/AI/Character/EnemyBase.h"
#include "MovementShooter/Objective/ObjectiveSubsystem.h"

UObjectiveTriggerComponent::UObjectiveTriggerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UObjectiveTriggerComponent::TriggerEvent()
{
	// Only the server is allowed to trigger objective progress
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    
	// Prevent false kills from triggering during Snapshot Time Rewinds!
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(GetOwner()))
	{
		if (Enemy->bIsResetting) return;
	}

	if (EventTag == NAME_None) return;

	if (UObjectiveSubsystem* Subsystem = GetWorld()->GetGameInstance()->GetSubsystem<UObjectiveSubsystem>())
	{
		Subsystem->ReportObjectiveEvent(EventTag);
	}
}

