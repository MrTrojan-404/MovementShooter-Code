
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "RangedEnemy.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API ARangedEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	ARangedEnemy();

	// Preferred distance to maintain from target
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float PreferredCombatDistance = 600.f;

	// Min distance before backing up
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float MinCombatDistance = 300.f;

protected:
	virtual void BeginPlay() override;

	virtual int32 GetEnemyType() const override { return 1; }

};

