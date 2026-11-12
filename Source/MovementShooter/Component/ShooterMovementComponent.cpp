#include "MovementShooter/Component/ShooterMovementComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Particles/ParticleSystem.h"

UShooterMovementComponent::UShooterMovementComponent()
{
	NavAgentProps.bCanCrouch = true;
	MaxCustomMovementSpeed = WallRunSpeed;

	// Starting values for a fast first-person feel. All of them are editable on the component.
	MaxWalkSpeed = 650.f;
	MaxWalkSpeedCrouched = 325.f;
	MaxAcceleration = 2600.f;
	BrakingDecelerationWalking = 2600.f;
	GroundFriction = 8.f;
	AirControl = 0.7f;
	GravityScale = 1.6f;
}

void UShooterMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyJumpSettings();
}

void UShooterMovementComponent::ApplyJumpSettings()
{
	if (CharacterOwner)
	{
		CharacterOwner->JumpMaxCount = FMath::Max(1, MaxJumpCount);
	}

	if (bUseJumpHeight)
	{
		// v = sqrt(2 * g * h), using the gravity this character really falls with (includes Gravity Scale)
		const float Gravity = FMath::Abs(GetGravityZ());
		JumpZVelocity = FMath::Sqrt(2.f * Gravity * FMath::Max(JumpHeight, 0.f));
	}
}

float UShooterMovementComponent::GetMaxSpeed() const
{
	if (bIsSprinting && (IsMovingOnGround() || IsFalling()))
	{
		return SprintSpeed;
	}
	return Super::GetMaxSpeed();
}

#pragma region Helpers
void UShooterMovementComponent::ClampHorizontalSpeed(float MaxSpeed)
{
	FVector Flat(Velocity.X, Velocity.Y, 0.f);
	const float Speed = Flat.Size();
	if (Speed > MaxSpeed)
	{
		Flat *= MaxSpeed / Speed;
		Velocity.X = Flat.X;
		Velocity.Y = Flat.Y;
	}
}

FVector UShooterMovementComponent::GetFlatCameraForward() const
{
	FVector Forward = CharacterOwner ? CharacterOwner->GetControlRotation().Vector() : FVector::ForwardVector;
	Forward.Z = 0.f;
	return Forward.GetSafeNormal();
}

bool UShooterMovementComponent::IsHoldingForward() const
{
	// The wall run needs the player to keep pushing forward (the old code got this from the W key).
	if (!CharacterOwner) return false;
	return FVector::DotProduct(GetLastInputVector(), CharacterOwner->GetActorForwardVector()) > 0.1f;
}

float UShooterMovementComponent::GetGravityZ() const
{
	// No gravity while dashing, so an air dash travels flat.
	return bIsDashing ? 0.f : Super::GetGravityZ();
}
#pragma endregion Helpers

#pragma region Movement overrides
void UShooterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);

	if (CustomMovementMode == CMOVE_WallRun)
	{
		PhysWallRun(DeltaTime, Iterations);
	}
}

void UShooterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (bIsGroundSmashing)
	{
		Velocity.X = 0.f;
		Velocity.Y = 0.f;
		Velocity.Z = -GroundSmashSpeed;
		return;
	}
	
	if (bIsDashing)
	{
		// Constant horizontal velocity for the whole dash. Z is left alone (gravity is off, and it was zeroed for air dashes).
		Velocity.X = DashDirection.X * DashSpeed;
		Velocity.Y = DashDirection.Y * DashSpeed;
		return;
	}

	if (bIsSliding && IsMovingOnGround())
	{
		TickSlideVelocity(DeltaTime);
		return;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UShooterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);

	const float Now = GetWorld()->GetTimeSeconds();

	// Sprint: hold the button while pushing forward on the ground; the speed carries through a jump while it stays held
	const bool bPushingForward = CharacterOwner &&
		FVector::DotProduct(GetLastInputVector(), CharacterOwner->GetActorForwardVector()) > 0.1f;
	if (IsMovingOnGround())
	{
		bIsSprinting = bSprintEnabled && bSprintHeld && bSprintAllowed && bPushingForward
			&& !IsCrouching() && !bIsSliding && !bIsDashing;
	}
	else if (IsFalling())
	{
		bIsSprinting = bIsSprinting && bSprintEnabled && bSprintHeld;
	}
	else
	{
		bIsSprinting = false;
	}

	// Time in the air, and the automatic wall run
	AirTime = IsFalling() ? AirTime + DeltaSeconds : 0.f;
	if (bAutoWallRun && IsFalling() && bCanWallRun && !bIsWallRunning && !bIsDashing && AirTime >= AutoWallRunMinAirTime)
	{
		TryStartWallRun(true);
	}

	// Dash timer
	if (bIsDashing)
	{
		DashElapsed += DeltaSeconds;
		if (DashElapsed >= DashDuration)
		{
			EndDash();
		}
	}

	// Ground refreshes
	if (IsMovingOnGround())
	{
		bHasAirDashed = false;
		if (Now - LastWallRunEndTime >= WallRunCooldown)
		{
			bCanWallRun = true;
		}
	}

	// Buffered slide expires
	if (bSlideBuffered && Now > SlideBufferEndTime)
	{
		bSlideBuffered = false;
	}

	// Slide end conditions
	if (bIsSliding)
	{
		SlideElapsed += DeltaSeconds;
		const float Speed2D = Velocity.Size2D();
		
		// --- Removed the !bCrouchHeld check ---
		if (!IsMovingOnGround() || Speed2D < SlideEndSpeed || SlideElapsed >= SlideMaxDuration)
		{
			EndSlide();
		}
	}
}

void UShooterMovementComponent::ProcessLanded(const FHitResult& Hit, float RemainingTime, int32 Iterations)
{
	Super::ProcessLanded(Hit, RemainingTime, Iterations);

	if (bIsGroundSmashing)
	{
		bIsGroundSmashing = false;
		OnGroundSmashLanded.Broadcast();
		PlayMovementShake(GroundSmashShake, 1.5f); // Play a heavy camera shake!
	}
	
	const float Now = GetWorld()->GetTimeSeconds();
	bHasAirDashed = false;
	if (Now - LastWallRunEndTime >= WallRunCooldown)
	{
		bCanWallRun = true;
	}

	const bool bSlideOnLanding = bSlideBuffered && bCrouchHeld;

	// Momentum bleed after a wall run: the player lands with all their speed only if they are going to slide.
	if (bDidWallRunThisJump)
	{
		bDidWallRunThisJump = false;
		if (!bSlideOnLanding)
		{
			ClampHorizontalSpeed(GroundLandingSpeed);
		}
	}

	bSlideBuffered = false;
	if (bSlideOnLanding)
	{
		if (CanStartSlide())
		{
			StartSlide();
		}
		else if (CharacterOwner)
		{
			CharacterOwner->Crouch();
		}
	}
	PlayMovementShake(LandShake);
}

void UShooterMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	// The wall run was ended by something else (death, DisableMovement, a teleport...): clean up.
	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_WallRun && bIsWallRunning)
	{
		bIsWallRunning = false;
		LastWallRunEndTime = GetWorld()->GetTimeSeconds();
		OnWallRunEnded.Broadcast();
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}
#pragma endregion Movement overrides

#pragma region Wall run
bool UShooterMovementComponent::HandleJumpPressed()
{
	if (!CharacterOwner) return false;

	// 1. Already on a wall: jump off it
	if (bIsWallRunning)
	{
		if (GetWorld()->GetTimeSeconds() - WallRunStartTime >= 0.15f)
		{
			WallJump();
		}
		return true; // never do a normal jump off the wall
	}

	// 2. Only look for a wall while strictly falling (not while sliding, walking, ...)
	if (bCanWallRun && IsFalling())
	{
		return TryStartWallRun();
	}

	return false;
}

bool UShooterMovementComponent::IsValidWallHit(const FHitResult& Hit) const
{
	AActor* HitActor = Hit.GetActor();
	if (!HitActor) return false;
	if (HitActor->IsA<APawn>()) return false;                          // never run on characters
	if (Cast<USkeletalMeshComponent>(Hit.GetComponent())) return false; // never run on ragdolls
	return FMath::Abs(Hit.ImpactNormal.Z) < 0.2f;                      // must be (nearly) vertical
}

bool UShooterMovementComponent::TryStartWallRun(bool bAutomatic)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastWallRunEndTime < WallRunCooldown) return false;

	// The automatic version only grabs a wall the wall run would keep: the player must be pushing forward
	if (bAutomatic && !IsHoldingForward()) return false;

	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	if (Flat.Size() < MinWallSpeed) return false;

	const FVector Start = CharacterOwner->GetActorLocation();
	const FVector Right = CharacterOwner->GetActorRightVector();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	TArray<AActor*> AttachedActors;
	CharacterOwner->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);
	
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bRight = (Side == 0);
		const FVector End = Start + (bRight ? Right : -Right) * TraceDistance;

		FHitResult Hit;
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

		if (bShowDebugTraces)
		{
			DrawDebugLine(GetWorld(), Start, End, bRight ? FColor::Red : FColor::Blue, false, 0.1f, 0, 2.f);
		}

		if (bHit && IsValidWallHit(Hit))
		{
			if (bAutomatic)
			{
				const FVector Normal2D = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();

				// Never grab a wall we are moving away from
				if (FVector::DotProduct(Flat.GetSafeNormal(), Normal2D) > 0.3f) continue;

				// And only if we are facing along it (otherwise the wall run would end right away)
				const FVector WallTangent = (bRight
					? FVector::CrossProduct(FVector::UpVector, Normal2D)
					: FVector::CrossProduct(Normal2D, FVector::UpVector)).GetSafeNormal();
				if (FVector::DotProduct(GetFlatCameraForward(), WallTangent) < -0.1f) continue;
			}

			StartWallRun(Hit.ImpactNormal, bRight);
			return true;
		}
	}

	return false;
}

void UShooterMovementComponent::StartWallRun(const FVector& WallNormal, bool bRightWall)
{
	bIsWallRunning = true;
	bIsRightWall = bRightWall;
	bDidWallRunThisJump = true;
	WallRunStartTime = GetWorld()->GetTimeSeconds();

	CurrentWallNormal = FVector(WallNormal.X, WallNormal.Y, 0.f).GetSafeNormal();
	const FVector Tangent = bRightWall
		? FVector::CrossProduct(FVector::UpVector, CurrentWallNormal)
		: FVector::CrossProduct(CurrentWallNormal, FVector::UpVector);
	CurrentWallTangent = Tangent.GetSafeNormal();

	// Attaching to a wall refreshes the air dash once (not every tick while attached).
	bHasAirDashed = false;

	if (bIsDashing) EndDash();
	if (bIsSliding) EndSlide();

	// Start running along the wall with a small upward kick, pressed into the wall
	Velocity = CurrentWallTangent * WallRunSpeed + CurrentWallNormal * (-WallRunAdhesionSpeed);
	Velocity.Z = WallRunUpAssist;

	SetMovementMode(MOVE_Custom, CMOVE_WallRun);

	OnWallRunStarted.Broadcast(bRightWall);

	PlayMovementShake(WallRunShake);
}

bool UShooterMovementComponent::UpdateWallRunState()
{
	const float Now = GetWorld()->GetTimeSeconds();

	// Let go of forward: drop off the wall
	if (!IsHoldingForward())
	{
		bCanWallRun = false;
		return false;
	}

	// Don't test the wall again right after attaching
	if (Now - WallRunStartTime < MinWallRunCheckDelay) return true;

	// Looking against the direction of travel: drop off
	if (FVector::DotProduct(GetFlatCameraForward(), CurrentWallTangent) < -0.1f)
	{
		bCanWallRun = false;
		return false;
	}

	// Stalled
	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	if (Flat.Size() < 10.f)
	{
		bCanWallRun = false;
		return false;
	}

	// Is the wall still there?
	const FVector Start = CharacterOwner->GetActorLocation();
	const FVector Right = CharacterOwner->GetActorRightVector();
	const FVector End = Start + (bIsRightWall ? Right : -Right) * TraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CharacterOwner);

	TArray<AActor*> AttachedActors;
	CharacterOwner->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	if (!bHit || !IsValidWallHit(Hit))
	{
		bCanWallRun = false; // tire them out so they don't glitch on a corner
		return false;
	}

	// Follow gently curved walls
	CurrentWallNormal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
	const FVector Tangent = bIsRightWall
		? FVector::CrossProduct(FVector::UpVector, CurrentWallNormal)
		: FVector::CrossProduct(CurrentWallNormal, FVector::UpVector);
	CurrentWallTangent = Tangent.GetSafeNormal();

	return true;
}

void UShooterMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < 1e-6f) return;

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity()))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdateWallRunState())
	{
		ExitWallRun(false);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	Iterations++;
	bJustTeleported = false;

	// Along the wall at a fixed speed, pressed into it; vertical speed follows reduced gravity.
	const float NewZ = Velocity.Z + GetGravityZ() * WallRunGravityScale * DeltaTime;
	Velocity = CurrentWallTangent * WallRunSpeed + CurrentWallNormal * (-WallRunAdhesionSpeed);
	Velocity.Z = NewZ;

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Delta = Velocity * DeltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

	if (Hit.Time < 1.f)
	{
		// Reached the ground: leave the wall and let normal falling physics land us
		if (Hit.IsValidBlockingHit() && IsWalkable(Hit))
		{
			ExitWallRun(false);
			StartNewPhysics(DeltaTime * (1.f - Hit.Time), Iterations);
			return;
		}

		HandleImpact(Hit, DeltaTime, Delta);
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, true);
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime;
	}
}

void UShooterMovementComponent::ExitWallRun(bool bFromJump)
{
	if (!bIsWallRunning) return;

	bIsWallRunning = false;
	LastWallRunEndTime = GetWorld()->GetTimeSeconds();

	if (MovementMode == MOVE_Custom && CustomMovementMode == CMOVE_WallRun)
	{
		SetMovementMode(MOVE_Falling);
	}

	// Leaving the wall without jumping caps the speed so the player doesn't fly off at full wall-run speed
	if (!bFromJump)
	{
		ClampHorizontalSpeed(AirExitMaxSpeed);
	}

	OnWallRunEnded.Broadcast();
}

void UShooterMovementComponent::WallJump()
{
	// Out from the wall and toward where the camera looks, plus a hop up
	const FVector JumpDirection = (CurrentWallNormal * 0.3f + GetFlatCameraForward() * 0.7f).GetSafeNormal();
	const FVector LaunchVelocity = (JumpDirection * JumpOutSpeed) + (FVector::UpVector * JumpUpSpeed);

	bCanWallRun = true;
	ExitWallRun(true);

	// A wall jump counts as the first jump: one more (double) jump stays available afterwards
	if (CharacterOwner)
	{
		CharacterOwner->JumpCurrentCount = FMath::Min(CharacterOwner->JumpCurrentCount, FMath::Max(0, CharacterOwner->JumpMaxCount - 1));
	}

	Velocity = LaunchVelocity;
	SetMovementMode(MOVE_Falling);
}
#pragma endregion Wall run

#pragma region Dash
void UShooterMovementComponent::TryDash(FVector WorldInputDirection)
{
	if (!CharacterOwner) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDashTime < DashCooldown) return;

	const bool bGrounded = IsMovingOnGround();
	if (!bGrounded && bHasAirDashed) return; // one air dash per jump

	// Dash where the player is pushing; with no input, dash backward
	FVector Direction = WorldInputDirection;
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		Direction = -CharacterOwner->GetControlRotation().Vector();
	}
	Direction.Z = 0.f;
	Direction = Direction.GetSafeNormal();
	if (Direction.IsNearlyZero()) return;

	if (bIsWallRunning) ExitWallRun(true);
	if (bIsSliding) EndSlide();

	PreDashSpeed = Velocity.Size2D();
	bIsDashing = true;
	DashElapsed = 0.f;
	LastDashTime = Now;
	DashDirection = Direction;

	if (!bGrounded)
	{
		bHasAirDashed = true;
		Velocity.Z = 0.f; // fly flat (gravity is off while dashing)
	}
	Velocity.X = Direction.X * DashSpeed;
	Velocity.Y = Direction.Y * DashSpeed;

	PlayDashFX(Direction);
	OnDashStarted.Broadcast(Direction);
}

void UShooterMovementComponent::EndDash()
{
	if (!bIsDashing) return;
	bIsDashing = false;

	// Scale the 2000.f dash velocity down to exactly the speed you had before the dash.
	// We use FMath::Max so if you dash from a complete standstill (0 speed), 
	// you still get a little bit of slide at the end (PostDashLandingSpeed).
	ClampHorizontalSpeed(FMath::Max(PreDashSpeed, PostDashLandingSpeed));

	EndDashFX();
	OnDashEnded.Broadcast();
}

void UShooterMovementComponent::PlayDashFX(const FVector& Direction)
{
	if (!CharacterOwner) return;

	USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	UWorld* World = GetWorld();

	// 1. Directional animation
	const FVector Forward = CharacterOwner->GetActorForwardVector();
	const FVector Right = CharacterOwner->GetActorRightVector();
	const float ForwardDot = FVector::DotProduct(Forward, Direction);
	const float RightDot = FVector::DotProduct(Right, Direction);

	UAnimMontage* MontageToPlay;
	if (FMath::Abs(ForwardDot) > FMath::Abs(RightDot))
	{
		MontageToPlay = (ForwardDot > 0.f) ? DashForwardAnim : DashBackwardAnim;
	}
	else
	{
		MontageToPlay = (RightDot > 0.f) ? DashRightAnim : DashLeftAnim;
	}

	if (MontageToPlay && Mesh)
	{
		if (UAnimInstance* AnimInst = Mesh->GetAnimInstance())
		{
			AnimInst->Montage_Play(MontageToPlay);
		}
	}

	// 2. Start VFX
	if (DashStartVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DashStartVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}
	if (DashStartCascadeVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, DashStartCascadeVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}

	// 3. Start audio
	if (DashStartSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, DashStartSound, CharacterOwner->GetActorLocation());
	}

	// 4. Looping trail VFX and audio
	if (DashTrailVFX && Mesh)
	{
		ActiveTrailVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(DashTrailVFX, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
	if (DashTrailSound && Mesh)
	{
		ActiveTrailSound = UGameplayStatics::SpawnSoundAttached(DashTrailSound, Mesh);
	}

	// 5. Afterimages: one now, then every interval for the rest of the dash
	if (AfterimageVFX)
	{
		SpawnAfterimage();
		World->GetTimerManager().SetTimer(AfterimageTimer, this, &UShooterMovementComponent::SpawnAfterimage, AfterimageInterval, true);
	}

	PlayMovementShake(DashShake);
}

void UShooterMovementComponent::TryGroundSmash()
{
	if (!CharacterOwner || !IsFalling() || bIsGroundSmashing) return;

	if (bIsWallRunning) ExitWallRun(true);
	if (bIsDashing) EndDash();
	if (bIsSliding) EndSlide();

	bIsGroundSmashing = true;
	
	// Kill forward momentum and drop instantly
	Velocity.X = 0.f;
	Velocity.Y = 0.f;
	Velocity.Z = -GroundSmashSpeed;
}

void UShooterMovementComponent::EndDashFX()
{
	if (!CharacterOwner) return;
	UWorld* World = GetWorld();

	if (ActiveTrailVFX)
	{
		ActiveTrailVFX->Deactivate();
		ActiveTrailVFX = nullptr;
	}
	if (ActiveTrailSound)
	{
		ActiveTrailSound->Stop();
		ActiveTrailSound = nullptr;
	}

	World->GetTimerManager().ClearTimer(AfterimageTimer);

	if (DashEndVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, DashEndVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}
	if (DashEndCascadeVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, DashEndCascadeVFX, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorRotation());
	}
	if (DashEndSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, DashEndSound, CharacterOwner->GetActorLocation());
	}
}

void UShooterMovementComponent::SpawnAfterimage()
{
	if (!CharacterOwner || !AfterimageVFX || !CharacterOwner->GetMesh()) return;

	// Spawned attached so the Niagara skeletal mesh module can sample the current pose
	UNiagaraFunctionLibrary::SpawnSystemAttached(AfterimageVFX, CharacterOwner->GetMesh(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
}
#pragma endregion Dash

#pragma region Slide
bool UShooterMovementComponent::CanStartSlide() const
{
	return CharacterOwner && IsMovingOnGround() && !bIsSliding && !bIsDashing && Velocity.Size2D() >= SlideEnterSpeed;
}

void UShooterMovementComponent::CrouchPressed()
{
	if (!CharacterOwner) return;
	bCrouchHeld = true;

	// Slide turned off: crouch is just a crouch
	if (!bSlideEnabled)
	{
		CharacterOwner->Crouch();
		return;
	}

	// In the air (or on a wall): remember the press, so landing slides
	if (IsFalling() || bIsWallRunning)
	{
		bSlideBuffered = true;
		SlideBufferEndTime = GetWorld()->GetTimeSeconds() + SlideBufferTime;
		return;
	}

	if (CanStartSlide())
	{
		StartSlide();
	}
	else
	{
		CharacterOwner->Crouch();
	}
}

void UShooterMovementComponent::CrouchReleased()
{
	if (!CharacterOwner) return;
	bCrouchHeld = false;
	bSlideBuffered = false;

	// Only uncrouch if we are NOT sliding
	if (!bIsSliding)
	{
		CharacterOwner->UnCrouch();
	}
}

void UShooterMovementComponent::StartSlide()
{
	bIsSliding = true;
	SlideElapsed = 0.f;
	bSlideBuffered = false;

	bWantsToCrouch = true; // the capsule shrinks for the slide

	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	float Speed = Flat.Size();
	const FVector Direction = Speed > KINDA_SMALL_NUMBER ? Flat / Speed : CharacterOwner->GetActorForwardVector();

	Speed = FMath::Min(Speed + SlideBoostSpeed, FMath::Max(SlideMaxSpeed, Speed));
	Velocity.X = Direction.X * Speed;
	Velocity.Y = Direction.Y * Speed;

	OnSlideStarted.Broadcast();

	PlayMovementShake(SlideShake);
}

void UShooterMovementComponent::CancelMovementAbilities()
{
	if (bIsDashing) EndDash();
	EndSlide();
	ExitWallRun(true);
	bSlideBuffered = false;
	bCrouchHeld = false;
	bDidWallRunThisJump = false;
	bIsSprinting = false;
}

void UShooterMovementComponent::EndSlide()
{
	if (!bIsSliding) return;
	bIsSliding = false;
	OnSlideEnded.Broadcast();

	// --- Automatically stand up if the player isn't holding the crouch button ---
	if (CharacterOwner && !bCrouchHeld)
	{
		CharacterOwner->UnCrouch();
	}
}

void UShooterMovementComponent::TickSlideVelocity(float DeltaTime)
{
	FVector Flat(Velocity.X, Velocity.Y, 0.f);
	float Speed = Flat.Size();
	FVector Direction = Speed > KINDA_SMALL_NUMBER ? Flat / Speed : CharacterOwner->GetActorForwardVector();

	// Steer slowly toward the move input
	FVector Input = GetLastInputVector();
	Input.Z = 0.f;
	if (!Input.IsNearlyZero())
	{
		const FVector Wanted = Input.GetSafeNormal();
		const float AngleToInput = FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction, Wanted), -1.f, 1.f));
		const float StepAngle = FMath::Min(AngleToInput, FMath::DegreesToRadians(SlideSteerRate) * DeltaTime);
		if (StepAngle > KINDA_SMALL_NUMBER)
		{
			const float Sign = FVector::CrossProduct(Direction, Wanted).Z >= 0.f ? 1.f : -1.f;
			Direction = Direction.RotateAngleAxisRad(Sign * StepAngle, FVector::UpVector).GetSafeNormal();
		}
	}

	// Gravity along the floor speeds the slide up going downhill and slows it going uphill
	const FVector FloorNormal = CurrentFloor.IsWalkableFloor() ? CurrentFloor.HitResult.ImpactNormal : FVector::UpVector;
	const FVector SlopeAcceleration = FVector::VectorPlaneProject(FVector(0.f, 0.f, GetGravityZ()), FloorNormal) * SlideSlopeFactor;
	const float AlongDirection = FVector::DotProduct(SlopeAcceleration, Direction);

	Speed += (AlongDirection - SlideDeceleration) * DeltaTime;
	Speed = FMath::Clamp(Speed, 0.f, SlideMaxSpeed);

	Velocity.X = Direction.X * Speed;
	Velocity.Y = Direction.Y * Speed;
}
#pragma endregion Slide

void UShooterMovementComponent::PlayMovementShake(TSubclassOf<UCameraShakeBase> ShakeClass, float Scale)
{
	if (!ShakeClass || !CharacterOwner) return;
	
	if (APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController()))
	{
		PC->ClientStartCameraShake(ShakeClass, Scale);
	}
}

bool UShooterMovementComponent::DoJump(bool bReplayingMoves)
{
	bool bDidJump = Super::DoJump(bReplayingMoves);
	
	// If the jump was successful and this isn't a network playback frame, play the shake
	if (bDidJump && !bReplayingMoves)
	{
		PlayMovementShake(JumpShake);
	}
	
	return bDidJump;
}