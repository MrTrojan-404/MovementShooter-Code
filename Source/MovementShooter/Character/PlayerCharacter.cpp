#include "MovementShooter/Character/PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PawnNoiseEmitterComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/Actor/Weapon.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/Component/InteractionComponent.h"
#include "MovementShooter/Component/ShooterMovementComponent.h"
#include "MovementShooter/Component/StealthComponent.h"
#include "MovementShooter/GameVariables/CombatState.h"
#include "MovementShooter/GameMode/MissionGameMode.h"
#include "MovementShooter/MovementShooter.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"
#include "MovementShooter/UI/HUD/ShooterHUD.h"
#include "MovementShooter/UI/Widgets/CharacterOverlay.h"

APlayerCharacter::APlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UShooterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	NoiseEmitterComponent = CreateDefaultSubobject<UPawnNoiseEmitterComponent>(TEXT("NoiseEmitterComponent"));
	StealthComponent = CreateDefaultSubobject<UStealthComponent>(TEXT("StealthComponent"));

	// First-person camera rides on the capsule at eye height and follows the controller rotation.
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;

	// The character mesh is the first-person arms: attached to the camera, seen only by the owner.
	GetMesh()->SetupAttachment(FirstPersonCamera);
	GetMesh()->SetOnlyOwnerSee(true);
	GetMesh()->bCastDynamicShadow = false;
	GetMesh()->CastShadow = false;

	// The body never turns by itself in first person: the controller drives the view.
	bUseControllerRotationYaw = true;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	TargetCameraHeight = StandingCameraHeight;

	// Sword: a mesh on the arms that only shows during a slash
	SwordMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(GetMesh(), SwordSocketName);
	SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SwordMesh->SetOnlyOwnerSee(true);
	SwordMesh->CastShadow = false;
	SwordMesh->SetVisibility(false);

	// Player melee is a sword slash: longer reach and a wider swing than the weapon bash.
	// (Tune all of these on the CombatComponent in the player Blueprint.)
	if (CombatComponent)
	{
		CombatComponent->SetMeleeStats(/*Damage*/ 75.f, /*Range*/ 220.f, /*Radius*/ 70.f);
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	TargetCameraHeight = StandingCameraHeight;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	// Keep the head out of the first-person view
	if (bHideHeadBone && GetMesh() && GetMesh()->GetBoneIndex(HeadBoneName) != INDEX_NONE)
	{
		GetMesh()->HideBoneByName(HeadBoneName, PBO_None);
	}

	ShooterMovement = Cast<UShooterMovementComponent>(GetCharacterMovement());
	if (ShooterMovement)
	{
		ShooterMovement->OnDashStarted.AddDynamic(this, &APlayerCharacter::HandleDashStarted);
		ShooterMovement->OnDashEnded.AddDynamic(this, &APlayerCharacter::HandleDashEnded);
		ShooterMovement->OnWallRunStarted.AddDynamic(this, &APlayerCharacter::HandleWallRunStarted);
		ShooterMovement->OnGroundSmashLanded.AddDynamic(this, &APlayerCharacter::HandleGroundSmashLanded);
	}

	if (FirstPersonCamera)
	{
		LastCameraRotation = FirstPersonCamera->GetComponentRotation();
	}
}

void APlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PollInit();
	UpdateCamera(DeltaTime);
	UpdateWeaponSway(DeltaTime);
	UpdateCameraRecoil(DeltaTime);

	// Aiming (or being locked out) cancels sprint
	if (ShooterMovement)
	{
		const bool bAiming = CombatComponent && CombatComponent->GetIsAiming();
		ShooterMovement->SetSprintAllowed(!bDisableGameplay && !bElimmed && !bAiming);
	}

	// Hide the sword once the slash is over (also covers an interrupted montage)
	if (SwordMesh && SwordMesh->IsVisible() && CombatComponent && CombatComponent->GetCombatState() != ECombatState::ECS_Melee)
	{
		SwordMesh->SetVisibility(false);
	}
}

void APlayerCharacter::PollInit()
{
	if (CorePlayerController == nullptr)
	{
		CorePlayerController = Cast<ACorePlayerController>(Controller);
		if (CorePlayerController && !bStartingSetupDone)
		{
			bStartingSetupDone = true;
			SpawnDefaultWeapon(true, bElimmed);
			UpdateHUDAmmo();
			UpdateHUDHealth();
		}
	}
}

#pragma region Camera

void APlayerCharacter::UpdateCamera(float DeltaTime)
{
    if (!FirstPersonCamera) return;

    // 1. Crouch / Stand height (Base Location)
    // Because we are no longer applying the bob to this base location, it will never jitter or drift!
    FVector Location = FirstPersonCamera->GetRelativeLocation();
    Location.Z = FMath::FInterpTo(Location.Z, TargetCameraHeight, DeltaTime, CameraHeightInterpSpeed);
    FirstPersonCamera->SetRelativeLocation(Location);

    // 2. FOV Interpolation
    float TargetFOV = DefaultFOV;
    if (CombatComponent && CombatComponent->GetIsAiming() && !bElimmed)
    {
       AWeapon* Primary = CombatComponent->GetPrimaryWeapon();
       const bool bSniper = Primary && Primary->GetWeaponType() == EWeaponType::EWT_SniperRifle;
       TargetFOV = bSniper ? SniperAimFOV : ((Primary && Primary->AimFOV > 0.f) ? Primary->AimFOV : AimFOV);
    }
    FirstPersonCamera->SetFieldOfView(FMath::FInterpTo(FirstPersonCamera->FieldOfView, TargetFOV, DeltaTime, FOVInterpSpeed));

    // 3. Wall-run roll
    float TargetRoll = 0.f;
    if (ShooterMovement && ShooterMovement->IsWallRunning() && !bElimmed)
    {
       TargetRoll = ShooterMovement->IsRightWall() ? -WallRunCameraRoll : WallRunCameraRoll;
    }
    CurrentCameraRoll = FMath::FInterpTo(CurrentCameraRoll, TargetRoll, DeltaTime, CameraRollInterpSpeed);

	// 4. Procedural Head Bob Math
	float Speed2D = GetVelocity().Size2D();
    
	// Only bob if we are walking/running (NOT sliding, NOT dashing, NOT standing still)
	bool bIsBobbingState = ShooterMovement && ShooterMovement->IsMovingOnGround() && 
						   !ShooterMovement->IsSliding() && !ShooterMovement->IsDashing() && 
						   Speed2D > 10.f;

	// Constantly flow time forward as long as we have speed
	if (Speed2D > 10.f)
	{
		HeadBobTime += DeltaTime * (Speed2D / 600.f); 
	}

	// TSmoothly fade the bob effect in and out instead of snapping the timer
	float TargetWeight = bIsBobbingState ? 1.0f : 0.0f;
	CurrentBobWeight = FMath::FInterpTo(CurrentBobWeight, TargetWeight, DeltaTime, 15.f);

	// Apply the weight multiplier to the final calculation
	float BobOffsetZ = FMath::Sin(HeadBobTime * BobFrequency) * BobAmplitude * CurrentBobWeight;
	float BobOffsetY = FMath::Cos(HeadBobTime * (BobFrequency * 0.5f)) * (BobAmplitude * 0.5f) * CurrentBobWeight;

	// 5. Apply Roll and Bob as a temporary Additive Offset
	FirstPersonCamera->ClearAdditiveOffset();
    
	FTransform AdditiveOffset;
	AdditiveOffset.SetRotation(FQuat(FRotator(0.f, 0.f, CurrentCameraRoll)));
	AdditiveOffset.SetLocation(FVector(0.f, BobOffsetY, BobOffsetZ));
    
	FirstPersonCamera->AddAdditiveOffset(AdditiveOffset, 0.f);
}

void APlayerCharacter::UpdateWeaponSway(float DeltaTime)
{
	if (!FirstPersonCamera || !GetMesh()) return;

	// 1. Look Sway (Rotation based on Mouse/Camera Delta)
	FRotator CurrentCamRot = FirstPersonCamera->GetComponentRotation();
	FRotator DeltaRot = CurrentCamRot - LastCameraRotation;
	DeltaRot.Normalize();
	LastCameraRotation = CurrentCamRot;

	FRotator RotRate = DeltaRot * (1.f / DeltaTime);

	// Separated Horizontal and Vertical controls!
	float TargetYaw = FMath::Clamp(-RotRate.Yaw * SwayMultiplierYaw, -MaxSwayYaw, MaxSwayYaw);
	float TargetPitch = FMath::Clamp(-RotRate.Pitch * SwayMultiplierPitch, -MaxSwayPitch, MaxSwayPitch);
	float TargetRoll = TargetYaw * 0.5f;

	FRotator TargetSwayRot(TargetPitch, TargetYaw, TargetRoll);

	// 2. Movement Lag (Location based on Local Velocity)
	FVector LocalVelocity = GetActorTransform().InverseTransformVectorNoScale(GetVelocity());
	
	// ASYMMETRICAL CLAMP: Forward (+X) allows MaxMoveLag, Backward (-X) is strictly limited by MaxMoveLagBackward
	float TargetX = FMath::Clamp(-LocalVelocity.X * MoveLagMultiplier, -MaxMoveLagBackward, MaxMoveLag);
	float TargetY = FMath::Clamp(-LocalVelocity.Y * MoveLagMultiplier, -MaxMoveLag, MaxMoveLag);
	float TargetZ = FMath::Clamp(-LocalVelocity.Z * MoveLagMultiplier, -MaxMoveLag, MaxMoveLag);

	FVector TargetSwayLoc(TargetX, TargetY, TargetZ);

	// 3. Dampen sway when Aiming Down Sights
	if (CombatComponent && CombatComponent->GetIsAiming())
	{
		TargetSwayRot *= 0.1f; 
		TargetSwayLoc *= 0.1f;
	}

	// 4. Interpolate and Apply Sway
	FRotator BaseRot = DefaultMeshRelativeTransform.Rotator();
	FVector BaseLoc = DefaultMeshRelativeTransform.GetLocation();

	FRotator NewRot = FMath::RInterpTo(GetMesh()->GetRelativeRotation(), BaseRot + TargetSwayRot, DeltaTime, SwayInterpSpeed);
	FVector NewLoc = FMath::VInterpTo(GetMesh()->GetRelativeLocation(), BaseLoc + TargetSwayLoc, DeltaTime, SwayInterpSpeed);

	// 5. INJECT VISUAL RECOIL!
	// We add it directly to the final smoothed value so the recoil kick is instant and snappy,
	// while the Combat Component handles smoothly recovering it back to zero.
	if (CombatComponent)
	{
		NewRot += CombatComponent->GetVisualRecoil();
		NewLoc += CombatComponent->GetVisualRecoilLocation();
	}

	GetMesh()->SetRelativeRotation(NewRot);
	GetMesh()->SetRelativeLocation(NewLoc);
}

void APlayerCharacter::AddCameraRecoil(float PitchAmount, float YawAmount)
{
	// Stack recoil for full-auto firing
	TargetRecoil += FRotator(PitchAmount, YawAmount, 0.f);
}

void APlayerCharacter::UpdateCameraRecoil(float DeltaTime)
{
	if (!Controller || !CombatComponent) return;

	float KickSpeed = 15.f;
	float RecovSpeed = 10.f;

	if (CombatComponent->GetPrimaryWeapon())
	{
		KickSpeed = CombatComponent->GetPrimaryWeapon()->RecoilKickSpeed;
		RecovSpeed = CombatComponent->GetPrimaryWeapon()->RecoilRecoverySpeed;
	}

	// 1. Calculate how much we need to move the camera this frame
	FRotator DeltaRecoil = FMath::RInterpTo(CurrentRecoil, TargetRecoil, DeltaTime, KickSpeed) - CurrentRecoil;
	
	CurrentRecoil += DeltaRecoil;

	// 2. Inject it into the player's camera
	AddControllerPitchInput(DeltaRecoil.Pitch);
	AddControllerYawInput(DeltaRecoil.Yaw);

	// 3. THE RECOVERY PHASE: Only pull the camera back down when they STOP shooting
	if (!CombatComponent->GetFireButtonPressed())
	{
		// Draining the TargetRecoil forces the DeltaRecoil to become negative,
		// which smoothly pulls the player's crosshair back to its original resting spot.
		TargetRecoil = FMath::RInterpTo(TargetRecoil, FRotator::ZeroRotator, DeltaTime, RecovSpeed);
	}
}

bool APlayerCharacter::CanJumpInternal_Implementation() const
{
	// 1. Must have gameplay enabled and movement must be allowed
	bool bCanJump = !bDisableGameplay && GetCharacterMovement() && GetCharacterMovement()->IsJumpAllowed();

	if (bCanJump)
	{
		// 2. CRITICAL: We intentionally ignore the `!bIsCrouched` check here so you can slide-jump,
		// but we MUST check if you are falling and enforce the MaxJumpCount limit!
		if (!GetCharacterMovement()->IsFalling() || (JumpCurrentCount < JumpMaxCount))
		{
			return true;
		}
	}

	return false;
}

void APlayerCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// Not calling Super on purpose: it raises the mesh by HalfHeightAdjust to keep a third-person body on the
	// floor, but here the mesh (the arms) hangs off the camera, so that lifted the hands above the view.
	// The capsule centre drops by ScaledHalfHeightAdjust, so lift the camera by the same amount and ease it
	// down to the crouched height instead of popping.
	if (FirstPersonCamera)
	{
		FVector Location = FirstPersonCamera->GetRelativeLocation();
		Location.Z += ScaledHalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(Location);
	}
	TargetCameraHeight = CrouchedCameraHeight;
	K2_OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}

void APlayerCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	// Same reasoning in reverse: the capsule centre rises when standing up.
	if (FirstPersonCamera)
	{
		FVector Location = FirstPersonCamera->GetRelativeLocation();
		Location.Z -= ScaledHalfHeightAdjust;
		FirstPersonCamera->SetRelativeLocation(Location);
	}
	TargetCameraHeight = StandingCameraHeight;
	K2_OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
}
#pragma endregion Camera

#pragma region Input
void APlayerCharacter::MoveInput(const FVector2D& Axis)
{
	if (bDisableGameplay || !Controller) return;

	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void APlayerCharacter::LookInput(const FVector2D& Axis)
{
	if (bDisableGameplay || !Controller) return;

	AddControllerYawInput(Axis.X * ActiveMouseSensitivity);
	AddControllerPitchInput(Axis.Y * ActiveMouseSensitivity);
}

void APlayerCharacter::SprintPressed()
{
	if (ShooterMovement) ShooterMovement->SetSprintHeld(true);
}

void APlayerCharacter::SprintReleased()
{
	if (ShooterMovement) ShooterMovement->SetSprintHeld(false);
}

void APlayerCharacter::JumpPressed()
{
	if (bDisableGameplay) return;

	if (ShooterMovement)
	{
		if (ShooterMovement->HandleJumpPressed()) return;

		if (ShooterMovement->IsSliding())
		{
			FVector SlideDir = ShooterMovement->Velocity.GetSafeNormal2D();
			if (SlideDir.IsNearlyZero())
			{
				SlideDir = GetActorForwardVector();
			}
			
			ShooterMovement->Velocity += (SlideDir * ShooterMovement->SlideJumpBoost);

			ShooterMovement->EndSlide();
			UnCrouch();
		}
	}

	Jump();
}

void APlayerCharacter::JumpReleased()
{
	StopJumping();
}



void APlayerCharacter::CrouchPressed()
{
	if (bDisableGameplay) return;

	if (ShooterMovement)
	{
		ShooterMovement->CrouchPressed();
	}
	else
	{
		Crouch();
	}
}

void APlayerCharacter::CrouchReleased()
{
	if (ShooterMovement)
	{
		ShooterMovement->CrouchReleased();
	}
	else
	{
		UnCrouch();
	}
}

void APlayerCharacter::DashPressed()
{
	if (bDisableGameplay || !ShooterMovement) return;

	ShooterMovement->TryDash(ShooterMovement->GetLastInputVector());
}

void APlayerCharacter::HandleDashStarted(FVector Direction)
{
	bInterruptedReload = false;

	// Interrupt a reload so it does not fight the dash animation, and resume it when the dash ends
	if (CombatComponent && (CombatComponent->GetCombatState() == ECombatState::ECS_Reloading || CombatComponent->bLocallyReloading))
	{
		CombatComponent->ResetCombatState();
		bInterruptedReload = true;

		if (GetMesh())
		{
			if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
			{
				if (UAnimMontage* ReloadMontage = GetReloadMontage_Implementation())
				{
					Anim->Montage_Stop(0.1f, ReloadMontage);
				}
			}
		}
	}
}

void APlayerCharacter::HandleDashEnded()
{
	if (bInterruptedReload && CombatComponent)
	{
		CombatComponent->Reload();
	}
	bInterruptedReload = false;
}

void APlayerCharacter::HandleWallRunStarted(bool bRightWall)
{

}
#pragma endregion Input

#pragma region HUD
void APlayerCharacter::UpdateHUDHealth()
{
	if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
	{
		PC->SetHUDHealth(Health, MaxHealth);
	}
}

void APlayerCharacter::UpdateHUDAmmo()
{
	if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
	{
		if (CombatComponent && CombatComponent->GetPrimaryWeapon())
		{
			PC->SetHUDCarriedAmmo(CombatComponent->CarriedAmmo);
			PC->SetHUDWeaponAmmo(CombatComponent->GetPrimaryWeapon()->GetAmmo());
		}
	}
}
#pragma endregion HUD

#pragma region Sword
void APlayerCharacter::PlayMeleeMontage_Implementation()
{
	UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (SwordSlashMontage && Anim)
	{
		Anim->Montage_Play(SwordSlashMontage, SwordSlashPlayRate);
		if (SwordMesh) SwordMesh->SetVisibility(true);
	}
	else
	{
		// No sword montage assigned yet: use the weapon-bash melee
		Super::PlayMeleeMontage_Implementation();
	}
}
#pragma endregion Sword

#pragma region Death
void APlayerCharacter::OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	Super::OnDamageReceived(DamagedActor, Damage, DamageType, InstigatorController, DamageCauser);

	if (bElimmed) return;

	UpdateHUDHealth();

	if (Health <= 0.f)
	{
		if (AMissionGameMode* GM = GetWorld()->GetAuthGameMode<AMissionGameMode>())
		{
			GM->PlayerEliminated(this);
		}
		else
		{
			// No mission game mode (for example a test level): still play the death sequence.
			Elim();
		}
	}
}

void APlayerCharacter::Elim()
{
	if (bElimmed) return;
	bElimmed = true;
	bDisableGameplay = true;

	if (CombatComponent)
	{
		CombatComponent->HandleFire(false);
		CombatComponent->SetAiming(false);
	}

	if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
	{
		PC->SetHUDWeaponAmmo(0);
	}

	// Hide the sniper scope if we died while looking through it
	if (IsLocallyControlled() && CombatComponent && CombatComponent->GetPrimaryWeapon() &&
		CombatComponent->GetPrimaryWeapon()->GetWeaponType() == EWeaponType::EWT_SniperRifle)
	{
		ShowSniperScopeWidget(false);
	}

	ClearFocusedItem();

	if (ShooterMovement)
	{
		ShooterMovement->CancelMovementAbilities();
	}
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (AttachedGrenade)
	{
		AttachedGrenade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (bRagdollOnDeath)
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetCapsuleComponent()->SetEnableGravity(false);
		ApplyDeathPhysics();
	}

	OnPlayerDied();
}

void APlayerCharacter::ResetToAlive()
{
	Super::ResetToAlive();

	bElimmed = false;
	if (ShooterMovement)
	{
		ShooterMovement->CancelMovementAbilities();
	}
	TargetCameraHeight = StandingCameraHeight;
	CurrentCameraRoll = 0.f;
	FirstPersonCamera->SetFieldOfView(DefaultFOV);

	UpdateHUDHealth();
	UpdateHUDAmmo();

	OnPlayerRestored();
}

bool APlayerCharacter::GetIsGameplayDisbled_Implementation()
{
	return bDisableGameplay;
}

bool APlayerCharacter::GetIsWallRunning_Implementation() const
{
	return ShooterMovement && ShooterMovement->IsWallRunning();
}

bool APlayerCharacter::GetIsRightWall_Implementation() const
{
	return ShooterMovement && ShooterMovement->IsRightWall();
}
#pragma endregion Death

#pragma region Interaction
void APlayerCharacter::UpdateInteractionFocus()
{
	if (!IsLocallyControlled() || bElimmed) return;

	AController* PlayerCon = GetController();
	if (!PlayerCon) return;

	FVector Start;
	FRotator Rot;
	PlayerCon->GetPlayerViewPoint(Start, Rot);
	const FVector End = Start + Rot.Vector() * INTERACTION_DISTANCE;

	FHitResult Hit;
	FCollisionQueryParams Params;

	// 1. Ignore the player
	Params.AddIgnoredActor(this);

	// 2. Ignore everything attached to the player (equipped weapons and items)
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;

	if (HitActor && HitActor->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
	{
		// A weapon someone is holding is not interactable
		AWeapon* Weapon = Cast<AWeapon>(HitActor);
		if (Weapon && Weapon->GetWeaponState() != EWeaponState::EWS_Dropped && Weapon->GetWeaponState() != EWeaponState::EWS_Initial)
		{
			ClearFocusedItem();
			return;
		}
		SetFocusedItem(HitActor);

		// Push the prompt to the HUD on the exact frame we focus
		const FText PromptText = IInteractionInterface::Execute_GetInteractionText(HitActor);
		if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
		{
			if (AShooterHUD* HUD = Cast<AShooterHUD>(PC->GetHUD()))
			{
				if (HUD->CharacterOverlay)
				{
					if (!PromptText.IsEmpty())
					{
						HUD->CharacterOverlay->UpdateInteractionPrompt(true, FString::Printf(TEXT("E - %s"), *PromptText.ToString()));
					}
					else
					{
						HUD->CharacterOverlay->UpdateInteractionPrompt(false, TEXT(""));
					}
				}
			}
		}
	}
	else
	{
		ClearFocusedItem();
	}
}

void APlayerCharacter::SetFocusedItem(AActor* NewItem)
{
	if (InteractionComponent->FocusedItem == NewItem) return;

	ClearFocusedItem();
	InteractionComponent->FocusedItem = NewItem;
}

void APlayerCharacter::ClearFocusedItem()
{
	// Hide the HUD prompt whether or not an item pointer is set
	if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
	{
		if (AShooterHUD* HUD = Cast<AShooterHUD>(PC->GetHUD()))
		{
			if (HUD->CharacterOverlay)
			{
				HUD->CharacterOverlay->UpdateInteractionPrompt(false, TEXT(""));
			}
		}
	}

	InteractionComponent->FocusedItem = nullptr;
}
#pragma endregion Interaction

void APlayerCharacter::GroundSmashPressed()
{
	if (bDisableGameplay || !ShooterMovement) return;
	
	// Only record the height if we are actually allowed to smash (falling and not already smashing)
	if (ShooterMovement->IsFalling() && !ShooterMovement->IsGroundSmashing())
	{
		SmashStartZ = GetActorLocation().Z;
		ShooterMovement->TryGroundSmash();
	}
}

void APlayerCharacter::HandleGroundSmashLanded()
{
	// Calculate the exact point on the floor beneath the player
	FVector ImpactLocation = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// --- Calculate the damage based on the distance fallen ---
	float FallDistance = FMath::Max(0.f, SmashStartZ - GetActorLocation().Z);
	
	float CalculatedDamage = FMath::GetMappedRangeValueClamped(
		FVector2D(0.f, MaxDamageFallDistance), 
		FVector2D(MinGroundSmashDamage, MaxGroundSmashDamage), 
		FallDistance
	);

	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this); // Don't damage yourself!

	// Deal the scaled radial damage to enemies
	UGameplayStatics::ApplyRadialDamage(
		GetWorld(),
		CalculatedDamage,
		ImpactLocation,
		GroundSmashRadius,
		UDamageType::StaticClass(),
		IgnoredActors,
		this,
		GetController(),
		true, 
		ECC_Visibility
	);

	// Draw the debug sphere and print the damage dealt
	if (bDrawGroundSmashDebug && GEngine)
	{
		DrawDebugSphere(GetWorld(), ImpactLocation, GroundSmashRadius, 24, FColor::Orange, false, 3.f, 0, 3.f);
		GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor::Cyan, FString::Printf(TEXT("Smash Damage: %f (Fell %f units)"), CalculatedDamage, FallDistance));
	}
}