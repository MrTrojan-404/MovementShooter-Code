// MovementShooter - single-player base character (ACharacter + CharacterMovementComponent).
// Ported from the Mover/APawn version. Removed: Mover, replication/RPCs, takedown, camera shoulder swap,
// lag compensation, downed/revive, reset-to-alive, tag-team dissolve, dodge, slide buffering (moves to the
// movement component), wall run / dash components (move to the movement component).

#pragma once

#include "CoreMinimal.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Character.h"
#include "MovementShooter/Interface/AnimInterface.h"
#include "MovementShooter/Interface/CombatInterface.h"
#include "MovementShooter/Interface/HitInterface.h"
#include "MovementShooter/Interface/InteractionInterface.h"
#include "MovementShooter/Interface/PickupInterface.h"
#include "BaseCharacter.generated.h"

class UBaseAnimInstance;
class AWeapon;
class UCombatComponent;
class UInteractionComponent;
class USpellComponent;
class UBehaviorTree;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBaseHealthChanged);

UCLASS()
class MOVEMENTSHOOTER_API ABaseCharacter : public ACharacter,
	public ICombatInterface, public IAnimInterface, public IHitInterface,
	public IInteractionInterface, public IPickupInterface
{
	GENERATED_BODY()

public:
	// The ObjectInitializer lets the player swap in the custom movement component
	ABaseCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void Tick(float DeltaTime) override;

	// Alias for ACharacter::GetMesh(), so the existing code that uses CharacterMesh keeps working.
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* CharacterMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCombatComponent* CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInteractionComponent* InteractionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpellComponent* SpellComponent;

	FVector LastHitDirection = FVector::ZeroVector;

	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnBaseHealthChanged OnBaseHealthChanged;

#pragma region CombatInterface
	virtual USkeletalMeshComponent* GetCombatMesh_Implementation() const override;
	virtual AActor* GetCombatOwner_Implementation() const override;
	virtual APawn* GetCombatPawn_Implementation() const override;
	virtual void PlayFireMontage_Implementation(bool bIsAiming) override;
	virtual UAnimMontage* GetReloadMontage_Implementation() override;
	virtual void PlayReloadMontage_Implementation() override;
	virtual void PlayThrowGrenadeMontage_Implementation() override;
	virtual UStaticMeshComponent* GetAttachedGrenade_Implementation() override;
	virtual void OnThrowGrenadeFinished_Implementation() override;
	virtual void OnLaunchGrenade_Implementation() override;
	virtual void OnFinishReload_Implementation() override;
	virtual void OnShotgunShellReload_Implementation() override;
	virtual void PlayMeleeMontage_Implementation() override;
	virtual void OnMeleeHit_Implementation() override;
	virtual void OnMeleeFinished_Implementation() override;
	virtual void PlayEquippedSwapMontage_Implementation() override;
	virtual void OnSwapEquippedFinished_Implementation() override;
	virtual void ShowSniperScopeCall_Implementation(bool bIsAiming) override;
#pragma endregion CombatInterface

#pragma region AnimInterface
	virtual bool GetAiming_Implementation() const override;
	virtual bool GetIsFiring_Implementation() const override;
	virtual UCombatComponent* GetCombatComponent_Implementation() override;
	virtual AWeapon* GetPrimaryWeapon_Implementation() override;
	virtual float GetAOPitch_Implementation() override;
	virtual float GetAOYaw_Implementation() override;
	virtual bool GetIsGameplayDisbled_Implementation() override;
	virtual EWeaponType GetEquippedWeaponType_Implementation() const override;
	// Wall run state now lives in the movement component (next step); until then these report false.
	virtual bool GetIsWallRunning_Implementation() const override;
	virtual bool GetIsRightWall_Implementation() const override;
#pragma endregion AnimInterface

	// StencilValue determines the color in your Post Process Material!
	void EnableTargetingOutline(bool bEnable, int32 StencilValue = 250);

	UPROPERTY(BlueprintReadWrite, Category = "State")
	bool bDisableGameplay = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "AI")
	UBehaviorTree* DefaultBehaviorTree;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;

	virtual void OnPickupBegin_Implementation(AActor* PickupActor) override;
	virtual void OnPickupEnd_Implementation(AActor* PickupActor) override;
	virtual void OnEquipItem_Implementation(AActor* Item) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI")
	FString CharacterName = TEXT("Unknown");

	UFUNCTION(BlueprintPure, Category = "UI")
	FString GetDisplayName() const { return CharacterName; }

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void ShowSniperScopeWidget(bool bShowScope);
	
	// === MOUSE SENSITIVITY SETTINGS ===
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float BaseMouseSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float DefaultAimSensitivity = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float SniperAimSens = 0.2f;

	UPROPERTY(BlueprintReadWrite, Category = "Mouse")
	float ActiveMouseSensitivity = 1.0f;

	// === FACTION IDENTIFIER ===
	UFUNCTION(BlueprintCallable, Category = "Faction")
	virtual bool IsPlayerAlly() const { return false; }

	UPROPERTY(EditAnywhere)
	TSubclassOf<AWeapon> DefaultWeaponClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<AWeapon> DefaultSecondaryWeaponClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Melee")
	UAnimMontage* MeleeMontage;

	virtual void ClearFocusedItem();

	// --- Medkits ---
	UFUNCTION(BlueprintCallable, Category = "Medkit")
	void AddMedkit(int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Medkit")
	void ConsumeMedkit();

	UFUNCTION(BlueprintPure, Category = "Medkit")
	int32 GetMedkitCount() const { return MedkitCount; }

	// --- Spell hooks (used by the gadget/spell system) ---
	// Default true. Override to return false while an enemy should be immune to crowd control.
	UFUNCTION(BlueprintNativeEvent, Category = "Spell")
	bool CanBeCrowdControlled() const;
	virtual bool CanBeCrowdControlled_Implementation() const { return true; }

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnStasisActive(bool bActive);

	UFUNCTION(BlueprintImplementableEvent, Category = "Spell")
	void OnStaggered(bool bWasHeavy);

	// --- Ragdoll (enemies only; Suspension Matrix suspends and releases them) ---
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void BeginRagdoll(FVector ImpulseDirection, float ImpulseStrength);

	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	void RecoverFromRagdoll();

	// Brings a dead body back to life in place. The checkpoint restore calls this on the player,
	// then sets health, ammo and position itself. Enemies are re-spawned by the checkpoint, not revived.
	UFUNCTION(BlueprintCallable, Category = "Ragdoll")
	virtual void ResetToAlive();

	// --- Health ---
	UPROPERTY(VisibleAnywhere, Category = "Player Stats")
	float Health = 100.f;

	// Dissolve timer handle keeps its original (typo'd) name because other classes use it.
	FTimerHandle DissolveTImer;
	void DissolveTimerFinished();

protected:
	virtual void BeginPlay() override;

	UPROPERTY()
	UBaseAnimInstance* CachedAnimInstance = nullptr;

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                   AController* InstigatorController, AActor* DamageCauser);

	// To be overridden by child classes (HUD updates etc.)
	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                              AController* InstigatorController, AActor* DamageCauser) {}

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	                         class AController* EventInstigator, AActor* DamageCauser) override;

	// The specific bone hit, used for death ragdolls
	UPROPERTY()
	FName LastHitBone = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float RagdollDeathImpulse = 5000.f;

	void ApplyDeathPhysics();

	virtual void SpawnDefaultWeapon(bool bSetToDestroyWeapon, bool bElimmedCheck);

	UPROPERTY(EditAnywhere, Category = "Player Stats")
	float MaxHealth = 100.f;

	// --- Dissolve effect ---
	UPROPERTY(EditDefaultsOnly)
	float DissolveDelay = 3.f;

	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance01;

	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance02;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance01;

	UPROPERTY(EditAnywhere, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance02;

	// --- Animation montages ---
	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* FireWeaponMontage;

	UPROPERTY(EditAnywhere, Category = Combat)
	UAnimMontage* ThrowGrenadeMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Montages")
	UAnimMontage* SwapMontage;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* AttachedGrenade;

	// Aim offset values returned through the anim interface
	float AO_Pitch = 0.f;
	float AO_Yaw = 0.f;

	void PlayHitReact(const FVector& HitDirection, FName HitBone);

	virtual void UpdateInteractionFocus();
	virtual void SetFocusedItem(AActor* NewItem);

	UPROPERTY(BlueprintReadOnly, Category = "Medkit")
	int32 MedkitCount = 0;

	UPROPERTY(EditDefaultsOnly, Category = "Medkit")
	int32 MaxMedkitCount = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Medkit")
	float MedkitHealAmount = 50.f;

	void UpdateMedkitHUD();

	// Where the mesh sat in BeginPlay (parent, socket and relative transform), restored after a ragdoll.
	// The player's first-person arms hang off the camera, enemies off the capsule, so it is captured per character.
	FTransform DefaultMeshRelativeTransform;

	UPROPERTY()
	USceneComponent* DefaultMeshParent = nullptr;

	FName DefaultMeshSocket = NAME_None;

	void ReattachMeshToDefaultParent();

	// --- Turn In Place ---
	UPROPERTY(EditDefaultsOnly, Category = "AI|Animation")
	UAnimMontage* TurnLeftMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AI|Animation")
	UAnimMontage* TurnRightMontage;

	bool bIsTurningInPlace = false;
	float TurnInPlaceTargetYaw = 0.f;

	// Which socket on the equipped weapon this character's left hand snaps to
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|IK")
	FName LeftHandSocketName = FName("LeftHandSocket");

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Montages")
	TMap<TSubclassOf<AWeapon>, UAnimMontage*> ReloadMontages;

private:
	UAnimMontage* ResolveReloadMontage() const;
	
public:
	FORCEINLINE UCombatComponent* GetCombatComponent() const { return CombatComponent; }
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE void SetHealth(float Amount) { Health = Amount; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
		FORCEINLINE FName GetLeftHandSocketName() const { return LeftHandSocketName; }

};
