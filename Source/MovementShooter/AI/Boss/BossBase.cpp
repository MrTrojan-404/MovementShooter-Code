
#include "MovementShooter/AI/Boss/BossBase.h"
#include "MovementShooter/AI/Boss/ArenaManager.h"
#include "MovementShooter/AI/Boss/BossController.h" // We will create this next!
#include "MovementShooter/Component/CombatComponent.h"

ABossBase::ABossBase()
{
	bIsHeavyEnemy = true; // Bosses should not get knocked around by basic attacks
	
	// Force bosses to use the ruthless Boss Brain by default
	AIControllerClass = ABossController::StaticClass();
}

void ABossBase::BeginPlay()
{
	Super::BeginPlay();

	SpawnDefaultWeapon(true, false);

}

void ABossBase::HandleDeath(AController* InstigatorController, AActor* DamageCauser)
{
	if (bIsFinalPhase)
	{
		// If this is Phase 2 of the Twin Commanders, let them die normally!
		Super::HandleDeath(InstigatorController, DamageCauser);
	}
	else
	{
		// INTERCEPT DEATH: This is a phase transition!
		if (bIsDead) return;
		
		// 1. Lock them down so they stop shooting and taking damage
		bDisableGameplay = true;
		if (CombatComponent)
		{
			CombatComponent->HandleFire(false);
			CombatComponent->SetAiming(false);
		}

		// 2. Tell the Arena Manager to trigger the Cutscene / Breather Phase
		if (ArenaManager)
		{
			ArenaManager->AdvancePhase();
		}
	}
}

void ABossBase::RecoverForNextPhase()
{
	// Called via Blueprint after the breather phase is over to wake them back up
	Health = MaxHealth;
	bDisableGameplay = false;
}

