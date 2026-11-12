
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/AICharacterBase.h"
#include "BossBase.generated.h"

class AArenaManager;
class AWeapon;

UCLASS()
class MOVEMENTSHOOTER_API ABossBase : public AAICharacterBase
{
	GENERATED_BODY()

public:
	ABossBase();

	virtual void BeginPlay() override;
	
	// Link this boss to the specific arena it belongs to
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss")
	AArenaManager* ArenaManager;

	// Does this boss actually die at 0 HP, or just transition phases?
	UPROPERTY(EditDefaultsOnly, Category = "Boss")
	bool bIsFinalPhase = false;

	// The specific Behavior Tree this boss should use
	UPROPERTY(EditDefaultsOnly, Category = "Boss AI")
	UBehaviorTree* BossBehaviorTree;

	UBehaviorTree* GetBossBehaviorTree() const { return BossBehaviorTree; }

protected:
	// OVERRIDE: We intercept death so they don't dissolve!
	virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser) override;

	// Allows the Boss to reset its health and stand back up for Phase 2
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void RecoverForNextPhase();
};

