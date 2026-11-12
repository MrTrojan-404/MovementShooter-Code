
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/AICharacterBase.h"
#include "EnemyBase.generated.h"

class UObjectiveTriggerComponent;
class AWaypointPath;
class UBehaviorTree;

UCLASS()
class MOVEMENTSHOOTER_API AEnemyBase : public AAICharacterBase
{
    GENERATED_BODY()

public:
    AEnemyBase();

    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
    float GracePeriodDuration = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Patrol")
    AWaypointPath* WaypointPath = nullptr;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
    float MaxChaseDistance = 3000.f;
    
    FVector PatrolOrigin;

    UPROPERTY(BlueprintReadWrite, Category = "AI")
    bool bIsResetting = false;

    UFUNCTION(BlueprintCallable, Category = "AI")
    void TriggerLeashReset();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Objective")
    UObjectiveTriggerComponent* ObjectiveTrigger = nullptr;
    
protected:
    virtual void BeginPlay() override;
    virtual void Destroyed() override;

    virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser) override;
};

