#include "MovementShooter/AI/Character/MeleeEnemy.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"

AMeleeEnemy::AMeleeEnemy()
{
	SightRadius = 3500.f;
	HearingRange = 5000.f;
	MeleeRange = 150.f;
	MaxHealth = 150.f;
	Health = 150.f;
	CombatStyle = 0;
	bRagdollOnDeath = false; // Use DeathMontage instead of ragdoll
}

void AMeleeEnemy::BeginPlay()
{
	Super::BeginPlay();

	AEnemyAIController* AIController = Cast<AEnemyAIController>(GetController());
	if (AIController && AIController->GetBlackboardComponent())
	{
		AIController->GetBlackboardComponent()->SetValueAsInt(FName("EnemyType"), 0);
		AIController->GetBlackboardComponent()->SetValueAsFloat(FName("PreferredRange"), MeleeRange);
	}

	if (bUseRestState && RestMontage)
	{
		EnterRest();
	}
}

void AMeleeEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead || !bUseRestState || !RestMontage) return;

	// If we are walking back to PatrolOrigin in Patrol state, check if we have arrived
	if (!bIsResting && !bIsWakingUp)
	{
		if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
		{
			if (AICon->GetAIState() == EAIState::Patrol && IsAtRestLocation() && GetVelocity().Size2D() < 10.f)
			{
				EnterRest();
			}
		}
	}
}

bool AMeleeEnemy::IsAtRestLocation() const
{
	// Use 2D distance because PatrolOrigin.Z is at the bottom of the capsule
	return FVector::Dist2D(GetActorLocation(), PatrolOrigin) <= RestArrivalRadius;
}

void AMeleeEnemy::SetAIMovementSpeed(EAIMovementSpeed SpeedState)
{
	// Remember the requested speed, but keep MaxWalkSpeed pinned to 0 while resting or standing up!
	if (bIsResting || bIsWakingUp)
	{
		CurrentMovementSpeed = SpeedState;
		GetCharacterMovement()->MaxWalkSpeed = 0.f;
		return;
	}

	Super::SetAIMovementSpeed(SpeedState);
}

void AMeleeEnemy::ApplyMovementSpeedForState(EAIState NewState)
{
	if (!bUseRestState || !RestMontage)
	{
		Super::ApplyMovementSpeedForState(NewState);
		return;
	}

	if (NewState == EAIState::Patrol)
	{
		// Only rest immediately if we are ALREADY at the start point; otherwise walk back first!
		if (IsAtRestLocation())
		{
			EnterRest();
		}
		else
		{
			Super::ApplyMovementSpeedForState(EAIState::Patrol);
		}
		return;
	}

	// Transitioning to Alert or Engage while resting -> stand up first
	if (bIsResting)
	{
		TriggerUnrest(NewState);
		return;
	}

	// If already standing up, update CurrentMovementSpeed so OnUnrestFinished knows how fast to run
	if (bIsWakingUp)
	{
		switch (NewState)
		{
		case EAIState::Alert:  CurrentMovementSpeed = AlertSpeed;  break;
		case EAIState::Engage: CurrentMovementSpeed = EngageSpeed; break;
		default: break;
		}
		return;
	}

	Super::ApplyMovementSpeedForState(NewState);
}

void AMeleeEnemy::EnterRest()
{
	if (bIsDead) return;
	
	GetWorldTimerManager().ClearTimer(UnrestTimerHandle);
	bIsResting = true;
	bIsWakingUp = false;

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->MaxWalkSpeed = 0.f;

	if (CharacterMesh && CharacterMesh->GetAnimInstance() && RestMontage)
	{
		UAnimInstance* AnimInst = CharacterMesh->GetAnimInstance();
		AnimInst->Montage_Play(RestMontage);
		AnimInst->Montage_JumpToSection(FName("Rest"), RestMontage);
	}
}

void AMeleeEnemy::TriggerUnrest(EAIState TargetState)
{
	if (bIsDead) return;
	
	bIsResting = false;
	bIsWakingUp = true;

	CurrentMovementSpeed = (TargetState == EAIState::Engage) ? EngageSpeed : AlertSpeed;
	GetCharacterMovement()->MaxWalkSpeed = 0.f;

	if (CharacterMesh && CharacterMesh->GetAnimInstance() && RestMontage)
	{
		UAnimInstance* AnimInst = CharacterMesh->GetAnimInstance();
		if (!AnimInst->Montage_IsPlaying(RestMontage))
		{
			AnimInst->Montage_Play(RestMontage);
		}
		AnimInst->Montage_JumpToSection(FName("Unrest"), RestMontage);
	}

	GetWorldTimerManager().SetTimer(
		UnrestTimerHandle,
		this,
		&AMeleeEnemy::OnUnrestFinished,
		UnrestDuration,
		false
	);
}

void AMeleeEnemy::OnUnrestFinished()
{
	if (bIsDead) return;
	
	bIsWakingUp = false;
	Super::SetAIMovementSpeed(CurrentMovementSpeed);
}

void AMeleeEnemy::PlayMeleeMontage_Implementation()
{
	if (MeleeAttackMontages.Num() > 0 && CharacterMesh)
	{
		if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
		{
			const int32 RandomIndex = FMath::RandRange(0, MeleeAttackMontages.Num() - 1);
			if (UAnimMontage* ChosenMontage = MeleeAttackMontages[RandomIndex])
			{
				AnimInstance->Montage_Play(ChosenMontage);
				return;
			}
		}
	}

	Super::PlayMeleeMontage_Implementation();
}