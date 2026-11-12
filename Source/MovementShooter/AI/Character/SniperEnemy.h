
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/RangedEnemy.h"
#include "SniperEnemy.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API ASniperEnemy 
	: public ARangedEnemy
{
	GENERATED_BODY()

public:
	ASniperEnemy();

	// How long sniper aims before firing
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float AimDuration = 1.5f;

	// How long sniper waits between shots
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float ShotCooldown = 3.f;

	// Max range sniper engages at
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		Category = "AI|Combat")
	float SniperRange = 3000.f;

protected:
	virtual void BeginPlay() override;

	virtual int32 GetEnemyType() const override { return 2; }

};

