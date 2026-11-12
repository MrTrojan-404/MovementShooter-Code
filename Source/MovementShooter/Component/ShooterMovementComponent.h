// MovementShooter - custom CharacterMovementComponent: wall run, wall jump, slide and dash.
// Single-player, so there are no saved moves or server corrections; everything runs inside the normal
// movement tick (wall run is a custom movement mode, dash and slide override the velocity calculation).
//
// Ported from the old Mover-based WallRunComponent and DashComponent. The slide is new: in the old project
// it lived in the GASP animation Blueprint, so the numbers below are fresh defaults to tune.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShooterMovementComponent.generated.h"

class UAnimMontage;
class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class UParticleSystem;

// Custom movement modes (used with MOVE_Custom)
enum EShooterCustomMovementMode : uint8
{
	CMOVE_None = 0,
	CMOVE_WallRun = 1,
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDashSignature, FVector, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallRunStartedSignature, bool, bRightWall);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMovementAbilitySignature);

UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class MOVEMENTSHOOTER_API UShooterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShooterMovementComponent();

#pragma region Input entry points (called by the player character)
	// Jump button. Returns true if the press was used by the wall run (started one, or jumped off one).
	// When it returns false the caller should do a normal jump.
	bool HandleJumpPressed();

	// Dash button. WorldInputDirection is the current move input in world space (may be zero).
	UFUNCTION(BlueprintCallable, Category = "Movement|Dash")
	void TryDash(FVector WorldInputDirection);

	// Crouch button: slides when fast enough on the ground, buffers a slide in the air, crouches otherwise.
	void CrouchPressed();
	void CrouchReleased();

	void EndSlide();

	// Sprint button held / released
	void SetSprintHeld(bool bHeld) { bSprintHeld = bHeld; }

	// The player character turns this off while aiming, so aiming slows you down
	void SetSprintAllowed(bool bAllowed) { bSprintAllowed = bAllowed; }

	// Ends any dash, slide, wall run and buffered input (used on death and on checkpoint restore).
	void CancelMovementAbilities();
#pragma endregion

#pragma region State
	UFUNCTION(BlueprintPure, Category = "Movement|WallRun")
	bool IsWallRunning() const { return bIsWallRunning; }

	UFUNCTION(BlueprintPure, Category = "Movement|WallRun")
	bool IsRightWall() const { return bIsRightWall; }

	UFUNCTION(BlueprintPure, Category = "Movement|Dash")
	bool IsDashing() const { return bIsDashing; }

	UFUNCTION(BlueprintPure, Category = "Movement|Slide")
	bool IsSliding() const { return bIsSliding; }

	UFUNCTION(BlueprintPure, Category = "Movement|Sprint")
	bool IsSprinting() const { return bIsSprinting; }
#pragma endregion

#pragma region Events
	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnDashSignature OnDashStarted;

	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementAbilitySignature OnDashEnded;

	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnWallRunStartedSignature OnWallRunStarted;

	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementAbilitySignature OnWallRunEnded;

	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementAbilitySignature OnSlideStarted;

	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementAbilitySignature OnSlideEnded;
#pragma endregion

#pragma region Walk, sprint and jump tuning
	// Walk speed is the stock "Max Walk Speed" on this component (and "Max Walk Speed Crouched" for crouching).
	// Acceleration, braking, friction, air control and gravity scale are the stock values on this component too.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Sprint")
	bool bSprintEnabled = true;

	// Top speed while sprinting (hold the sprint button while pushing forward)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Sprint")
	float SprintSpeed = 1000.f;

	// When on, the jump strength is worked out from JumpHeight (and gravity) and replaces "Jump Z Velocity".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump")
	bool bUseJumpHeight = true;

	// How high a single jump goes, in unreal units
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump", meta = (EditCondition = "bUseJumpHeight"))
	float JumpHeight = 200.f;

	// Jumps before landing (2 = double jump). A wall jump gives one back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump", meta = (ClampMin = "1"))
	int32 MaxJumpCount = 2;

	// Re-applies JumpHeight / MaxJumpCount (call it after changing them at runtime)
	UFUNCTION(BlueprintCallable, Category = "Movement|Jump")
	void ApplyJumpSettings();

	// Turn the slide off to get a plain crouch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	bool bSlideEnabled = true;
#pragma endregion

#pragma region Wall run settings
	// Grab walls automatically while airborne next to one (pushing forward, moving along or into the wall).
	// The jump press still starts a wall run by hand as a fallback.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	bool bAutoWallRun = true;

	// Seconds in the air before the automatic wall run may trigger (so it doesn't grab the instant you leave the floor)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float AutoWallRunMinAirTime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float TraceDistance = 60.f;

	// Minimum horizontal speed needed to attach to a wall
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float MinWallSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float WallRunSpeed = 1200.f;

	// Speed pushing the character into the wall so it stays attached
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float WallRunAdhesionSpeed = 200.f;

	// Upward speed given when the wall run starts; gravity then pulls it back down (see WallRunGravityScale)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float WallRunUpAssist = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float WallRunGravityScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float WallRunCooldown = 0.2f;

	// Seconds after attaching before the wall is checked again
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float MinWallRunCheckDelay = 0.12f;

	// Horizontal speed cap when leaving a wall without jumping
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float AirExitMaxSpeed = 650.f;

	// Horizontal speed cap when landing after a wall run (skipped when a slide is buffered)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun")
	float GroundLandingSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun|Jump")
	float JumpOutSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun|Jump")
	float JumpUpSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|WallRun|Debug")
	bool bShowDebugTraces = false;
#pragma endregion

#pragma region Dash settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
	float DashSpeed = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
	float DashDuration = 0.27f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
	float DashCooldown = 0.6f;

	// Horizontal speed cap applied when the dash ends
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
	float PostDashLandingSpeed = 600.f;

	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Animation") UAnimMontage* DashForwardAnim = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Animation") UAnimMontage* DashBackwardAnim = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Animation") UAnimMontage* DashLeftAnim = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Animation") UAnimMontage* DashRightAnim = nullptr;

	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals") UNiagaraSystem* DashStartVFX = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals") UNiagaraSystem* DashEndVFX = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals") UNiagaraSystem* DashTrailVFX = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals") UNiagaraSystem* AfterimageVFX = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals") float AfterimageInterval = 0.085f;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals|Cascade") UParticleSystem* DashStartCascadeVFX = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Visuals|Cascade") UParticleSystem* DashEndCascadeVFX = nullptr;

	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Audio") USoundBase* DashStartSound = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Audio") USoundBase* DashEndSound = nullptr;
	UPROPERTY(EditAnywhere, Category = "Movement|Dash|Audio") USoundBase* DashTrailSound = nullptr;
#pragma endregion

#pragma region Slide settings (new defaults, tune to taste)
	// Horizontal speed needed on the ground for crouch to start a slide instead of a plain crouch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideEnterSpeed = 500.f;

	// Extra speed added when the slide starts
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideBoostSpeed = 150.f;

	// Extra horizontal speed added when jumping out of a slide
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideJumpBoost = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideMaxSpeed = 1300.f;

	// Speed lost per second while sliding on flat ground
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideDeceleration = 350.f;

	// How much of gravity along the slope speeds up (downhill) or slows down (uphill) the slide
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideSlopeFactor = 1.f;

	// The slide ends below this speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideEndSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideMaxDuration = 1.5f;

	// Degrees per second the slide direction can turn toward the move input
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideSteerRate = 60.f;

	// Crouch pressed this many seconds before landing still starts a slide on landing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Slide")
	float SlideBufferTime = 0.4f;
#pragma endregion

	UFUNCTION(BlueprintCallable, Category = "Movement|GroundSmash")
	void TryGroundSmash();

	UFUNCTION(BlueprintPure, Category = "Movement|GroundSmash")
	bool IsGroundSmashing() const { return bIsGroundSmashing; }

	UPROPERTY(BlueprintAssignable, Category = "Movement|Events")
	FOnMovementAbilitySignature OnGroundSmashLanded;

	// How fast the character rockets straight down
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundSmash")
	float GroundSmashSpeed = 4000.f;
	
protected:
	virtual void BeginPlay() override;
	virtual float GetMaxSpeed() const override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual float GetGravityZ() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;
	virtual bool DoJump(bool bReplayingMoves) override;
	
#pragma region Camera Shakes
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|CameraShakes")
	TSubclassOf<class UCameraShakeBase> JumpShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|CameraShakes")
	TSubclassOf<class UCameraShakeBase> LandShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|CameraShakes")
	TSubclassOf<class UCameraShakeBase> DashShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|CameraShakes")
	TSubclassOf<class UCameraShakeBase> SlideShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|CameraShakes")
	TSubclassOf<class UCameraShakeBase> WallRunShake;

	// Helper to safely trigger shakes
	void PlayMovementShake(TSubclassOf<class UCameraShakeBase> ShakeClass, float Scale = 1.0f);
#pragma endregion

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|CameraShakes")
	TSubclassOf<class UCameraShakeBase> GroundSmashShake;
	
private:
	bool bSprintHeld = false;
	bool bSprintAllowed = true;
	bool bIsSprinting = false;
	float AirTime = 0.f;
	float PreDashSpeed = 0.f;
	
#pragma region Wall run
	bool bIsWallRunning = false;
	bool bIsRightWall = false;
	bool bCanWallRun = true;
	bool bDidWallRunThisJump = false;
	FVector CurrentWallNormal = FVector::ZeroVector;
	FVector CurrentWallTangent = FVector::ZeroVector;
	float WallRunStartTime = 0.f;
	float LastWallRunEndTime = -100.f;

	bool TryStartWallRun(bool bAutomatic = false);
	void StartWallRun(const FVector& WallNormal, bool bRightWall);
	void ExitWallRun(bool bFromJump);
	void WallJump();
	void PhysWallRun(float DeltaTime, int32 Iterations);
	bool UpdateWallRunState();   // returns false when the wall run should end
	bool IsValidWallHit(const FHitResult& Hit) const;
	bool IsHoldingForward() const;
	FVector GetFlatCameraForward() const;
#pragma endregion

#pragma region Dash
	bool bIsDashing = false;
	bool bHasAirDashed = false;
	float DashElapsed = 0.f;
	float LastDashTime = -100.f;
	FVector DashDirection = FVector::ZeroVector;

	FTimerHandle AfterimageTimer;

	UPROPERTY()
	UNiagaraComponent* ActiveTrailVFX = nullptr;

	UPROPERTY()
	UAudioComponent* ActiveTrailSound = nullptr;

	void EndDash();
	void PlayDashFX(const FVector& Direction);
	void EndDashFX();
	void SpawnAfterimage();
#pragma endregion

#pragma region Slide
	bool bIsSliding = false;
	bool bCrouchHeld = false;
	bool bSlideBuffered = false;
	float SlideBufferEndTime = 0.f;
	float SlideElapsed = 0.f;

	bool CanStartSlide() const;
	void StartSlide();
	void TickSlideVelocity(float DeltaTime);
#pragma endregion

	void ClampHorizontalSpeed(float MaxSpeed);

	bool bIsGroundSmashing = false;
	
};
