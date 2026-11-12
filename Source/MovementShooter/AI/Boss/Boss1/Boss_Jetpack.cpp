#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MovementShooter/MovementShooter.h"
#include "MovementShooter/AI/Boss/ArenaManager.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"

ABoss_Jetpack::ABoss_Jetpack()
{
	bIsHeavyEnemy = true;
	bIsFinalPhase = true;
	MaxHealth = 4000.f;
	Health = 4000.f;
	CombatStyle = 4;

	JetpackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("JetpackMesh"));
	JetpackMesh->SetupAttachment(GetMesh(), JetpackAttachSocket);
	JetpackMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	JetpackMesh->SetCollisionObjectType(ECC_SkeletalMesh);
	JetpackMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	JetpackMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	JetpackMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
}

void ABoss_Jetpack::BeginPlay()
{
	// Spawns and equips DefaultWeaponClass via ABossBase
	Super::BeginPlay();
}

void ABoss_Jetpack::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Smoothly rise to and maintain hover altitude in Phase 2
	if (bIsHovering && !bIsDead)
	{
		FVector Loc = GetActorLocation();
		if (Loc.Z < TargetHoverZ)
		{
			Loc.Z = FMath::FInterpTo(Loc.Z, TargetHoverZ, DeltaTime, 4.f);
			SetActorLocation(Loc, true);
		}
	}
}

float ABoss_Jetpack::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	bLastHitWasJetpack = false;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamageEvent = static_cast<const FPointDamageEvent*>(&DamageEvent);
		// True if the bullet directly hit the separate Jetpack Static Mesh!
		if (PointDamageEvent->HitInfo.Component.Get() == JetpackMesh)
		{
			bLastHitWasJetpack = true;
		}
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABoss_Jetpack::OnDamageReceived(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	if (bIsDead) return;

	// 1. Check Weak Point using LastHitBone and LastHitDirection populated by ABaseCharacter::TakeDamage
	const float ForwardDot = FVector::DotProduct(GetActorForwardVector(), LastHitDirection.GetSafeNormal2D());
	const bool bHitFromBehind = ForwardDot > 0.35f;
	const bool bHitWeakPoint = bLastHitWasJetpack || bHitFromBehind;

	float TotalDamageDealt = Damage;

	// 2. Apply Bonus Weak Point Damage
	if (bHitWeakPoint && Health > 0.f)
	{
		const float BonusDamage = Damage * (WeakPointMultiplier - 1.f);
		Health = FMath::Clamp(Health - BonusDamage, 0.f, MaxHealth);
		TotalDamageDealt += BonusDamage;
	}

	// 3. Call Super AFTER applying weak-point bonus damage so fatal hits trigger HandleDeath immediately
	Super::OnDamageReceived(DamagedActor, TotalDamageDealt, DamageType, InstigatorController, DamageCauser);
	if (bIsDead) return;

	// 4. Trigger Phase 2 at 50% Health
	if (!bIsPhase2 && Health <= (MaxHealth * 0.5f))
	{
		bIsPhase2 = true;

		if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
		{
			if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
			{
				BB->SetValueAsBool(FName("bIsPhase2"), true);
			}
		}

		if (ArenaManager)
		{
			ArenaManager->AdvancePhase();
		}

		StartHover();
	}

	// 5. Force the boss out of the sky if the player damages the jetpack enough mid-air
	if (bIsHovering && bHitWeakPoint)
	{
		AccumulatedAirJetpackDamage += TotalDamageDealt;
		if (AccumulatedAirJetpackDamage >= MidAirKnockdownThreshold)
		{
			StopHover(true);
		}
	}
}

void ABoss_Jetpack::StartHover()
{
	if (bIsDead || bIsHovering) return;

	bIsHovering = true;
	AccumulatedAirJetpackDamage = 0.f;
	TargetHoverZ = GetActorLocation().Z + HoverAltitudeOffset;

	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->MaxFlySpeed = JogSpeed;

	if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(FName("bIsHovering"), true);
		}
	}

	OnHoverStateChanged(true, false);

	// Fuel limit forces him to land periodically for recovery windows
	GetWorldTimerManager().SetTimer(HoverFuelTimer, this, &ABoss_Jetpack::OnHoverFuelExpired, MaxHoverDuration, false);
}

void ABoss_Jetpack::OnHoverFuelExpired()
{
	StopHover(false);
}

void ABoss_Jetpack::StopHover(bool bForcedDownByPlayer)
{
	if (!bIsHovering) return;

	bIsHovering = false;
	GetWorldTimerManager().ClearTimer(HoverFuelTimer);

	GetCharacterMovement()->SetMovementMode(MOVE_Falling);

	if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
		{
			BB->SetValueAsBool(FName("bIsHovering"), false);
		}
	}

	OnHoverStateChanged(false, bForcedDownByPlayer);

	// Schedule next lift-off after the grounded recovery window (1.5x longer if player shot the jetpack down)
	if (bIsPhase2 && !bIsDead)
	{
		const float RecoveryWindow = bForcedDownByPlayer ? (GroundedRecoveryDuration * 1.5f) : GroundedRecoveryDuration;
		GetWorldTimerManager().SetTimer(GroundCooldownTimer, this, &ABoss_Jetpack::StartNextHoverCycle, RecoveryWindow, false);
	}
}

void ABoss_Jetpack::StartNextHoverCycle()
{
	if (bIsDead) return;
	
	StartHover();
}