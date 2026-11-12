
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/SpellAbility/SpellBase.h"
#include "ArcDartSpell.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UArcDartSpell : public USpellBase
{
	GENERATED_BODY()

public:
	UArcDartSpell()
	{
		EnergySegmentCost = 1; 
		SpellDisplayName = TEXT("Arc Dart");
	}

	virtual void Execute_Implementation(ABaseCharacter* Caster) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Arc Dart")
	TSubclassOf<class AArcDartProjectile> DartProjectileClass;

};

