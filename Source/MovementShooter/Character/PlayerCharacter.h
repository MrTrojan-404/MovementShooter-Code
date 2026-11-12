// MovementShooter - first-person, single-player player character.
// Ported from the third-person GASP player: no team colors, overhead widgets, squad/companions, downed/revive,
// elimination timers or respawn RPCs. Death hands over to the mission game mode, which restores the last checkpoint.

#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UPawnNoiseEmitterComponent;
class UStealthComponent;
class AWeapon;
class ACorePlayerController;
class UShooterMovementComponent;

UCLASS()
class MOVEMENTSHOOTER_API APlayerCharacter : public ABaseCharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	virtual bool IsPlayerAlly() const override { return true; }

	// --- Camera ---
	UFUNCTION(BlueprintPure, Category = "Camera")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	// --- Input API (called by ACorePlayerController, which owns all the key bindings) ---
	void MoveInput(const FVector2D& Axis);
	void LookInput(const FVector2D& Axis);
	void JumpPressed();
	void JumpReleased();
	void CrouchPressed();
	void CrouchReleased();
	void SprintPressed();
	void SprintReleased();

	// --- HUD ---
	void UpdateHUDHealth();
	void UpdateHUDAmmo();

	virtual bool CanJumpInternal_Implementation() const override;

	// --- Death / checkpoint restore ---
	// Locks the player and plays the death sequence. The mission game mode calls this, then fails the mission.
	void Elim();

	// Called by the checkpoint restore: base reset plus clearing the eliminated flag.
	virtual void ResetToAlive() override;

	FORCEINLINE bool IsElimmed() const { return bElimmed; }

	// Dash button (called by the controller). Dashes where the player is pushing.
	virtual void DashPressed();

	UFUNCTION(BlueprintPure, Category = "Movement")
	UShooterMovementComponent* GetShooterMovement() const { return ShooterMovement; }

	// Blueprint hook for the camera/death sequence (fall, fade, death screen).
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnPlayerDied();

	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void OnPlayerRestored();

	virtual bool GetIsGameplayDisbled_Implementation() override;
	virtual void PlayMeleeMontage_Implementation() override;
	virtual bool GetIsWallRunning_Implementation() const override;
	virtual bool GetIsRightWall_Implementation() const override;

	void GroundSmashPressed();

	// Receives the recoil values from the Combat Component
	UFUNCTION(BlueprintCallable, Category = "Camera|Recoil")
	void AddCameraRecoil(float PitchAmount, float YawAmount);
	
protected:
	virtual void BeginPlay() override;

	virtual void OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                              AController* InstigatorController, AActor* DamageCauser) override;

	// Waits for the controller, then spawns the starting weapons and fills the HUD (same as the old PollInit).
	void PollInit();

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
	UPawnNoiseEmitterComponent* NoiseEmitterComponent;

	UPROPERTY()
	UStealthComponent* StealthComponent = nullptr;

	// --- Sword ---
	// Shown only while a slash is playing. Assign the mesh in the Blueprint; it attaches to SwordSocketName on the arms mesh.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	UStaticMeshComponent* SwordMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Sword")
	FName SwordSocketName = FName("SwordSocket");

	// The slash animation. Put a MeleeHit notify where the blade should connect and a MeleeFinished notify at the end.
	// Falls back to the weapon-bash montage when empty.
	UPROPERTY(EditDefaultsOnly, Category = "Sword")
	UAnimMontage* SwordSlashMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Sword")
	float SwordSlashPlayRate = 1.f;

	// Hide the head bone of the mesh so the head never shows up (or clips) in the camera.
	// Does nothing when the mesh has no bone with this name (for example arms only).
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	bool bHideHeadBone = true;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	FName HeadBoneName = FName("head");

	// --- Camera tuning ---
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float StandingCameraHeight = 64.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CrouchedCameraHeight = 36.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float CameraHeightInterpSpeed = 12.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float DefaultFOV = 100.f;

	// FOV while aiming with a normal weapon
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float AimFOV = 80.f;

	// FOV while aiming with a sniper rifle
	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float SniperAimFOV = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera")
	float FOVInterpSpeed = 18.f;

	// Camera roll while wall running, in degrees. Positive leans the view away from the wall;
	// use a negative value to lean toward it, or 0 to turn the roll off.
	UPROPERTY(EditDefaultsOnly, Category = "Camera|WallRun")
	float WallRunCameraRoll = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|WallRun")
	float CameraRollInterpSpeed = 8.f;

	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// --- Interaction (camera trace) ---
	virtual void UpdateInteractionFocus() override;
	virtual void SetFocusedItem(AActor* NewItem) override;
	virtual void ClearFocusedItem() override;

	bool bElimmed = false;

	// Ragdoll the character mesh on death. Leave off when the mesh is the first-person arms.
	UPROPERTY(EditDefaultsOnly, Category = "Death")
	bool bRagdollOnDeath = false;

	// Movement events: wall run and dash interrupt reloads and combat actions, like the old components did
	UFUNCTION()
	void HandleDashStarted(FVector Direction);

	UFUNCTION()
	void HandleDashEnded();

	UFUNCTION()
	void HandleWallRunStarted(bool bRightWall);

	// --- Procedural Head Bob ---
	float HeadBobTime = 0.f;

	// How fast the camera bobs
	UPROPERTY(EditAnywhere, Category = "Camera|HeadBob")
	float BobFrequency = 15.f;

	// How far the camera moves up/down and left/right
	UPROPERTY(EditAnywhere, Category = "Camera|HeadBob")
	float BobAmplitude = 1.5f;

	// Smoothly scales the bob in and out to prevent jitter
	float CurrentBobWeight = 0.f;

	// --- Ground Smash ---
	UFUNCTION()
	void HandleGroundSmashLanded();

	UPROPERTY(EditDefaultsOnly, Category = "Combat|GroundSmash")
	float GroundSmashDamage = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|GroundSmash")
	float GroundSmashRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|GroundSmash")
	bool bDrawGroundSmashDebug = true;

	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|GroundSmash")
	float MinGroundSmashDamage = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|GroundSmash")
	float MaxGroundSmashDamage = 500.f;

	// How far the player must fall to deal the Maximum damage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|GroundSmash")
	float MaxDamageFallDistance = 1500.f;

	// --- Weapon Sway & Lag ---
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float MaxSwayYaw = 5.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float MaxSwayPitch = 2.f; // Keep vertical cap tighter

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float SwayMultiplierYaw = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float SwayMultiplierPitch = 0.015f; // Half as sensitive vertically

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float MaxMoveLag = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float MaxMoveLagBackward = 1.0f; // NEW: Hard limit to stop shoulder clipping

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float MoveLagMultiplier = 0.005f;

	UPROPERTY(EditDefaultsOnly, Category = "Camera|Weapon Sway")
	float SwayInterpSpeed = 12.f;

	FRotator LastCameraRotation;
	void UpdateWeaponSway(float DeltaTime);

	FRotator CurrentRecoil = FRotator::ZeroRotator;
	FRotator TargetRecoil = FRotator::ZeroRotator;

	void UpdateCameraRecoil(float DeltaTime);
	
private:
	UPROPERTY()
	ACorePlayerController* CorePlayerController = nullptr;

	UPROPERTY()
	UShooterMovementComponent* ShooterMovement = nullptr;

	bool bInterruptedReload = false;

	float TargetCameraHeight = 64.f;
	float CurrentCameraRoll = 0.f;
	bool bStartingSetupDone = false;

	void UpdateCamera(float DeltaTime);

	// Tracks how high we were when the smash started
	float SmashStartZ = 0.f;
};
