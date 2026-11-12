#include "BTTask_EnterCover.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MovementShooter/AI/Actor/CoverPoint.h"
#include "MovementShooter/AI/Character/AICharacterBase.h"


UBTTask_EnterCover::UBTTask_EnterCover()
{
	NodeName = TEXT("Enter Cover");
}

EBTNodeResult::Type UBTTask_EnterCover::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	AAICharacterBase* AIChar = AICon ? Cast<AAICharacterBase>(AICon->GetPawn()) : nullptr;
	
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	AActor* TargetActor = BB ? Cast<AActor>(BB->GetValueAsObject(FName("TargetActor"))) : nullptr;
	ACoverPoint* CoverNode = BB ? Cast<ACoverPoint>(BB->GetValueAsObject(FName("CoverPoint"))) : nullptr; 

	if (!AIChar || !TargetActor || !CoverNode) return EBTNodeResult::Failed;

	AIChar->GetCharacterMovement()->StopMovementImmediately();
	AICon->ClearFocus(EAIFocusPriority::Gameplay);
	AICon->ClearFocus(EAIFocusPriority::Default);

	// 1. TRACE BACKWARDS TO FIND THE ACTUAL WALL
	// The arrow points away from the wall, so we trace in the exact opposite direction
	FVector TraceStart = CoverNode->GetActorLocation();
	FVector TraceEnd = TraceStart - (CoverNode->GetActorForwardVector() * 200.f); 
	
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(AIChar);

	FVector FinalLocation = CoverNode->GetActorLocation();
	FRotator FinalRotation = CoverNode->GetActorRotation();

	if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		// Find the exact radius of this specific enemy's capsule
		float CapsuleRadius = AIChar->GetCapsuleComponent()->GetScaledCapsuleRadius();

		// Push the AI away from the wall exactly enough to fit their capsule, plus a 5 unit buffer
		FinalLocation = Hit.ImpactPoint + (Hit.ImpactNormal * CapsuleRadius);

		// Align them perfectly flush against the geometry instead of relying on the designer's arrow!
		FinalRotation = Hit.ImpactNormal.Rotation();
		FinalRotation.Pitch = 0.f; // Keep them upright
		FinalRotation.Roll = 0.f;
	}

	// Snap the capsule perfectly against the wall
	AIChar->SetActorLocationAndRotation(FinalLocation, FinalRotation);

	// 2. BLACKBOARD SETUP & DIRECTION LOGIC
	bool bIsRight = true; // Default to right

	// If BOTH sides are open, dynamically pick the side closest to the player
	if (CoverNode->bCanPeekLeft && CoverNode->bCanPeekRight)
	{
		FVector DirToPlayer = (TargetActor->GetActorLocation() - AIChar->GetActorLocation()).GetSafeNormal();
		bIsRight = FVector::DotProduct(AIChar->GetActorRightVector(), DirToPlayer) > 0.f;
	}
	// If only ONE side is open, strictly enforce the level designer's choice
	else if (CoverNode->bCanPeekRight)
	{
		bIsRight = true;
	}
	else if (CoverNode->bCanPeekLeft)
	{
		bIsRight = false;
	}

	BB->SetValueAsBool(FName("CoverRight"), bIsRight);
	BB->SetValueAsBool(FName("IsInCover"), true);

	// 3. PLAY IN-PLACE MONTAGE
	if (AIChar->EnterCoverMontage)
	{
		FName SectionName = bIsRight ? FName("Right") : FName("Left");
		AIChar->PlayAnimMontage(AIChar->EnterCoverMontage, 1.f, SectionName);
	}

	return EBTNodeResult::Succeeded;
}