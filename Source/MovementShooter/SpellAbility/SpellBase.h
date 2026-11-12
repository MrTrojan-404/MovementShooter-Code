
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SpellBase.generated.h"

class ABaseCharacter;

UCLASS(Abstract, Blueprintable, EditInlineNew)
class MOVEMENTSHOOTER_API USpellBase : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Spell")
	void Execute(ABaseCharacter* Caster);
	virtual void Execute_Implementation(ABaseCharacter* Caster) {}

	// Cost in Energy segments. Ultimate slot ignores this — it's gated by the separate kill-only bar instead.
	UPROPERTY(EditDefaultsOnly, Category = "Spell")
	int32 EnergySegmentCost = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Spell")
	FName SpellDisplayName;

	UFUNCTION(BlueprintNativeEvent, Category = "Spell")
	void OnReleased(ABaseCharacter* Caster);
	virtual void OnReleased_Implementation(ABaseCharacter* Caster) {}
	
};

