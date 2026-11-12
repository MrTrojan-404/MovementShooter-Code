#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "MeleeEnemy.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API AMeleeEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AMeleeEnemy();

	virtual void Tick(float DeltaTime) override;
	virtual void PlayMeleeMontage_Implementation() override;
	virtual void SetAIMovementSpeed(EAIMovementSpeed SpeedState) override;
	virtual void ApplyMovementSpeedForState(EAIState NewState) override;

protected:
	virtual void BeginPlay() override;
	virtual int32 GetEnemyType() const override { return 0; }

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	TArray<UAnimMontage*> MeleeAttackMontages;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Rest")
	UAnimMontage* RestMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Rest")
	bool bUseRestState = true;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Rest")
	float UnrestDuration = 1.0f;

	// How close (in 2D units) the enemy must be to PatrolOrigin before sitting down to rest
	UPROPERTY(EditDefaultsOnly, Category = "AI|Rest")
	float RestArrivalRadius = 120.f;

private:
	bool bIsResting = false;
	bool bIsWakingUp = false;
	FTimerHandle UnrestTimerHandle;

	bool IsAtRestLocation() const;
	void EnterRest();
	void TriggerUnrest(EAIState TargetState);
	void OnUnrestFinished();
};