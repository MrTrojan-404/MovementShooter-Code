// MovementShooter - single-player base character (ACharacter + CharacterMovementComponent).

#include "MovementShooter/Character/BaseCharacter.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/MovementShooter.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/Actor/Item.h"
#include "MovementShooter/Actor/Weapon.h"
#include "MovementShooter/Animation/BaseAnimInstance.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/Component/InteractionComponent.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"
#include "MovementShooter/SpellAbility/SpellComponent.h"

ABaseCharacter::ABaseCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	CharacterMesh = GetMesh();

	CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	SpellComponent = CreateDefaultSubobject<USpellComponent>(TEXT("SpellComponent"));
	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));

	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
}

void ABaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	CharacterMesh = GetMesh();
	DefaultMeshRelativeTransform = CharacterMesh ? CharacterMesh->GetRelativeTransform() : FTransform::Identity;
	DefaultMeshParent = CharacterMesh ? CharacterMesh->GetAttachParent() : nullptr;
	DefaultMeshSocket = CharacterMesh ? CharacterMesh->GetAttachSocketName() : NAME_None;

	if (DissolveMaterialInstance01 && !DynamicDissolveMaterialInstance01)
	{
		DynamicDissolveMaterialInstance01 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance01, this);
		DynamicDissolveMaterialInstance02 = DissolveMaterialInstance02
			? UMaterialInstanceDynamic::Create(DissolveMaterialInstance02, this) : nullptr;

		if (CharacterMesh)
		{
			CharacterMesh->SetMaterial(0, DynamicDissolveMaterialInstance01);
			if (DynamicDissolveMaterialInstance02) CharacterMesh->SetMaterial(1, DynamicDissolveMaterialInstance02);
		}
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Dissolve"), -0.5f);
		if (DynamicDissolveMaterialInstance02)
		{
			DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Dissolve"), -0.5f);
		}
	}

	if (CharacterMesh)
	{
		CachedAnimInstance = Cast<UBaseAnimInstance>(CharacterMesh->GetAnimInstance());
	}

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	if (CharacterMesh)
	{
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
		CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	}

	if (!AttachedGrenade)
	{
		AttachedGrenade = FindComponentByTag<UStaticMeshComponent>(FName("Grenade"));
	}
	if (AttachedGrenade)
	{
		AttachedGrenade->SetVisibility(false);
	}

	// Every character (player, AI, enemies) starts at its Blueprint MaxHealth.
	Health = MaxHealth;
	OnTakeAnyDamage.AddDynamic(this, &ABaseCharacter::ReceiveDamage);

	ActiveMouseSensitivity = BaseMouseSensitivity;
}

void ABaseCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (InteractionComponent)
	{
		UpdateInteractionFocus();
	}

	// --- Aim Offset and Turn-in-Place Math for AI ---
	if (Controller && !IsPlayerControlled() && !bDisableGameplay && Health > 0.f)
	{
		float Speed2D = GetVelocity().Size2D();
		FRotator AimRot = GetBaseAimRotation();
		FRotator ActorRot = GetActorRotation();

		// 1. Calculate Pitch (Map 270-360 range to -90-0 for proper AimOffset blending)
		AO_Pitch = AimRot.Pitch;
		if (AO_Pitch > 90.f && !IsLocallyControlled())
		{
			FVector2D InRange(270.f, 360.f);
			FVector2D OutRange(-90.f, 0.f);
			AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
		}

		// 2. Calculate the Yaw difference between the eyes and the physical chest
		float DeltaYaw = FRotator::NormalizeAxis(AimRot.Yaw - ActorRot.Yaw);

		if (Speed2D > 10.f)
		{
			// Cancel turning if they start walking/running
			AO_Yaw = DeltaYaw;
			bIsTurningInPlace = false; 
		}
		else
		{
			if (bIsTurningInPlace)
			{
				// Smoothly rotate the actor ALL THE WAY to the target
				TurnInPlaceTargetYaw = AimRot.Yaw; // Keep updating in case the player is moving
				FRotator TargetRot = FRotator(0.f, TurnInPlaceTargetYaw, 0.f);
				SetActorRotation(FMath::RInterpTo(ActorRot, TargetRot, DeltaTime, 5.f));

				// Recalculate AO_Yaw for the AnimGraph while turning
				AO_Yaw = FRotator::NormalizeAxis(AimRot.Yaw - GetActorRotation().Yaw);

				// Release the turn lock once they are fully facing the player (within 5 degrees)
				if (FMath::Abs(AO_Yaw) < 5.f)
				{
					bIsTurningInPlace = false;
				}
			}
			else
			{
				AO_Yaw = DeltaYaw;

				// Trigger the Turn-In-Place lock if they look too far
				if (FMath::Abs(AO_Yaw) > 90.f)
				{
					bIsTurningInPlace = true;
					TurnInPlaceTargetYaw = AimRot.Yaw;

					// Play the correct animation montage
					if (UAnimInstance* AnimInst = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr)
					{
						if (AO_Yaw > 0.f && TurnRightMontage)
						{
							AnimInst->Montage_Play(TurnRightMontage);
						}
						else if (AO_Yaw < 0.f && TurnLeftMontage)
						{
							AnimInst->Montage_Play(TurnLeftMontage);
						}
					}
				}
			}
		}
	}
}

#pragma region Health
float ABaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// 1. Point damage (bullets, snipers, shotguns)
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		LastHitBone = PointDamageEvent->HitInfo.BoneName;
		LastHitDirection = PointDamageEvent->ShotDirection;
	}
	// 2. Radial damage (grenades, rockets)
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent* RadialDamageEvent = static_cast<const FRadialDamageEvent*>(&DamageEvent);
		LastHitBone = NAME_None;
		LastHitDirection = (GetActorLocation() - RadialDamageEvent->Origin).GetSafeNormal();
	}
	// 3. Generic damage (falling, zones, melee)
	else
	{
		LastHitBone = NAME_None;
		if (DamageCauser)
		{
			LastHitDirection = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
		}
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABaseCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	// The old game mode's CalculateDamage() just returned the base damage, so damage applies directly.
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);

	OnDamageReceived(DamagedActor, Damage, DamageType, InstigatorController, DamageCauser);

	// Only flinch while still alive
	if (Health > 0.f)
	{
		PlayHitReact(LastHitDirection, LastHitBone);
	}

	OnBaseHealthChanged.Broadcast();
}

void ABaseCharacter::PlayHitReact(const FVector& HitDirection, FName HitBone)
{
	if (CharacterMesh && CharacterMesh->GetAnimInstance())
	{
		if (UBaseAnimInstance* BaseAnim = Cast<UBaseAnimInstance>(CharacterMesh->GetAnimInstance()))
		{
			BaseAnim->TriggerHitReact(HitDirection, HitBone);
		}
	}
}

void ABaseCharacter::AddMedkit(int32 Amount)
{
	MedkitCount = FMath::Clamp(MedkitCount + Amount, 0, MaxMedkitCount);
	UpdateMedkitHUD();
}

void ABaseCharacter::ConsumeMedkit()
{
	if (MedkitCount <= 0) return;
	if (Health >= MaxHealth) return; // don't waste one at full HP

	MedkitCount--;
	SetHealth(FMath::Clamp(GetHealth() + MedkitHealAmount, 0.f, GetMaxHealth()));
	UpdateMedkitHUD();

	if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
	{
		PC->SetHUDHealth(GetHealth(), GetMaxHealth());
	}
}

void ABaseCharacter::UpdateMedkitHUD()
{
	if (ACorePlayerController* PC = Cast<ACorePlayerController>(GetController()))
	{
		PC->SetHUDMedkits(MedkitCount);
	}
}
#pragma endregion Health

#pragma region Interaction
void ABaseCharacter::UpdateInteractionFocus()
{
	// Intentionally empty. The player overrides this to trace from the camera/crosshair.
}

void ABaseCharacter::SetFocusedItem(AActor* NewItem)
{
	if (InteractionComponent->FocusedItem == NewItem) return;

	ClearFocusedItem();
	InteractionComponent->FocusedItem = NewItem;

	if (InteractionComponent->FocusedItem)
	{
		IInteractionInterface::Execute_ShowInteractionPrompt(InteractionComponent->FocusedItem, true);
	}
}

void ABaseCharacter::ClearFocusedItem()
{
	if (InteractionComponent->FocusedItem)
	{
		IInteractionInterface::Execute_ShowInteractionPrompt(InteractionComponent->FocusedItem, false);
	}
	InteractionComponent->FocusedItem = nullptr;
}

void ABaseCharacter::Interact_Implementation(AActor* Interactor)
{
	// Nothing to interact with on a character now that revive is gone.
}

FText ABaseCharacter::GetInteractionText_Implementation() const
{
	return FText::GetEmpty();
}
#pragma endregion Interaction

#pragma region PickupInterface
void ABaseCharacter::OnPickupBegin_Implementation(AActor* PickupActor)
{
	if (!PickupActor || !CombatComponent) return;
	if (PickupActor->GetClass()->ImplementsInterface(UInteractionInterface::StaticClass()))
	{
		if (InteractionComponent) InteractionComponent->NearbyItem.AddUnique(PickupActor);
	}
}

void ABaseCharacter::OnPickupEnd_Implementation(AActor* PickupActor)
{
	if (!PickupActor || !InteractionComponent) return;
	InteractionComponent->NearbyItem.Remove(PickupActor);
	if (InteractionComponent->FocusedItem == PickupActor) ClearFocusedItem();
}

void ABaseCharacter::OnEquipItem_Implementation(AActor* Item)
{
	if (!CombatComponent || !Item) return;
	AItem* WeaponItem = Cast<AItem>(Item);
	if (WeaponItem) CombatComponent->ServerEquip(WeaponItem);
}
#pragma endregion PickupInterface

void ABaseCharacter::EnableTargetingOutline(bool bEnable, int32 StencilValue)
{
	if (CharacterMesh)
	{
		CharacterMesh->SetRenderCustomDepth(bEnable);
		CharacterMesh->SetCustomDepthStencilValue(StencilValue);
		CharacterMesh->MarkRenderStateDirty();
	}
}

#pragma region Death and Ragdoll
void ABaseCharacter::ApplyDeathPhysics()
{
	if (CharacterMesh == nullptr) return;

	CharacterMesh->bPauseAnims = true;

	// The profile wipes the collision slate, so the custom blocks go on AFTER it.
	CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); // ragdolls block projectiles
	CharacterMesh->SetAllBodiesSimulatePhysics(true);
	CharacterMesh->SetSimulatePhysics(true);
	CharacterMesh->SetEnableGravity(true);
	CharacterMesh->WakeAllRigidBodies();

	FName BoneToBlast = LastHitBone;
	if (BoneToBlast == NAME_None || CharacterMesh->GetBoneIndex(BoneToBlast) == INDEX_NONE)
	{
		BoneToBlast = FName("pelvis");
	}

	CharacterMesh->AddImpulse(LastHitDirection * RagdollDeathImpulse, BoneToBlast, true);
}

void ABaseCharacter::BeginRagdoll(FVector ImpulseDirection, float ImpulseStrength)
{
	if (!CharacterMesh) return;

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetEnableGravity(false);

	CharacterMesh->bPauseAnims = true;
	CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); // floating ragdolls block projectiles too
	CharacterMesh->SetAllBodiesSimulatePhysics(true);
	CharacterMesh->SetSimulatePhysics(true);
	CharacterMesh->WakeAllRigidBodies();
	CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	FName ImpulseBone = FName("pelvis");
	if (CharacterMesh->GetBoneIndex(ImpulseBone) == INDEX_NONE)
	{
		ImpulseBone = NAME_None;
	}

	// Impulse on the next tick so the freshly enabled physics bodies are awake.
	const FVector FinalImpulse = ImpulseDirection.GetSafeNormal() * ImpulseStrength;
	TWeakObjectPtr<ABaseCharacter> WeakSelf(this);
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakSelf, FinalImpulse, ImpulseBone]()
	{
		if (ABaseCharacter* Self = WeakSelf.Get())
		{
			if (Self->CharacterMesh)
			{
				Self->CharacterMesh->AddImpulse(FinalImpulse, ImpulseBone, true);
			}
		}
	}));

	if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(AEnemyAIController::BB_bIsInRagdoll, true);
		}
	}
}

void ABaseCharacter::RecoverFromRagdoll()
{
	// Never stand up a dead body.
	if (Health <= 0.f || bDisableGameplay) return;
	if (!CharacterMesh) return;

	FVector RestingLocation = CharacterMesh->GetSocketLocation(FName("pelvis"));
	if (RestingLocation.IsZero())
	{
		RestingLocation = CharacterMesh->GetComponentLocation();
	}

	CharacterMesh->SetAllBodiesSimulatePhysics(false);
	CharacterMesh->SetSimulatePhysics(false);
	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CharacterMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
	CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
	CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CharacterMesh->bPauseAnims = false;

	// Capsule half-height replaces the old hardcoded 90 so this works for any capsule size.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	GetCapsuleComponent()->SetWorldLocation(RestingLocation + FVector(0.f, 0.f, HalfHeight), false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetEnableGravity(true);

	// Put the mesh back exactly where the Blueprint had it.
	ReattachMeshToDefaultParent();

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(AEnemyAIController::BB_bIsInRagdoll, false);
		}
	}
}

void ABaseCharacter::ReattachMeshToDefaultParent()
{
	if (!CharacterMesh) return;

	USceneComponent* Parent = DefaultMeshParent ? DefaultMeshParent : static_cast<USceneComponent*>(GetCapsuleComponent());
	CharacterMesh->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, DefaultMeshSocket);
	CharacterMesh->SetRelativeTransform(DefaultMeshRelativeTransform);
}

void ABaseCharacter::ResetToAlive()
{
	bDisableGameplay = false;

	// Kill any pending death dissolve
	GetWorldTimerManager().ClearTimer(DissolveTImer);
	if (DissolveTimeline) DissolveTimeline->Stop();
	UpdateDissolveMaterial(-0.5f); // fully visible again

	// Wake the capsule
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetEnableGravity(true);

	// Stop the ragdoll
	if (CharacterMesh)
	{
		CharacterMesh->SetAllBodiesSimulatePhysics(false);
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CharacterMesh->SetCollisionProfileName(TEXT("CharacterMesh"));
		CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
		CharacterMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		CharacterMesh->SetCollisionObjectType(ECC_SkeletalMesh);
		CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		CharacterMesh->bPauseAnims = false;

		// Snap the mesh back exactly where the Blueprint had it
		ReattachMeshToDefaultParent();
	}

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ABaseCharacter::DissolveTimerFinished()
{
	if (DissolveMaterialInstance01)
	{
		DynamicDissolveMaterialInstance01 = UMaterialInstanceDynamic::Create(DissolveMaterialInstance01, this);
		DynamicDissolveMaterialInstance02 = DissolveMaterialInstance02
			? UMaterialInstanceDynamic::Create(DissolveMaterialInstance02, this) : nullptr;

		CharacterMesh->SetMaterial(0, DynamicDissolveMaterialInstance01);
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Glow"), 200.f);

		if (DynamicDissolveMaterialInstance02)
		{
			CharacterMesh->SetMaterial(1, DynamicDissolveMaterialInstance02);
			DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
			DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Glow"), 200.f);
		}
	}
	StartDissolve();
}

void ABaseCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	if (DynamicDissolveMaterialInstance01)
	{
		DynamicDissolveMaterialInstance01->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
	if (DynamicDissolveMaterialInstance02)
	{
		DynamicDissolveMaterialInstance02->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABaseCharacter::StartDissolve()
{
	DissolveTrack.BindDynamic(this, &ABaseCharacter::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
	}
}
#pragma endregion Death and Ragdoll

void ABaseCharacter::SpawnDefaultWeapon(bool bSetToDestroyWeapon, bool bElimmedCheck)
{
	UWorld* World = GetWorld();
	if (!World || bElimmedCheck) return;

	if (DefaultWeaponClass)
	{
		AWeapon* StartingWeapon = World->SpawnActor<AWeapon>(DefaultWeaponClass);
		StartingWeapon->bDestroyWeapon = bSetToDestroyWeapon;
		if (CombatComponent)
		{
			CombatComponent->EquipWeapon(StartingWeapon);
		}
	}

	if (DefaultSecondaryWeaponClass)
	{
		AWeapon* StartingSecondaryWeapon = World->SpawnActor<AWeapon>(DefaultSecondaryWeaponClass);
		StartingSecondaryWeapon->bDestroyWeapon = bSetToDestroyWeapon;
		if (CombatComponent)
		{
			// EquipWeapon slots it as Secondary because Primary is already full.
			CombatComponent->EquipWeapon(StartingSecondaryWeapon);
		}
	}
}

UAnimMontage* ABaseCharacter::ResolveReloadMontage() const
{
	if (!CombatComponent || !CombatComponent->GetPrimaryWeapon()) return nullptr;

	// Walk up the weapon's class chain so a child Blueprint inherits its parent's entry
	for (UClass* Class = CombatComponent->GetPrimaryWeapon()->GetClass();
		 Class && Class->IsChildOf(AWeapon::StaticClass());
		 Class = Class->GetSuperClass())
	{
		if (UAnimMontage* const* Montage = ReloadMontages.Find(TSubclassOf<AWeapon>(Class)))
		{
			if (*Montage) return *Montage;
		}
	}
	return nullptr;
}

UAnimMontage* ABaseCharacter::GetReloadMontage_Implementation()
{
	return ResolveReloadMontage();
}

void ABaseCharacter::PlayReloadMontage_Implementation()
{
	UAnimInstance* AnimInstance = CharacterMesh ? CharacterMesh->GetAnimInstance() : nullptr;
	UAnimMontage* ReloadToPlay = ResolveReloadMontage();
	if (AnimInstance && ReloadToPlay)
	{
		AnimInstance->Montage_Play(ReloadToPlay);
	}
}

#pragma region CombatInterface
USkeletalMeshComponent* ABaseCharacter::GetCombatMesh_Implementation() const
{
	return CharacterMesh ? CharacterMesh : GetMesh();
}

AActor* ABaseCharacter::GetCombatOwner_Implementation() const
{
	return const_cast<ABaseCharacter*>(this);
}

APawn* ABaseCharacter::GetCombatPawn_Implementation() const
{
	return const_cast<ABaseCharacter*>(this);
}

void ABaseCharacter::PlayFireMontage_Implementation(bool bIsAiming)
{
	if (CombatComponent == nullptr || CombatComponent->GetPrimaryWeapon() == nullptr) return;

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		// Force rewind for rapid fire
		AnimInstance->Montage_Stop(0.0f, FireWeaponMontage);
		AnimInstance->Montage_Play(FireWeaponMontage);

		FName SectionName;

		// Pick the section based entirely on the weapon type, ignoring aim state
		switch (CombatComponent->GetPrimaryWeapon()->GetWeaponType())
		{
		case EWeaponType::EWT_Pistol:
		case EWeaponType::EWT_SubmachineGun:
			SectionName = FName("Pistol");
			break;
		case EWeaponType::EWT_Shotgun:
			SectionName = FName("Shotgun");
			break;
		default:
			// Assault rifles, snipers, and launchers default to the Rifle animation
			SectionName = FName("Rifle");
			break;
		}

		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABaseCharacter::PlayThrowGrenadeMontage_Implementation()
{
	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && ThrowGrenadeMontage)
	{
		AnimInstance->Montage_Play(ThrowGrenadeMontage);
	}
}

void ABaseCharacter::PlayMeleeMontage_Implementation()
{
	if (!MeleeMontage || !CharacterMesh) return;

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->Montage_Play(MeleeMontage);

		// Fallback section for unarmed characters and basic AI
		FName SectionName = FName("Default");

		// With a weapon equipped, pick the matching bash animation
		if (CombatComponent && CombatComponent->GetPrimaryWeapon())
		{
			switch (CombatComponent->GetPrimaryWeapon()->GetWeaponType())
			{
			case EWeaponType::EWT_AssaultRifle:
			case EWeaponType::EWT_SniperRifle:
			case EWeaponType::EWT_RocketLauncher:
			case EWeaponType::EWT_GrenadeLauncher:
				SectionName = FName("Rifle");
				break;
			case EWeaponType::EWT_Pistol:
			case EWeaponType::EWT_SubmachineGun:
				SectionName = FName("Pistol");
				break;
			case EWeaponType::EWT_Shotgun:
				SectionName = FName("Shotgun");
				break;
			default:
				break;
			}
		}
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABaseCharacter::OnMeleeHit_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->MeleeHit();
	}
}

void ABaseCharacter::OnMeleeFinished_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->SetCombatState(ECombatState::ECS_Unoccupied);
	}
}

void ABaseCharacter::PlayEquippedSwapMontage_Implementation()
{
	if (CombatComponent == nullptr || CombatComponent->GetPrimaryWeapon() == nullptr) return;

	UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
	if (AnimInstance && SwapMontage)
	{
		AnimInstance->Montage_Play(SwapMontage);
	}
}

void ABaseCharacter::OnSwapEquippedFinished_Implementation()
{
	if (CombatComponent)
	{
		CombatComponent->OnEquippedSwapFinished();
	}
}

void ABaseCharacter::ShowSniperScopeCall_Implementation(bool bIsAiming)
{
	// Only the human player draws the scope; AI must not spawn UI widgets.
	if (IsLocallyControlled())
	{
		ShowSniperScopeWidget(bIsAiming);
	}
}

UStaticMeshComponent* ABaseCharacter::GetAttachedGrenade_Implementation()
{
	return AttachedGrenade;
}

void ABaseCharacter::OnThrowGrenadeFinished_Implementation()
{
	GetCombatComponent()->OnThrowGrenadeFinished();
}

void ABaseCharacter::OnLaunchGrenade_Implementation()
{
	GetCombatComponent()->OnLaunchGrenade();
}

void ABaseCharacter::OnFinishReload_Implementation()
{
	GetCombatComponent()->OnFinishReload();
}

void ABaseCharacter::OnShotgunShellReload_Implementation()
{
	GetCombatComponent()->OnShotgunShellReload();
}
#pragma endregion CombatInterface

#pragma region AnimInterface
bool ABaseCharacter::GetAiming_Implementation() const
{
	return CombatComponent ? CombatComponent->GetIsAiming() : false;
}

bool ABaseCharacter::GetIsFiring_Implementation() const
{
	return CombatComponent ? CombatComponent->GetFireButtonPressed() : false;
}

UCombatComponent* ABaseCharacter::GetCombatComponent_Implementation()
{
	return CombatComponent;
}

AWeapon* ABaseCharacter::GetPrimaryWeapon_Implementation()
{
	return GetCombatComponent()->GetPrimaryWeapon();
}

float ABaseCharacter::GetAOPitch_Implementation()
{
	return AO_Pitch;
}

float ABaseCharacter::GetAOYaw_Implementation()
{
	return AO_Yaw;
}

bool ABaseCharacter::GetIsGameplayDisbled_Implementation()
{
	return bDisableGameplay;
}

EWeaponType ABaseCharacter::GetEquippedWeaponType_Implementation() const
{
	return CombatComponent->GetEquippedWeaponType();
}

bool ABaseCharacter::GetIsWallRunning_Implementation() const
{
	// Wall run lives in the movement component (next step); the player overrides these then.
	return false;
}

bool ABaseCharacter::GetIsRightWall_Implementation() const
{
	return false;
}
#pragma endregion AnimInterface
