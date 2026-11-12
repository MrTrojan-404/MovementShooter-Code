#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/Animation/BaseAnimInstance.h"
#include "MovementShooter/AI/EnemyAITypes.h"
#include "MeleeAnimInstance.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UMeleeAnimInstance : public UBaseAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void TriggerHitReact(const FVector& HitWorldDirection, FName HitBone) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Melee AI")
	EAIState CurrentAIState = EAIState::Patrol;

	// Single montage containing Back, BodyLeft, BodyRight, HeadLeft, HeadRight sections
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Melee AI|Hit Reactions")
	class UAnimMontage* HitReactMontage = nullptr;
};