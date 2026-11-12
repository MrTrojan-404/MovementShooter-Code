#include "MovementShooter/AI/Character/AICharacterBase.h"

#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/WidgetComponent.h"
#include "Perception/AISense_Damage.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"
#include "MovementShooter/SpellAbility/SpellComponent.h"
#include "MovementShooter/UI/HUD/ShooterHUD.h"
#include "MovementShooter/UI/Widgets/CharacterOverlay.h"

AAICharacterBase::AAICharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- Decouple the body from the AI Controller's Aim Rotation ---
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = false;
	GetCharacterMovement()->bOrientRotationToMovement = true; // Body faces the direction of travel

	StealthIndicatorWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StealthIndicatorWidget"));
	StealthIndicatorWidget->SetupAttachment(RootComponent);
	StealthIndicatorWidget->SetRelativeLocation(FVector(0.f, 0.f, 110.f)); 
	StealthIndicatorWidget->SetWidgetSpace(EWidgetSpace::Screen);
	StealthIndicatorWidget->SetDrawAtDesiredSize(true);

	// Hardcode the path to the Blueprint Widget (Right-click widget in Editor -> Copy Reference)
	// MAKE SURE it ends with _C
	static ConstructorHelpers::FClassFinder<UUserWidget> IndicatorClass(TEXT("/Game/Blueprints/UI/WBP_DirectionalIndicator.WBP_DirectionalIndicator_C"));
	if (IndicatorClass.Succeeded())
	{
		StealthIndicatorWidget->SetWidgetClass(IndicatorClass.Class);
	}
	
}
void AAICharacterBase::BeginPlay()
{
	Super::BeginPlay();

	// Start at the patrol speed (the controller keeps it up to date afterwards)
	ApplyMovementSpeedForState(EAIState::Patrol);
}

void AAICharacterBase::SetAIMovementSpeed(EAIMovementSpeed SpeedState)
{
	CurrentMovementSpeed = SpeedState;

	float NewSpeed = WalkSpeed;
	if (SpeedState == EAIMovementSpeed::Jog) NewSpeed = JogSpeed;
	else if (SpeedState == EAIMovementSpeed::Sprint) NewSpeed = SprintSpeed;

	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
}

void AAICharacterBase::ApplyMovementSpeedForState(EAIState NewState)
{
	if (!bAutoMovementSpeed || bIsDead) return;

	switch (NewState)
	{
	case EAIState::Patrol: SetAIMovementSpeed(PatrolSpeed); break;
	case EAIState::Alert:  SetAIMovementSpeed(AlertSpeed);  break;
	case EAIState::Engage: SetAIMovementSpeed(EngageSpeed); break;
	default: break;
	}
}

void AAICharacterBase::OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	if (InstigatorController && InstigatorController->GetPawn())
	{
		UAISense_Damage::ReportDamageEvent(
			GetWorld(),
			this,                              // who got hurt (the AI)
			InstigatorController->GetPawn(),   // who shot us (the player)
			Damage,
			GetActorLocation(),
			DamageCauser->GetActorLocation()   // where the bullet came from
		);
	}

	if (bIsDead) return;

	if (Health <= 0.f)
	{
		HandleDeath(InstigatorController, DamageCauser);
	}
}

void AAICharacterBase::HandleDeath(AController* InstigatorController, AActor* DamageCauser)
{
	if (bIsDead) return;

	ABaseCharacter* Killer = nullptr;
	bool bWasStealthKill = false;

	if (KillCreditInstigator.IsValid())
	{
		Killer = KillCreditInstigator.Get();
		bWasStealthKill = true;
	}
	else if (InstigatorController)
	{
		Killer = Cast<ABaseCharacter>(InstigatorController->GetPawn());
	}

	if (Killer)
	{
		if (USpellComponent* SpellComp = Killer->FindComponentByClass<USpellComponent>())
		{
			SpellComp->OnKillScored(bWasStealthKill);
		}
	}

	NotifyControllerOfDeath();

	bIsDead = true;

	if (CombatComponent)
	{
		CombatComponent->DropEquippedWeapon();
	}

	// Call ApplyDeathVisuals directly (no duplicate montage or delay timer here!)
	ApplyDeathVisuals(LastHitDirection, LastHitBone);

	SetLifeSpan(DissolveDelay + 5.f);
}

void AAICharacterBase::NotifyControllerOfDeath()
{
	// Tell the brain to stop processing stealth math
	if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
	{
		AICon->OnEnemyDied();
	}
}

void AAICharacterBase::ApplyDeathVisuals(const FVector& HitDirection, FName HitBone)
{
	bIsDead = true;
	bDisableGameplay = true;
	LastHitDirection = HitDirection;
	LastHitBone = HitBone;

	if (CombatComponent) CombatComponent->HandleFire(false);

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetEnableGravity(false);
		Capsule->ComponentVelocity = FVector::ZeroVector;
	}

	if (bRagdollOnDeath)
	{
		if (CharacterMesh)
		{
			CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			CharacterMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
			CharacterMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		}
		ApplyDeathPhysics();
	}
	else if (CharacterMesh)
	{
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (UAnimInstance* AnimInst = CharacterMesh->GetAnimInstance())
		{
			AnimInst->StopAllMontages(0.f);

			if (DeathMontage)
			{
				DeathMontage->bEnableAutoBlendOut = false;
				AnimInst->Montage_Play(DeathMontage);

				int32 ChosenSectionIndex = 0;

				// --- DIRECTIONAL DEATH SECTION ROUTING ---
				if (!HitDirection.IsNearlyZero())
				{
					const FVector LocalHitDir = GetActorTransform().InverseTransformVectorNoScale(HitDirection.GetSafeNormal());
					FName DeathSection = NAME_None;

					if (FMath::Abs(LocalHitDir.X) >= FMath::Abs(LocalHitDir.Y))
					{
						DeathSection = (LocalHitDir.X > 0.f) ? FName("Back") : FName("Front");
					}
					else
					{
						DeathSection = (LocalHitDir.Y > 0.f) ? FName("Left") : FName("Right");
					}

					if (DeathMontage->IsValidSectionName(DeathSection))
					{
						AnimInst->Montage_JumpToSection(DeathSection, DeathMontage);
						ChosenSectionIndex = DeathMontage->GetSectionIndex(DeathSection);
					}
				}

				// Lock the skeleton pose right before the section finishes so it can never blend back to Idle!
				const float SectionLength = DeathMontage->GetSectionLength(FMath::Max(0, ChosenSectionIndex));
				const float FreezeDelay = FMath::Max(0.05f, SectionLength - 0.08f);
				GetWorldTimerManager().SetTimer(DeathPoseFreezeTimer, this, &AAICharacterBase::FreezeDeathPose, FreezeDelay, false);
			}
		}
	}

	if (IsPlayerAlly())
	{
		SetLifeSpan(0.f);
	}
	else
	{
		GetWorldTimerManager().SetTimer(DissolveTImer, this, &AAICharacterBase::DissolveTimerFinished, DissolveDelay);
	}

	if (APlayerController* LocalPC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
	{
		if (ACorePlayerController* CorePC = Cast<ACorePlayerController>(LocalPC))
		{
			if (AShooterHUD* HUD = Cast<AShooterHUD>(CorePC->GetHUD()))
			{
				if (HUD->CharacterOverlay)
				{
					HUD->CharacterOverlay->UpdateStealthHUD(this, 0.f, 0);
				}
			}
		}
	}
}

void AAICharacterBase::FreezeDeathPose()
{
	if (CharacterMesh)
	{
		CharacterMesh->bPauseAnims = true;
	}
}

void AAICharacterBase::TriggerElectrocutionDeath(AActor* InstigatorActor, UNiagaraSystem* ZapVFX, USoundBase* ZapSound)
{
	if (bIsDead) return;

	bDisableGameplay = true;

	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		if (AIC->GetBrainComponent()) AIC->GetBrainComponent()->StopLogic("Electrocuted");
	}

	if (CombatComponent) CombatComponent->HandleFire(false);

	KillCreditInstigator = Cast<ABaseCharacter>(InstigatorActor);

	// Work out how long the animation takes
	float MontageLength = 2.0f;
	if (ElectrocuteDeathMontage) MontageLength = ElectrocuteDeathMontage->GetPlayLength();

	// Cut the FX slightly before the animation ends so it syncs with the ragdoll drop
	const float FXDuration = FMath::Max(0.1f, MontageLength - 0.25f);

	PlayElectrocuteFX(ZapVFX, ZapSound, FXDuration, true);
}

void AAICharacterBase::PlayElectrocuteFX(UNiagaraSystem* ZapVFX, USoundBase* ZapSound, float Duration, bool bPlayDeathAnimation)
{
	if (bIsDead) return;

	// Only freeze movement and play the death fall if they are actually dying
	if (bPlayDeathAnimation)
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();

		if (ElectrocuteDeathMontage && CharacterMesh && CharacterMesh->GetAnimInstance())
		{
			CharacterMesh->GetAnimInstance()->Montage_Play(ElectrocuteDeathMontage);
		}
	}

	// 1. Spawn and keep a reference to the VFX
	if (ZapVFX && CharacterMesh)
	{
		ActiveElectrocuteVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(ZapVFX, CharacterMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}

	// 2. Spawn and keep a reference to the audio
	if (ZapSound && CharacterMesh)
	{
		ActiveElectrocuteSound = UGameplayStatics::SpawnSoundAttached(ZapSound, CharacterMesh);
	}

	// 3. A strict timer shuts both down early
	GetWorldTimerManager().SetTimer(ElectrocuteFXTimer, this, &AAICharacterBase::StopElectrocuteFX, Duration, false);
}

void AAICharacterBase::StopElectrocuteFX()
{
	if (ActiveElectrocuteVFX)
	{
		ActiveElectrocuteVFX->Deactivate();
	}
	if (ActiveElectrocuteSound)
	{
		ActiveElectrocuteSound->Stop();
	}

	// Wake the Heavy back up when the sparks stop
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AIC->GetBlackboardComponent())
		{
			BB->SetValueAsBool(AEnemyAIController::BB_bIsElectrocuted, false);
		}
	}
}
