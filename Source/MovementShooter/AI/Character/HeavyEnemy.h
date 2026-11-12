
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "HeavyEnemy.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API AHeavyEnemy : public AEnemyBase
{
	GENERATED_BODY()

public:
	AHeavyEnemy();

protected:
	virtual void BeginPlay() override;

	// Let's assign EnemyType 3 to the Heavy
	virtual int32 GetEnemyType() const override { return 3; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float HeavyEngagementRange = 1000.f;
};

