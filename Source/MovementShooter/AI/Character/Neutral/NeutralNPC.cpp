#include "NeutralNPC.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Damage.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/AI/Waypoint/WaypointPath.h"

ANeutralNPC::ANeutralNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	// Invulnerable by default
	SetCanBeDamaged(false);

	bAutoMovementSpeed = true;
	PatrolSpeed = EAIMovementSpeed::Walk;
}

void ANeutralNPC::BeginPlay()
{
	// Call AAICharacterBase::BeginPlay() directly to skip AEnemyBase::BeginPlay()
	// (prevents spawning a dummy 1-point WaypointPath when standing idle)
	AAICharacterBase::BeginPlay();

	SpawnDefaultWeapon(true, false);

	ConfigurePassiveBrain();
}

void ANeutralNPC::ConfigurePassiveBrain()
{
	AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController());
	if (!AICon) return;

	if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
	{
		// Neutral NPCs never shoot
		BB->SetValueAsBool(AEnemyAIController::BB_bWeaponsFree, false);
		AICon->SetAIState(EAIState::Patrol);
	}

	// 1. Disable AI Perception if they shouldn't detect the player (Hideout Cultists & The Leader)
	if (!bEnableLevel1StealthDetection)
	{
		if (UAIPerceptionComponent* Perception = AICon->GetPerceptionComponent())
		{
			Perception->Deactivate();
			Perception->SetSenseEnabled(UAISense_Sight::StaticClass(), false);
			Perception->SetSenseEnabled(UAISense_Hearing::StaticClass(), false);
			Perception->SetSenseEnabled(UAISense_Damage::StaticClass(), false);
		}
	}

	// 2. If no WaypointPath is assigned, they are a standing Idle NPC -> shut down the Behavior Tree!
	if (WaypointPath == nullptr)
	{
		if (AICon->GetBrainComponent())
		{
			AICon->GetBrainComponent()->StopLogic(TEXT("Standing Idle Neutral NPC"));
		}
	}
}

void ANeutralNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// --- LEADER WALK-AND-TALK DISTANCE TETHER ---
	if (bWaitForPlayerWhenGuiding && WaypointPath != nullptr)
	{
		APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController());
		if (!PlayerPawn || !AICon) return;

		const float DistToPlayer = FVector::Dist(GetActorLocation(), PlayerPawn->GetActorLocation());

		if (!bIsWaitingForPlayer && DistToPlayer > MaxLeadDistance)
		{
			// Player fell behind: pause movement and look back at the player
			bIsWaitingForPlayer = true;
			GetCharacterMovement()->MaxWalkSpeed = 0.f;
			AICon->SetFocus(PlayerPawn);
		}
		else if (bIsWaitingForPlayer && DistToPlayer <= ResumeWalkDistance)
		{
			// Player caught up: clear focus and resume walking speed
			bIsWaitingForPlayer = false;
			AICon->ClearFocus(EAIFocusPriority::Gameplay);
			SetAIMovementSpeed(PatrolSpeed);
		}
	}
}

void ANeutralNPC::StartGuidingAlongPath(AWaypointPath* NewPath)
{
	if (!NewPath) return;

	WaypointPath = NewPath;
	bIsWaitingForPlayer = false;
	SetAIMovementSpeed(PatrolSpeed);

	if (AEnemyAIController* AICon = Cast<AEnemyAIController>(GetController()))
	{
		AICon->ClearFocus(EAIFocusPriority::Gameplay);
		AICon->SetWaypointIndex(0);
		AICon->SetAIState(EAIState::Patrol);

		if (AICon->GetBrainComponent())
		{
			AICon->GetBrainComponent()->RestartLogic();
		}
	}
}

void ANeutralNPC::Interact_Implementation(AActor* Interactor)
{
	if (!bCanInteract) return;

	// Turn to look at the player when spoken to
	if (AAIController* AICon = Cast<AAIController>(GetController()))
	{
		AICon->SetFocus(Interactor);
	}

	if (CurrentDialogue)
	{
		OnPlayDialogue(CurrentDialogue);
	}
}

FText ANeutralNPC::GetInteractionText_Implementation() const
{
	return bCanInteract ? InteractionPrompt : FText::GetEmpty();
}