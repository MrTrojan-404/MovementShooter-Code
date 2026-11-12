#include "MovementShooter/Animation/MeleeAnimInstance.h"

#include "MovementShooter/AI/Character/AICharacterBase.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"

void UMeleeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	bUseFABRIK = false;
	bUseAimOffsets = false;
	bTransformRightHand = false;
	bShouldLookAtTarget = false;
	LookAtAlpha = 0.f;

	if (APawn* Pawn = TryGetPawnOwner())
	{
		if (AEnemyAIController* AICon = Cast<AEnemyAIController>(Pawn->GetController()))
		{
			CurrentAIState = AICon->GetAIState();
		}
	}
}

void UMeleeAnimInstance::TriggerHitReact(const FVector& HitWorldDirection, FName HitBone)
{
	APawn* Pawn = TryGetPawnOwner();
	if (!Pawn || !HitReactMontage || bIsMeleeing) return;

	// Never play a hit reaction if the enemy is already dead!
	if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(Pawn))
	{
		if (!AIChar->IsAlive()) return;
	}
	
	FVector LocalHitDir = Pawn->GetActorTransform().InverseTransformVectorNoScale(HitWorldDirection);
	FString BoneString = HitBone.ToString().ToLower();

	FName SectionName = FName("BodyLeft");

	if (LocalHitDir.X > 0.35f)
	{
		SectionName = FName("Back");
	}
	else
	{
		const bool bIsHeadHit = BoneString.Contains("head") || BoneString.Contains("neck");
		bool bHitLeftSide = BoneString.Contains("_l") || BoneString.Contains("left");
		if (!bHitLeftSide && !BoneString.Contains("_r") && !BoneString.Contains("right"))
		{
			bHitLeftSide = (LocalHitDir.Y > 0.f);
		}

		if (bIsHeadHit)
		{
			SectionName = bHitLeftSide ? FName("HeadLeft") : FName("HeadRight");
		}
		else
		{
			SectionName = bHitLeftSide ? FName("BodyLeft") : FName("BodyRight");
		}
	}

	Montage_Play(HitReactMontage);
	Montage_JumpToSection(SectionName, HitReactMontage);
}