#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/EnemyAITypes.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "AICharacterBase.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API AAICharacterBase : public ABaseCharacter
{
	GENERATED_BODY()

public:
	AAICharacterBase();

	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser) override;

	virtual int32 GetEnemyType() const { return -1; }

	// --- UNIVERSAL PERCEPTION ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float SightRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float PeripheralVisionAngle = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float HearingRange = 800.f;

	// --- UNIVERSAL COMBAT ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	float AimZOffset = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	float AimErrorRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float MeleeRange = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float EngagementRange = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat")
	float AttackCooldown = 1.f;

	// 0 = Melee, 1 = Ranged, 2 = Sniper, 3 = Heavy_Ranged
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat")
	int32 CombatStyle = 1; 

	bool IsAlive() const { return !bIsDead; }

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	bool bIsHeavyEnemy = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	UAnimMontage* ElectrocuteDeathMontage;

	void TriggerElectrocutionDeath(AActor* InstigatorActor, class UNiagaraSystem* ZapVFX, class USoundBase* ZapSound);

	void PlayElectrocuteFX(class UNiagaraSystem* ZapVFX, class USoundBase* ZapSound, float Duration, bool bPlayDeathAnimation);

	// --- MOVEMENT SPEED (replaces the old GASP walk / jog / sprint states) ---
	// Speeds are unreal units per second; the character's Max Walk Speed is set to one of them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
	float JogSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
	float SprintSpeed = 650.f;

	// Pick the speed from the AI state automatically. A Behavior Tree "Set Movement Speed" task still overrides it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement")
	bool bAutoMovementSpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement", meta = (EditCondition = "bAutoMovementSpeed"))
	EAIMovementSpeed PatrolSpeed = EAIMovementSpeed::Walk;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement", meta = (EditCondition = "bAutoMovementSpeed"))
	EAIMovementSpeed AlertSpeed = EAIMovementSpeed::Jog;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Movement", meta = (EditCondition = "bAutoMovementSpeed"))
	EAIMovementSpeed EngageSpeed = EAIMovementSpeed::Sprint;

	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	virtual void SetAIMovementSpeed(EAIMovementSpeed SpeedState);
	
	UFUNCTION(BlueprintPure, Category = "AI|Movement")
	EAIMovementSpeed GetAIMovementSpeed() const { return CurrentMovementSpeed; }

	// Called by the AI controller whenever the AI state changes
	virtual void ApplyMovementSpeedForState(EAIState NewState);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	class UWidgetComponent* StealthIndicatorWidget;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Cover")
	UAnimMontage* EnterCoverMontage;
	
protected:
	virtual void BeginPlay() override;

	virtual void HandleDeath(AController* InstigatorController, AActor* DamageCauser);

	void NotifyControllerOfDeath();

	void ApplyDeathVisuals(const FVector& HitDirection, FName HitBone);

	bool bIsDead = false;

	EAIMovementSpeed CurrentMovementSpeed = EAIMovementSpeed::Walk;

	// Who gets kill credit when an electrocution (not a bullet) finishes this enemy.
	TWeakObjectPtr<ABaseCharacter> KillCreditInstigator;

	void StopElectrocuteFX();

	UPROPERTY()
	class UNiagaraComponent* ActiveElectrocuteVFX;

	UPROPERTY()
	class UAudioComponent* ActiveElectrocuteSound;

	FTimerHandle ElectrocuteFXTimer;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	UAnimMontage* DeathMontage;

	// Set to false on enemies like the Stickman whose meshes deform badly in ragdoll
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Death")
	bool bRagdollOnDeath = true;

	FTimerHandle DeathPoseFreezeTimer;
	void FreezeDeathPose();
	
};
