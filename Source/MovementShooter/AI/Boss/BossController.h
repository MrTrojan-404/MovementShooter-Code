#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "BossController.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API ABossController : public AEnemyAIController
{
	GENERATED_BODY()

public:
	// Called by the Arena Manager to force the Boss to attack a specific player
	UFUNCTION(BlueprintCallable, Category = "Boss AI")
	void LockOntoTarget(AActor* NewTarget);

	virtual void OnPossess(APawn* InPawn) override;
};