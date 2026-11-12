
#include "MovementShooter/AI/BehaviorTree/BTTask_GetNextWaypoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Components/SplineComponent.h"
#include "MovementShooter/MovementShooter.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "MovementShooter/AI/Character/Neutral/NeutralNPC.h"
#include "MovementShooter/AI/Waypoint/WaypointPath.h"

UBTTask_GetNextWaypoint::UBTTask_GetNextWaypoint()
{
    NodeName = TEXT("Get Next Waypoint");

    // Filter keys to correct types
    WaypointIndexKey.AddIntFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, WaypointIndexKey));

    PingPongForwardKey.AddBoolFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, PingPongForwardKey));

    TargetLocationKey.AddVectorFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, TargetLocationKey));

    WaitTimeKey.AddFloatFilter(
        this, GET_MEMBER_NAME_CHECKED(
            UBTTask_GetNextWaypoint, WaitTimeKey));
}

EBTNodeResult::Type UBTTask_GetNextWaypoint::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory)
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return EBTNodeResult::Failed;

    AAIController* Controller = OwnerComp.GetAIOwner();
    if (!Controller) return EBTNodeResult::Failed;

    AEnemyBase* Enemy = Cast<AEnemyBase>(Controller->GetPawn());
    if (!Enemy) return EBTNodeResult::Failed;

    AWaypointPath* Path = Enemy->WaypointPath;
    if (!Path || !Path->IsValid())
    {
        return EBTNodeResult::Failed;
    }
    
    int32 CurrentIndex = BB->GetValueAsInt(WaypointIndexKey.SelectedKeyName);
    bool bForward = BB->GetValueAsBool(PingPongForwardKey.SelectedKeyName);
    int32 NextIndex = Path->GetNextIndex(CurrentIndex, bForward);
    FVector NextLocation = Path->GetLocationAtIndex(NextIndex);

    if (NextLocation == FVector::ZeroVector)
    {
        return EBTNodeResult::Failed;
    }

    // Update blackboard...
    BB->SetValueAsInt(WaypointIndexKey.SelectedKeyName, NextIndex);
    BB->SetValueAsBool(PingPongForwardKey.SelectedKeyName, bForward);
    BB->SetValueAsVector(TargetLocationKey.SelectedKeyName, NextLocation);

    FWaypointSplinePoint PointData = Path->GetPointData(NextIndex);
    
    BB->SetValueAsFloat(WaitTimeKey.SelectedKeyName, PointData.WaitTime);

    // Trigger ArrivalDialogue if this spline point has one!
    if (PointData.ArrivalDialogue)
    {
        if (ANeutralNPC* Neutral = Cast<ANeutralNPC>(Enemy))
        {
            Neutral->OnPlayDialogue(PointData.ArrivalDialogue);
        }
    }
    
    return EBTNodeResult::Succeeded;
}

FString UBTTask_GetNextWaypoint::GetStaticDescription() const
{
    return TEXT("Gets next waypoint location from enemy path.\n"
                 "Updates WaypointIndex and TargetLocation.");
}

