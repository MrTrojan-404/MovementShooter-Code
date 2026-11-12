
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/SpellAbility/SpellBase.h"
#include "SuspensionMatrixSpell.generated.h"

class ASuspensionMatrixProjectile;

UCLASS()
class MOVEMENTSHOOTER_API USuspensionMatrixSpell : public USpellBase
{
	GENERATED_BODY()

public:
	USuspensionMatrixSpell()
	{
		EnergySegmentCost = 0; // Ultimate
	}

	virtual void Execute_Implementation(ABaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix")
	TSubclassOf<ASuspensionMatrixProjectile> MatrixProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix|Audio")
	USoundBase* CastSound;
	
};

