#include "BTTask_JetpackLand.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "MovementShooter/AI/Boss/Boss1/Boss_Jetpack.h"

UBTTask_JetpackLand::UBTTask_JetpackLand()
{
	NodeName = TEXT("Jetpack Land");
	bNotifyTick = true;
	bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_JetpackLand::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AAIController* AICon = OwnerComp.GetAIOwner();
	CachedBoss = AICon ? Cast<ABoss_Jetpack>(AICon->GetPawn()) : nullptr;
	if (!CachedBoss) return EBTNodeResult::Failed;

	ElapsedTime = 0.f;

	// StopHover also fires the "hover off" VFX event, so only fire it ourselves
	// at touchdown if the boss wasn't in the C++ hover state to begin with.
	bNeedsVFXOff = !CachedBoss->IsHovering();

	//  release the Phase 2 altitude hold. While bIsHovering is true, Boss_Jetpack::Tick
	// keeps pulling the boss back up toward TargetHoverZ, which fights this task's descent and
	// leaves him stuck above the floor. StopHover also clears the fuel timer and the BB flag.
	// It early-outs harmlessly if he wasn't hovering.
	CachedBoss->StopHover(false);

	// Let the character movement component do the drop so collision is respected
	UCharacterMovementComponent* Move = CachedBoss->GetCharacterMovement();
	Move->SetMovementMode(MOVE_Falling);
	Move->Velocity = FVector(0.f, 0.f, -DropSpeed);

	return EBTNodeResult::InProgress;
}

void UBTTask_JetpackLand::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	if (!CachedBoss)
	{
		FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
		return;
	}

	UCharacterMovementComponent* Move = CachedBoss->GetCharacterMovement();
	ElapsedTime += DeltaSeconds;

	// CMC switches to Walking by itself when he touches down
	if (Move->IsMovingOnGround())
	{
		if (bNeedsVFXOff) CachedBoss->OnHoverStateChanged(false, false);
		FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
		return;
	}

	// Fail-safe so the behavior tree can never hang on this node
	if (ElapsedTime >= MaxLandTime)
	{
		const bool bSnapped = SnapToFloor();
		if (bNeedsVFXOff) CachedBoss->OnHoverStateChanged(false, false);
		FinishLatentTask(OwnerComp, bSnapped ? EBTNodeResult::Succeeded : EBTNodeResult::Failed);
	}
}

bool UBTTask_JetpackLand::SnapToFloor()
{
	if (!CachedBoss) return false;

	const FVector Start = CachedBoss->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, 5000.f);

	// Only hit level geometry, so the player or the boss's weapon can't be mistaken for the floor
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CachedBoss);
	TArray<AActor*> Attached;
	CachedBoss->GetAttachedActors(Attached);
	Params.AddIgnoredActors(Attached);

	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, ObjParams, Params))
	{
		return false;
	}

	const float CapHalfHeight = CachedBoss->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	CachedBoss->SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, CapHalfHeight + 2.f), false, nullptr, ETeleportType::TeleportPhysics);

	UCharacterMovementComponent* Move = CachedBoss->GetCharacterMovement();
	Move->Velocity = FVector::ZeroVector;
	Move->SetMovementMode(MOVE_Walking);
	return true;
}