#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Boss/BossBase.h"
#include "Boss_Jetpack.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API ABoss_Jetpack : public ABossBase
{
	GENERATED_BODY()

public:
	ABoss_Jetpack();
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	// Separate mesh for the jetpack weak point
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss|WeakPoint")
	UStaticMeshComponent* JetpackMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|WeakPoint")
	FName JetpackAttachSocket = FName("spine_03");

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss|Mobility")
	TArray<AActor*> JumpPoints;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|WeakPoint")
	float WeakPointMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase 2 Hover")
	float HoverAltitudeOffset = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase 2 Hover")
	float MaxHoverDuration = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase 2 Hover")
	float GroundedRecoveryDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Boss|Phase 2 Hover")
	float MidAirKnockdownThreshold = 250.f;

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsHovering() const { return bIsHovering; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsPhase2() const { return bIsPhase2; }

	void StartHover();
	void StopHover(bool bForcedDownByPlayer);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|VFX")
	void OnMissileLockWarning(AActor* TargetActor);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|VFX")
	void OnHoverStateChanged(bool bAirborne, bool bWasForcedDown);

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss|VFX")
	void OnJetpackHopLaunched(FVector TargetLocation);

protected:
	virtual void BeginPlay() override;
	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) override;

private:
	bool bIsPhase2 = false;
	bool bIsHovering = false;
	bool bLastHitWasJetpack = false;

	float TargetHoverZ = 0.f;
	float AccumulatedAirJetpackDamage = 0.f;

	FTimerHandle HoverFuelTimer;
	FTimerHandle GroundCooldownTimer;

	void OnHoverFuelExpired();
	void StartNextHoverCycle();
};