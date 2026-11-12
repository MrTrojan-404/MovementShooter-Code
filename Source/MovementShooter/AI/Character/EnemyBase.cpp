
#include "MovementShooter/AI/Character/EnemyBase.h"

#include "Components/CapsuleComponent.h"
#include "Components/SplineComponent.h"
#include "MovementShooter/MovementShooter.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/AI/Waypoint/WaypointPath.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/Objective/ObjectiveTriggerComponent.h"

AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = true;

    // AI controller class
    AIControllerClass = AEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    ObjectiveTrigger = CreateDefaultSubobject<UObjectiveTriggerComponent>(TEXT("ObjectiveTrigger"));
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    SpawnDefaultWeapon(true, false);
    
    PatrolOrigin = GetActorLocation();
    if (GetCapsuleComponent()) PatrolOrigin.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    
    // DYNAMIC GUARD POST
    if (WaypointPath == nullptr)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        
        WaypointPath = GetWorld()->SpawnActor<AWaypointPath>(AWaypointPath::StaticClass(), PatrolOrigin, FRotator::ZeroRotator, SpawnParams);
        
        if (WaypointPath && WaypointPath->GetSpline())
        {
            WaypointPath->GetSpline()->ClearSplinePoints(true);
            WaypointPath->GetSpline()->AddSplinePoint(PatrolOrigin, ESplineCoordinateSpace::World, true);

            // Manually set PointData for the single guard point
            WaypointPath->PointData.Empty();
            FWaypointSplinePoint GuardPoint;
            GuardPoint.WaitTime = 3.f;
            WaypointPath->PointData.Add(GuardPoint);
        }
    }
}

void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // --- CLEAR LEASH RESET ---
    if (bIsResetting)
    {
        float DistFromPatrol = FVector::Dist(GetActorLocation(), PatrolOrigin);
        
        // Force the AI to walk almost all the way home (within 500 units) before dropping invulnerability!
        if (DistFromPatrol <= 500.f)
        {
            bIsResetting = false;
            
            // Now that we are safely home, we can resume normal Patrolling!
            if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
            {
                AICon->SetAIState(EAIState::Patrol);
            }
            
            UE_LOG(LogAI, Warning, TEXT("[RESET] Enemy returned home. Resetting disabled."));
            if (GEngine) 
                GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("RESET CLEARED! Ready to fight."));
        }
    }
}

void AEnemyBase::TriggerLeashReset()
{
    if (bIsResetting) return;

    bIsResetting = true;
    
    // Fully heal the enemy. 
    Health = MaxHealth; 
    
    // Force the AI to stop aiming and firing so it can freely sprint back home.
    if (CombatComponent)
    {
        CombatComponent->SetAiming(false);
        CombatComponent->HandleFire(false);
    }

    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("ENEMY RESET: Full Heal & Invulnerable!"));
}

void AEnemyBase::Destroyed()
{
    Super::Destroyed();
}

void AEnemyBase::HandleDeath(AController* InstigatorController, AActor* DamageCauser)
{
    // 1. Shout to the Objective Subsystem that we died!
    if (ObjectiveTrigger)
    {
        ObjectiveTrigger->TriggerEvent();
    }

    // 2. Let the base AI class handle the ragdoll, weapon drops, and memory wiping
    Super::HandleDeath(InstigatorController, DamageCauser);
}

