
#include "MovementShooter/AI/BehaviorTree/BTDecorator_CanSeeTarget.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTDecorator_CanSeeTarget::UBTDecorator_CanSeeTarget()
{
    NodeName = TEXT("Can See Target");
    FlowAbortMode = EBTFlowAbortMode::Self;

    TargetActorKey.AddObjectFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CanSeeTarget, TargetActorKey), AActor::StaticClass());
    CanSeeTargetKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CanSeeTarget, CanSeeTargetKey));
    IsInGracePeriodKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CanSeeTarget, IsInGracePeriodKey));
    
    // 1. Register the new Cover Key
    IsInCoverKey.AddBoolFilter(this, GET_MEMBER_NAME_CHECKED(UBTDecorator_CanSeeTarget, IsInCoverKey));
}

bool UBTDecorator_CanSeeTarget::CalculateRawConditionValue(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) const
{
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (!BB) return false;

    AActor* Target = Cast<AActor>(BB->GetValueAsObject(TargetActorKey.SelectedKeyName));
    if (!Target) return false;

    bool bCanSee = BB->GetValueAsBool(CanSeeTargetKey.SelectedKeyName);
    bool bInGrace = BB->GetValueAsBool(IsInGracePeriodKey.SelectedKeyName);
    
    // 2. Read the Cover state
    bool bInCover = BB->GetValueAsBool(IsInCoverKey.SelectedKeyName);

    // 3. THE FIX: If they are in cover, NEVER abort the tree, even if they can't see the player!
    return bCanSee || bInGrace || bInCover;
}

FString UBTDecorator_CanSeeTarget::GetStaticDescription() const
{
    return TEXT("Returns true if AI can see target\n"
                 "or is within grace period after losing sight.");
}


