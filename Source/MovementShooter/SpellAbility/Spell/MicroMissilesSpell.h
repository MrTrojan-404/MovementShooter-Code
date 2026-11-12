
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/SpellAbility/SpellBase.h"
#include "MicroMissilesSpell.generated.h"

class AMicroMissileProjectile;

UCLASS()
class MOVEMENTSHOOTER_API UMicroMissilesSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UMicroMissilesSpell()
	{
		EnergySegmentCost = 2; 
		SpellDisplayName = TEXT("Micro Missiles");
	}

	virtual void Execute_Implementation(ABaseCharacter* Caster) override;
	virtual void OnReleased_Implementation(ABaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	int32 MaxLocks = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	float LockTraceRange = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	float ScanInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	float LaunchStaggerDelay = 0.15f; 

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles")
	TSubclassOf<AMicroMissileProjectile> MissileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Micro Missiles|Outline")
	int32 LockStencil = 252; 

private:
	UPROPERTY()
	ABaseCharacter* CachedCaster = nullptr;

	UPROPERTY()
	TArray<TWeakObjectPtr<ABaseCharacter>> LockedTargets;

	FTimerHandle ScanTimer;

	void ScanForTarget();
	
	void LaunchMissile(ABaseCharacter* Caster, ABaseCharacter* Target, int32 Index);
};

