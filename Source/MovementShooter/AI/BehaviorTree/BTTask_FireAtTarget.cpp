
#include "MovementShooter/AI/BehaviorTree/BTTask_FireAtTarget.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

#include "MovementShooter/AI/Character/AICharacterBase.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/Component/CombatComponent.h"

UBTTask_FireAtTarget::UBTTask_FireAtTarget()
{
    NodeName = TEXT("Fire At Target");
    bNotifyTick = true;
    bCreateNodeInstance = true; 
    
    TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FireAtTarget, TargetActorKey), AActor::StaticClass());
}

EBTNodeResult::Type UBTTask_FireAtTarget::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    // Never shoot if trapped
    if (BB && (BB->GetValueAsBool(AEnemyAIController::BB_bIsFrozen) || BB->GetValueAsBool(AEnemyAIController::BB_bIsCaged) || BB->GetValueAsBool(AEnemyAIController::BB_bIsInRagdoll)))
    {
        return EBTNodeResult::Failed; 
    }
    
    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!Controller) return EBTNodeResult::Failed;

    ABaseCharacter* AICharacter = Cast<ABaseCharacter>(Controller->GetPawn());
    if (!AICharacter) return EBTNodeResult::Failed;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!Target) return EBTNodeResult::Failed;

    UCombatComponent* Combat = AICharacter->GetCombatComponent();
    
    // Ensure the pointer is valid
    if (!Combat || !Combat->GetPrimaryWeapon())
    {
        return EBTNodeResult::Failed;
    }

    //  If the weapon is empty, SUCCEED immediately. 
    // This allows the Simple Parallel to finish so the AI can run to cover!
    if (Combat->GetPrimaryWeapon()->IsEmpty())
    {
        return EBTNodeResult::Succeeded;
    }
    // Cache everything
    CachedCombat = Combat;
    CachedOwnerComp = &OwnerComp;
    CachedAICharacter = AICharacter;

    // Reset error so they track perfectly while aiming
    CurrentBurstError = FVector::ZeroVector;

    // Face the target instantly to begin tracking
    float AimZOffset = 30.f;
    if (AAICharacterBase* AsAI = Cast<AAICharacterBase>(AICharacter)) AimZOffset = AsAI->AimZOffset;
    
    FVector TargetLocation = Target->GetActorLocation();
    TargetLocation.Z += AimZOffset;
    Combat->SetHitTarget(TargetLocation);
    FVector Direction = (TargetLocation - AICharacter->GetActorLocation()).GetSafeNormal();
    Controller->SetControlRotation(Direction.Rotation());

    // START AIMING (Raises weapon)
    Combat->SetAiming(true);

    // FETCH DELAY: Use Sniper's custom duration if it exists, otherwise use Grunt default
    float AimDelay = DefaultAimDelay;
    if (BB->IsValidKey(BB->GetKeyID(FName("AimDuration"))))
    {
        float BBAimDelay = BB->GetValueAsFloat(FName("AimDuration"));
		
        // ONLY override the DefaultAimDelay if the Blackboard value is actually set!
        if (BBAimDelay > 0.f)
        {
            AimDelay = BBAimDelay;
        }
    }
    if (AimDelay <= 0.05f) AimDelay = 0.05f;

    // Wait for the Aiming Phase to finish before firing!
    AICharacter->GetWorldTimerManager().SetTimer(AimTimer, this, &UBTTask_FireAtTarget::BeginFire, AimDelay, false);

    return EBTNodeResult::InProgress;
}

void UBTTask_FireAtTarget::BeginFire()
{
    if (!CachedCombat || !CachedOwnerComp || !CachedAICharacter) return;

    UBlackboardComponent* BB = CachedOwnerComp->GetBlackboardComponent();
    AActor* Target = BB ? Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName)) : nullptr;
    if (!Target)
    {
        StopFiring();
        return;
    }

    // 1. Generate Error Mistake right as they pull the trigger
    float AimErrorRad = 30.f;
    if (AAICharacterBase* AsAI = Cast<AAICharacterBase>(CachedAICharacter)) AimErrorRad = AsAI->AimErrorRadius;
    CurrentBurstError = FMath::VRand() * FMath::RandRange(0.f, AimErrorRad);

    // 2. THE FIX: Dynamic Burst Logic respects the boolean!
    float CalculatedFireDuration = FireDuration;

    if (bUseDynamicBurst)
    {
        float DistanceToTarget = FVector::Dist(CachedAICharacter->GetActorLocation(), Target->GetActorLocation());
        if (DistanceToTarget < 600.f) CalculatedFireDuration = FMath::RandRange(1.5f, 3.0f);
        else CalculatedFireDuration = FMath::RandRange(0.3f, 0.6f);
    }

    // 3. START FIRING
    CachedCombat->HandleFire(true);

    // 4. Schedule the Stop Firing event
    CachedAICharacter->GetWorldTimerManager().SetTimer(FireStopTimer, this, &UBTTask_FireAtTarget::StopFiring, CalculatedFireDuration, false);
}

void UBTTask_FireAtTarget::TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickTask(OwnerComp, NodeMemory, DeltaSeconds);

    if (!CachedCombat || !CachedAICharacter) return;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    AAIController* Controller = OwnerComp.GetAIOwner();
    
    if (BB && Controller)
    {
        AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));

        if (Target)
        {
            float AimZOffset = 30.f;
            if (AAICharacterBase* AsAI = Cast<AAICharacterBase>(CachedAICharacter)) AimZOffset = AsAI->AimZOffset;

            FVector TargetLocation = Target->GetActorLocation();
            TargetLocation.Z += AimZOffset;

            // ---  Line of Sight Check ---
            FHitResult Hit;
            FCollisionQueryParams Params;
            Params.AddIgnoredActor(CachedAICharacter);
            Params.AddIgnoredActor(Target);

            FVector StartTrace = CachedAICharacter->GetActorLocation() + FVector(0.f, 0.f, AimZOffset);
            bool bHitWall = GetWorld()->LineTraceSingleByChannel(Hit, StartTrace, TargetLocation, ECC_Visibility, Params);

            if (bHitWall)
            {
                // Stop pulling the trigger if a wall is in the way!
                if (CachedCombat->GetFireButtonPressed()) CachedCombat->HandleFire(false);
            }
            else
            {
                // Resume pulling the trigger when line of sight is clear
                if (!CachedCombat->GetFireButtonPressed()) CachedCombat->HandleFire(true);
            }

            TargetLocation += CurrentBurstError; 
            CachedCombat->SetHitTarget(TargetLocation);

            FVector Direction = (TargetLocation - CachedAICharacter->GetActorLocation()).GetSafeNormal();
            FRotator CurrentRot = Controller->GetControlRotation();
				
            FRotator SmoothRot = FMath::RInterpTo(CurrentRot, Direction.Rotation(), DeltaSeconds, 10.f);
            Controller->SetControlRotation(SmoothRot);
        }
    }
}

void UBTTask_FireAtTarget::StopFiring()
{
    if (CachedCombat)
    {
        CachedCombat->SetAiming(false);
        CachedCombat->HandleFire(false);
        CachedCombat->SetHitTarget(FVector::ZeroVector);
        CachedCombat = nullptr;
    }

    if (CachedOwnerComp)
    {
        FinishLatentTask(*CachedOwnerComp, EBTNodeResult::Succeeded);
        CachedOwnerComp = nullptr;
    }
    CachedAICharacter = nullptr;
}

EBTNodeResult::Type UBTTask_FireAtTarget::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    if (CachedAICharacter)
    {
        CachedAICharacter->GetWorldTimerManager().ClearTimer(AimTimer);
        CachedAICharacter->GetWorldTimerManager().ClearTimer(FireStopTimer);
    }
    if (CachedCombat)
    {
        CachedCombat->SetAiming(false);
        CachedCombat->HandleFire(false);
        CachedCombat->SetHitTarget(FVector::ZeroVector);
    }
    CachedCombat = nullptr;
    CachedAICharacter = nullptr;
    CachedOwnerComp = nullptr;
    
    return EBTNodeResult::Aborted;
}

FString UBTTask_FireAtTarget::GetStaticDescription() const
{
    return TEXT("Aims for AimDuration (or 0.5s), then fires a burst.");
}

