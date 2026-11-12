#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/WidgetComponent.h"
#include "MovementShooter/MovementShooter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "MovementShooter/AI/EnemyAITypes.h"
#include "MovementShooter/AI/Character/AICharacterBase.h"
#include "MovementShooter/AI/Boss/BossBase.h"
#include "MovementShooter/AI/Character/MeleeEnemy.h"
#include "MovementShooter/AI/Character/SniperEnemy.h"
#include "MovementShooter/AI/Character/Neutral/NeutralNPC.h"
#include "MovementShooter/AI/Waypoint/WaypointPath.h"
#include "MovementShooter/Character/PlayerCharacter.h"
#include "MovementShooter/Component/StealthComponent.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"
#include "MovementShooter/Subsystem/StealthSubsystem.h"
#include "MovementShooter/UI/Widgets/DirectionalIndicatorWidget.h"

// Blackboard key definitions
const FName AEnemyAIController::BB_TargetActor = TEXT("TargetActor");
const FName AEnemyAIController::BB_LastKnownLocation = TEXT("LastKnownLocation");
const FName AEnemyAIController::BB_bCanSeeTarget = TEXT("bCanSeeTarget");
const FName AEnemyAIController::BB_bHeardSound = TEXT("bHeardSound");
const FName AEnemyAIController::BB_SoundLocation = TEXT("SoundLocation");
const FName AEnemyAIController::BB_WaypointIndex = TEXT("WaypointIndex");
const FName AEnemyAIController::BB_bPingPongForward = TEXT("bPingPongForward");
const FName AEnemyAIController::BB_AIState = TEXT("AIState");
const FName AEnemyAIController::BB_bIsInGracePeriod = TEXT("bIsInGracePeriod");
const FName AEnemyAIController::BB_EnemyType = TEXT("EnemyType");
const FName AEnemyAIController::BB_PreferredRange = TEXT("PreferredRange");
const FName AEnemyAIController::BB_CombatStyle = TEXT("CombatStyle");
const FName AEnemyAIController::BB_WaypointWaitTime = TEXT("WaypointWaitTime");
const FName AEnemyAIController::BB_bIsCaged = TEXT("bIsCaged");
const FName AEnemyAIController::BB_bIsFrozen = TEXT("bIsFrozen");
const FName AEnemyAIController::BB_bIsInStasis = TEXT("bIsInStasis");
const FName AEnemyAIController::BB_bIsInRagdoll = TEXT("bIsInRagdoll");
const FName AEnemyAIController::BB_bIsSuspended = TEXT("bIsSuspended");
const FName AEnemyAIController::BB_bIsElectrocuted = TEXT("bIsElectrocuted");
const FName AEnemyAIController::BB_DistanceToTarget = TEXT("DistanceToTarget");

const FName AEnemyAIController::BB_bWeaponsFree = TEXT("bWeaponsFree");

AEnemyAIController::AEnemyAIController()
{
	// Create perception component normally
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(
		TEXT("AIPerception"));

	// Create sense configs using NewObject instead of 
	// CreateDefaultSubobject
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(
		TEXT("SightConfig"));
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(
		TEXT("HearingConfig"));
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(
		TEXT("DamageConfig"));

	// Configure defaults here so they show in details panel
	SightConfig->SightRadius = 4000.f;
	SightConfig->LoseSightRadius = SightConfig->SightRadius + 500.f; 
	SightConfig->PeripheralVisionAngleDegrees = 60.f;
	SightConfig->SetMaxAge(0);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->NearClippingRadius = 50.f;

	// Disable the AI's "Sixth Sense" close-range cheating
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;

	HearingConfig->HearingRange = 5500.f; // 50m
	HearingConfig->SetMaxAge(3.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
	
	DamageConfig->SetMaxAge(5.f);

	// Add senses to perception component in constructor
	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	AIPerception->ConfigureSense(*DamageConfig);
	AIPerception->SetDominantSense(
		SightConfig->GetSenseImplementation());

	if (GetWorld() && UAIPerceptionSystem::GetCurrent(GetWorld()))
	{
		UAIPerceptionSystem::GetCurrent(GetWorld())->UpdateListener(*AIPerception);
	}
	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateDetectionMeter(DeltaTime);
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();

	if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
	{
		Sub->OnGlobalAlert.AddDynamic(this, &AEnemyAIController::OnGlobalAlert);
}
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// 0. Cache the body for the UI!
	CachedUIBody = InPawn;

	// 1. Broad cast to Base Character
	OwnedBaseChar = Cast<ABaseCharacter>(InPawn);
	if (!OwnedBaseChar) return;

	// --- DESTROY UNUSED UI ---
	// If this is a regular combat enemy, they don't use stealth math, 
	// so completely destroy the floating widget component to save memory!
	if (!ShouldUseStealthMeter())
	{
		if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
		{
			if (AIChar->StealthIndicatorWidget)
			{
				AIChar->StealthIndicatorWidget->DestroyComponent();
			}
		}
	}
	InitializePerception(OwnedBaseChar);

	// 2. Brain selection: the Blueprint's own tree first, the controller default as a fallback
	if (OwnedBaseChar->DefaultBehaviorTree) RunBehaviorTree(OwnedBaseChar->DefaultBehaviorTree);
	else if (EnemyBehaviorTree) RunBehaviorTree(EnemyBehaviorTree);

	UE_LOG(LogAI, Warning, TEXT("[OnPossess] BlackboardComponent after run: %p"),
	GetBlackboardComponent());

	// Push the freeze time to the Blackboard
	if (UBlackboardComponent* BBComp = GetBlackboardComponent())
	{
		BBComp->SetValueAsFloat(FName("SurpriseDuration"), SurpriseFreezeTime);
	}

	SetupBlackboard(OwnedBaseChar);

	GetWorldTimerManager().SetTimer(TargetSweepTimer, this, &AEnemyAIController::PerformTargetSweep, 0.2f, true);
}

void AEnemyAIController::OnUnPossess()
{
	// We MUST push a 0 instead of instantly deleting, so the Client UI gets the message to remove the arrow.
	// Call this BEFORE Super::OnUnPossess() so GetPawn() is still valid for the Stealth Subsystem!
	ClearStealthUI();

	Super::OnUnPossess();

	GetWorldTimerManager().ClearTimer(GracePeriodTimer);
	GetWorldTimerManager().ClearTimer(TargetSweepTimer);
    
	// Delete the radio timer if the brain is detached
	GetWorldTimerManager().ClearTimer(GlobalAlertDelayTimer);

	OwnedBaseChar = nullptr;
}

void AEnemyAIController::PerformTargetSweep()
{
	// If the AI is dead, stop thinking
	if (GetAIState() == EAIState::Dead || !AIPerception || !OwnedBaseChar) return;

	if (!GetBlackboardComponent() || !GetPerceptionComponent() || !GetPawn())
	{
		return; // Silently skip this tick if the AI is dead or loading
	}

	// --- Stop scanning for new targets while in stasis ---
	if (IsInStasis()) return;

	TArray<AActor*> PerceivedActors;
	AIPerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), PerceivedActors);

	AActor* BestTarget = nullptr;
	float MinDistSq = MAX_flt;

	const float MaxSightDistance = SightConfig ? SightConfig->SightRadius : 3000.f;
	const float MaxSightSq = MaxSightDistance * MaxSightDistance;

	for (AActor* Actor : PerceivedActors)
	{
		if (!Actor || Actor->IsActorBeingDestroyed()) continue;

		const bool bIsEnemy = Cast<AEnemyBase>(Actor) != nullptr;
		const bool bIsPlayer = Cast<APlayerCharacter>(Actor) != nullptr;

		// ==========================================
		// VISUAL STEALTH DISTURBANCES
		// If I am a Guard, and I see a fellow Guard in trouble...
		// ==========================================
		if (bIsEnemy)
		{
			if (AAICharacterBase* SeenAlly = Cast<AAICharacterBase>(Actor))
			{
				bool bIsDead = !SeenAlly->IsAlive();
				bool bIsCCd = SeenAlly->bDisableGameplay; // True during Electrocution
				bool bInTrap = false;

				if (AEnemyAIController* AllyAI = Cast<AEnemyAIController>(SeenAlly->GetController()))
				{
					bInTrap = AllyAI->IsInStasis(); // Covers Stasis
					if (UBlackboardComponent* BB = AllyAI->GetBlackboardComponent())
					{
						// Covers Heavies in Suspension Matrix and Lights in Ragdoll (Gravity Grenades)
						bInTrap = bInTrap || BB->GetValueAsBool(AEnemyAIController::BB_bIsSuspended)
										  || BB->GetValueAsBool(AEnemyAIController::BB_bIsInRagdoll);
					}
				}

				// If I see a dead, trapped, or electrocuted buddy!
				if (bIsDead || bIsCCd || bInTrap)
				{
					// If I'm not already in combat, freak out and investigate!
					if (AlertPhase < EAlertPhase::Caution && GetAIState() != EAIState::Engage)
					{
						SetAlertPhase(EAlertPhase::Caution);
						DetectionMeter = FMath::Max(DetectionMeter, CautionThreshold);
						SetLastKnownLocation(SeenAlly->GetActorLocation());
						SetAIState(EAIState::Alert);
						StartAlertTimer();

						if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
						{
							BTComp->RestartTree();
						}
					}
				}
			}
			continue; // Guards don't target other Guards to shoot, so skip to the next actor!
		}

		// ==========================================
		// NORMAL TARGETING LOGIC
		// ==========================================
		// Skip dead bodies for combat targeting so we don't shoot corpses
		if (AAICharacterBase* BaseAI = Cast<AAICharacterBase>(Actor))
		{
			if (!BaseAI->IsAlive()) continue;
		}

		// Enemies only fight the player
		if (!bIsPlayer) continue;

		float DistSq = FVector::DistSquared(OwnedBaseChar->GetActorLocation(), Actor->GetActorLocation());

		if (DistSq > MaxSightSq) continue;

		if (DistSq < MinDistSq)
		{
			MinDistSq = DistSq;
			BestTarget = Actor;
		}
	}

	AActor* CurrentTarget = Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor));

	// CHECK IF OUR TARGET WAS DEFEATED SO WE DON'T RUSH THE BODY
	if (CurrentTarget)
	{
		bool bTargetDefeated = false;

		if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(CurrentTarget))
		{
			if (!AIChar->IsAlive()) bTargetDefeated = true;
		}
		else if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(CurrentTarget))
		{
			if (PlayerChar->IsElimmed()) bTargetDefeated = true;
		}

		if (bTargetDefeated)
		{
			ClearTarget();
			CurrentTarget = nullptr;
			SetAIState(EAIState::Alert);
			StartAlertTimer();
		}
	}

	if (BestTarget)
	{
		if (AlertPhase != EAlertPhase::Alert)
		{
			SuspectedActor = BestTarget;
		}
		else if (CurrentTarget != BestTarget)
		{
			SetTargetActor(BestTarget);
			SetAIState(EAIState::Engage);

			if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
			{
				BTComp->RestartTree();
			}
		}
	}
}

bool AEnemyAIController::ShouldUseStealthMeter() const
{
	if (ANeutralNPC* Neutral = Cast<ANeutralNPC>(GetPawn()))
	{
		return Neutral->bEnableLevel1StealthDetection;
	}
	// Regular Melee, Ranged, Heavy, and Bosses return false!
	return false;
}

#pragma region Perception

void AEnemyAIController::InitializePerception(ABaseCharacter* BaseChar)
{
	if (!BaseChar || !SightConfig || !HearingConfig) return;

	// Use the enemy's own tuned stats
	if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(BaseChar))
	{
		SightConfig->SightRadius = AIChar->SightRadius;
		SightConfig->LoseSightRadius = AIChar->SightRadius + 100.f;
		SightConfig->PeripheralVisionAngleDegrees = AIChar->PeripheralVisionAngle;
		HearingConfig->HearingRange = AIChar->HearingRange;
	}

	// Enemies must have real line of sight: no cheating through walls.
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.f;

	AIPerception->ConfigureSense(*SightConfig);
	AIPerception->ConfigureSense(*HearingConfig);
	AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SightConfig->SetMaxAge(0.f);

	AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AEnemyAIController::OnTargetPerceptionUpdated);
}

void AEnemyAIController::SetupBlackboard(ABaseCharacter* BaseChar)
{
	if (!Blackboard) return;

	SetAIState(EAIState::Patrol);
	if (AAICharacterBase* StartChar = Cast<AAICharacterBase>(BaseChar))
	{
		StartChar->ApplyMovementSpeedForState(EAIState::Patrol); // the state is already Patrol, so SetAIState did nothing
	}
	SetCanSeeTarget(false);
	SetHeardSound(false, FVector::ZeroVector);
	SetWaypointIndex(0);
	SetPingPongForward(true);
	SetIsInGracePeriod(false);

	// Hostile enemies are always weapons free.
	Blackboard->SetValueAsBool(BB_bWeaponsFree, true);

	// Pull the enemy's specific stats
	if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(BaseChar))
	{
		Blackboard->SetValueAsInt(BB_EnemyType, AIChar->GetEnemyType());
		Blackboard->SetValueAsInt(BB_CombatStyle, AIChar->CombatStyle);

		if (AMeleeEnemy* Melee = Cast<AMeleeEnemy>(AIChar)) SetPreferredRange(Melee->MeleeRange);
		else if (ARangedEnemy* Ranged = Cast<ARangedEnemy>(AIChar)) SetPreferredRange(Ranged->EngagementRange);
		else if (ASniperEnemy* Sniper = Cast<ASniperEnemy>(AIChar))
		{
			SetPreferredRange(Sniper->PreferredCombatDistance);
			Blackboard->SetValueAsFloat(FName("AimDuration"), Sniper->AimDuration);
		}
	}
}

void AEnemyAIController::OnTargetPerceptionUpdated(
	AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

	if (!GetBlackboardComponent() || !GetPawn())
	{
		return;
	}
	
	FString SenseType = TEXT("Unknown");
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
		SenseType = TEXT("Sight");
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
		SenseType = TEXT("Hearing");
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
		SenseType = TEXT("Damage");

	UE_LOG(LogAI, Warning,
	       TEXT("[Perception] Sense: %s | Actor: %s | Sensed: %s"),
	       *SenseType,
	       *Actor->GetName(),
	       Stimulus.WasSuccessfullySensed() ? TEXT("YES") : TEXT("NO"));

	if (Stimulus.Type ==
		UAISense::GetSenseID<UAISense_Sight>())
		HandleSightStimulus(Actor, Stimulus);
	else if (Stimulus.Type ==
		UAISense::GetSenseID<UAISense_Hearing>())
		HandleHearingStimulus(Actor, Stimulus);
	else if (Stimulus.Type ==
		UAISense::GetSenseID<UAISense_Damage>())
		HandleDamageStimulus(Actor, Stimulus);
}

void AEnemyAIController::HandleSightStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!OwnedBaseChar || !Actor) return;

	if (!GetBlackboardComponent() || !GetPawn())
	{
		return; // Ignore sight if the brain isn't fully loaded!
	}

	// --- Ignore visual changes while in stasis ---
	if (IsInStasis()) return;

	// Enemies being reset by a checkpoint restore don't react
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(OwnedBaseChar))
	{
		if (Enemy->bIsResetting) return;
	}

	// Enemies only react to the player
	if (!Cast<APlayerCharacter>(Actor)) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		bIsVisuallySensingTarget = true;
		SetLastKnownLocation(Actor->GetActorLocation());
		SetCanSeeTarget(true);
		GetWorldTimerManager().ClearTimer(GracePeriodTimer);
		GetWorldTimerManager().ClearTimer(AlertTimer);
		SetIsInGracePeriod(false);

		// --- THE BYPASS FOR REGULAR ENEMIES ---
		// Bosses that are already fighting this target must NOT restart the tree: a restart interrupts
		// the jetpack hop (takeoff/volley/glide/land) mid-air and sends the boss back to the Minigun Sweep.
		const bool bBossAlreadyEngaged = Cast<ABossBase>(GetPawn()) != nullptr
			&& GetAIState() == EAIState::Engage
			&& Blackboard != nullptr
			&& Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor)) == Actor;
		
		if (!ShouldUseStealthMeter())
		{
			SetAlertPhase(EAlertPhase::Alert);
			SetTargetActor(Actor);
			SetAIState(EAIState::Engage);
			
			if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()); !bBossAlreadyEngaged)
			{
				BTComp->RestartTree();
			}
			return; // Done. They are shooting immediately.
		}
		
		AActor* CurrentTarget = Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor));

		// If we are ALREADY fully alerted (or engaged from taking damage), lock on so we don't stare!
		if (AlertPhase == EAlertPhase::Alert || GetAIState() == EAIState::Engage)
		{
			if (!CurrentTarget || CurrentTarget != Actor)
			{
				SetTargetActor(Actor);
				SetAIState(EAIState::Engage);
				if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
				{
					BTComp->RestartTree();
				}
			}
		}
		else
		{
			// We are NOT fully alerted yet. Feed the stealth detection meter!
			// Do NOT set TargetActor or Engage here. UpdateDetectionMeter() will handle it.
			SuspectedActor = Actor;
			InvestigateLocation = Actor->GetActorLocation();
		}
	}
	else // lost sight
	{
		bIsVisuallySensingTarget = false;
		LogAIStatus(TEXT("SightLost"));
		SetLastKnownLocation(Actor->GetActorLocation());
		SetCanSeeTarget(false);
		bHasLOSToSuspected = false;

		if (GetAIState() == EAIState::Engage)
		{
			StartGracePeriod();
		}
		// For enemies not yet engaged: meter decays naturally because bHasLOSToSuspected is false.
	}
}

void AEnemyAIController::HandleHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!OwnedBaseChar || !Actor) return;

	// --- Ignore sounds while in stasis ---
	if (IsInStasis()) return;
	
	// KILL THE PERCEPTION ECHO
	// We MUST ignore the "forget" signal, or the AI counts every noise twice!
	if (!Stimulus.WasSuccessfullySensed()) return;

	// === GRENADE PANIC LOGIC ===
	if (Stimulus.Tag == FName("GrenadeBounce"))
	{
		// Check if a wall is already protecting us
		FHitResult Hit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(GetPawn()); // Ignore our own body
		QueryParams.AddIgnoredActor(Actor); // Ignore the grenade

		// Trace a line from the grenade to the AI's chest
		bool bHitWall = GetWorld()->LineTraceSingleByChannel(
			Hit,
			Stimulus.StimulusLocation,
			GetPawn()->GetActorLocation(),
			ECC_Visibility,
			QueryParams
		);

		// If the line hit a wall, we are completely safe from the blast. Ignore the panic!
		if (bHitWall)
		{
			UE_LOG(LogTemp, Warning, TEXT("AI: Grenade blocked by wall, holding position."));
			return;
		}

		if (Blackboard)
		{
			// 1. LOAD THE PAYLOAD FIRST: Silently save the grenade's location into memory.
			Blackboard->SetValueAsVector(FName("DangerLocation"), Stimulus.StimulusLocation);

			// 2. PULL THE TRIGGER: Now that the data is safe, wake up the Behavior Tree!
			Blackboard->SetValueAsBool(FName("bInDanger"), true);
		}

		// THE INTERRUPT
		// Forcefully restart the tree! This snaps them out of shooting tasks instantly.
		if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
		{
			BTComp->RestartTree();
		}

		if (GetAIState() == EAIState::Patrol)
		{
			SetAIState(EAIState::Alert);
		}

		return;
	}
	// === DISTRACTION LOGIC (Coin Toss / Rock) ===
	if (Stimulus.Tag == FName("Distraction"))
	{
		LogAIStatus(TEXT("Heard Distraction"));

		// Ignore coins if we are already in a gunfight or dead!
		if (GetAIState() == EAIState::Engage || GetAIState() == EAIState::Dead) return;

		// THE SPAM FILTER
		CurrentDistractionCount++;
		if (CurrentDistractionCount > MaxDistractionsAllowed)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("AI: STOP THROWING THINGS! WE ARE UNDER ATTACK!"));

			// The AI figured it out! Pinpoint the person who threw the coin (Actor).
			SuspectedActor = Actor; 
			InvestigateLocation = Actor->GetActorLocation();

			// Instantly force full detection! The next tick will trigger the gunfight.
			DetectionMeter = 1.f;
			return;
		}

		// 1. Force the meter up so the UI turns yellow (!)
		DetectionMeter = FMath::Max(DetectionMeter, SuspiciousThreshold);
		SetAlertPhase(EAlertPhase::Suspicious);

		// 2. Tell the brain exactly where the NEW coin landed
		SetLastKnownLocation(Stimulus.StimulusLocation);

		// 3. Put them into investigation mode
		SetAIState(EAIState::Alert);

		// 4.THE PIVOT
		// ALWAYS restart the Alert Timer and the Behavior Tree so they instantly 
		// abort whatever they were doing and snap their attention to the new noise!
		StartAlertTimer();
		if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
		{
			BTComp->RestartTree();
		}
		
		return; 
	}
	
	// Enemies ignore other enemy noises (gunfire, footsteps, etc.)
	if (Cast<AEnemyBase>(Actor)) return;

	// Enemies being reset by a checkpoint restore don't react
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(OwnedBaseChar))
	{
		if (Enemy->bIsResetting) return;
		float DistFromPatrol = FVector::Dist(Enemy->GetActorLocation(), Enemy->PatrolOrigin);

		if (DistFromPatrol > Enemy->MaxChaseDistance) return;

		float SoundDistFromPatrol = FVector::Dist(Stimulus.StimulusLocation, Enemy->PatrolOrigin);
		if (SoundDistFromPatrol > Enemy->MaxChaseDistance) return;
	}

	LogAIStatus(TEXT("HearingTriggered"));

	if (GetAIState() == EAIState::Engage || GetAIState() == EAIState::Dead) return;

	if (AlertPhase != EAlertPhase::Alert)
	{
		SuspectedActor = Actor;
		InvestigateLocation = Stimulus.StimulusLocation;

		// Gunshots jump to Caution immediately; footsteps just to Suspicious
		float NoiseBoost = (Stimulus.Tag == FName("Gunshot")) ? 0.70f : 0.25f;

		DetectionMeter = FMath::Min(0.95f, DetectionMeter + NoiseBoost);
	}

	if (Stimulus.WasSuccessfullySensed())
	{
		SetHeardSound(true, Stimulus.StimulusLocation);
		SetLastKnownLocation(Stimulus.StimulusLocation);

		// Transition from Patrol to Alert so the Behavior Tree reacts!
		if (GetAIState() != EAIState::Alert)
		{
			SetAIState(EAIState::Alert);

			// --- Only restart the tree ONCE when first transitioning from Patrol! ---
			if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
			{
				BTComp->RestartTree();
			}
		}
		
		// Always refresh the timer so they don't give up while you are still shooting
		StartAlertTimer();
	}
}

void AEnemyAIController::HandleDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!OwnedBaseChar || !Actor) return;

	// --- Ignore bullet damage perception while in stasis ---
	if (IsInStasis()) return;
	
	// If we are retreating, ignore EVERYTHING.
	if (AEnemyBase* Enemy = Cast<AEnemyBase>(OwnedBaseChar))
	{
		if (Enemy->bIsResetting) return;
	}

	//Do not process damage if the bullet killed us
	if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
	{
		if (!AIChar->IsAlive()) return;
	}
	
	// Enemies only react to damage from the player (they ignore enemy crossfire)
	if (!Cast<APlayerCharacter>(Actor)) return;

	LogAIStatus(TEXT("DamageReceived"));

	if (GetAIState() == EAIState::Dead) return;

	if (Stimulus.WasSuccessfullySensed() && Actor)
	{
		// Dynamically Boost Sight to see the attacker
		if (UAIPerceptionComponent* PerceptionComp = GetPerceptionComponent())
		{
			FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
			if (UAISenseConfig_Sight* CurrentSightConfig = Cast<UAISenseConfig_Sight>(
				PerceptionComp->GetSenseConfig(SightID)))
			{
				float DistanceToAttacker = FVector::Dist(GetPawn()->GetActorLocation(), Actor->GetActorLocation());
				if (CurrentSightConfig->SightRadius < DistanceToAttacker)
				{
					CurrentSightConfig->SightRadius = DistanceToAttacker + 500.f;
					CurrentSightConfig->LoseSightRadius = CurrentSightConfig->SightRadius + 500.f;
					PerceptionComp->ConfigureSense(*CurrentSightConfig);
					UAIPerceptionSystem::GetCurrent(GetWorld())->UpdateListener(*PerceptionComp);
				}
			}
		}

		// Bosses that are already fighting this target must NOT restart the tree: a restart interrupts
		// the jetpack hop (takeoff/volley/glide/land) mid-air and sends the boss back to the Minigun Sweep.
		const bool bBossAlreadyEngaged = Cast<ABossBase>(GetPawn()) != nullptr
			&& GetAIState() == EAIState::Engage
			&& Blackboard != nullptr
			&& Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor)) == Actor;
		
		if (!ShouldUseStealthMeter())
		{
			// Instant Aggro
			SetAlertPhase(EAlertPhase::Alert);
			SetTargetActor(Actor);
			SetAIState(EAIState::Engage);
			SetFocus(Actor);
			
			if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()); !bBossAlreadyEngaged)
			{
				BTComp->RestartTree();
			}
			return;
		}
		
// Feed the detection meter — a large boost but still requires LOS to fully engage.
		// The enemy knows they were shot FROM somewhere; they still need to LOOK at the player.
		SuspectedActor = Actor;
		InvestigateLocation = Actor->GetActorLocation();
		DetectionMeter = FMath::Min(0.95f, DetectionMeter + 0.75f);
		SetLastKnownLocation(Actor->GetActorLocation());
		SetFocus(Actor);

		// Bump the internal phase so the meter threshold transitions are consistent
		if (DetectionMeter >= CautionThreshold && AlertPhase < EAlertPhase::Caution)
			SetAlertPhase(EAlertPhase::Caution);

		// Switch to investigation mode — NOT full Engage yet
		if (GetAIState() != EAIState::Engage)
		{
			SetAIState(EAIState::Alert);
			StartAlertTimer();
			StopMovement(); // brief surprise freeze

			if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
				BTComp->RestartTree();
		}

		// Update the stealth HUD with the new meter value
		if (!bIsStealthUIHidden)
		{
			if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
				Sub->RegisterDetection(this, DetectionMeter, static_cast<uint8>(AlertPhase));
		}

		// Global alert/backup radio logic (unchanged timing intent)
		if (AlertPhase == EAlertPhase::Alert)
		{
			if (!bHasCalledForBackup && GetWorldTimerManager().IsTimerActive(GlobalAlertDelayTimer))
			{
				GetWorldTimerManager().ClearTimer(GlobalAlertDelayTimer);
				BroadcastGlobalAlert();
			}
		}
		else if (!bHasCalledForBackup && !GetWorldTimerManager().IsTimerActive(GlobalAlertDelayTimer))
		{
			GetWorldTimerManager().SetTimer(GlobalAlertDelayTimer, this,
			                                &AEnemyAIController::BroadcastGlobalAlert, TimeToCallForBackup, false);
		}
	}
}

#pragma endregion Perception

#pragma region GracePeriod

void AEnemyAIController::StartGracePeriod()
{
	LogAIStatus(TEXT("GraceStart"));

	UE_LOG(LogAI, Warning,
	       TEXT("[Grace] Starting grace period: %.1fs | AI: %s"),
	       GracePeriodDuration,
	       *GetName());

	SetIsInGracePeriod(true);
	GetWorldTimerManager().SetTimer(
		GracePeriodTimer,
		this,
		&AEnemyAIController::OnGracePeriodExpired,
		GracePeriodDuration,
		false
	);
}

void AEnemyAIController::OnGracePeriodExpired()
{
	LogAIStatus(TEXT("GraceExpired"));

	// THE FIX: If the AI is intentionally in cover, ignore the grace period!
	// We don't want them to "forget" the player just because they hid behind a wall.
	if (Blackboard && Blackboard->GetValueAsBool(FName("IsInCover")))
	{
		// Give them a fresh grace period for when they finally step out
		StartGracePeriod(); 
		return;
	}

	UE_LOG(LogAI, Warning,
		   TEXT("[Grace] Grace period expired | AI: %s"),
		   *GetName());

	SetIsInGracePeriod(false);
	ClearTarget();

	// Lost the player: go and investigate
	SetAIState(EAIState::Alert);
	StartAlertTimer();
}
#pragma endregion GracePeriod

#pragma region BlackboardHelpers

void AEnemyAIController::SetTargetActor(AActor* Target)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsObject(BB_TargetActor, Target);

	SetFocus(Target);
}

void AEnemyAIController::ClearTarget()
{
	if (!Blackboard) return;
	Blackboard->ClearValue(BB_TargetActor);
	SetCanSeeTarget(false);

	ClearFocus(EAIFocusPriority::Gameplay);
}

void AEnemyAIController::SetLastKnownLocation(
	const FVector& Location)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsVector(BB_LastKnownLocation, Location);
}

void AEnemyAIController::SetAIState(EAIState NewState)
{
	if (!Blackboard) return;
	EAIState OldState = GetAIState();

	if (OldState != NewState)
	{
		if (NewState == EAIState::Patrol)
		{
			GetWorldTimerManager().ClearTimer(GracePeriodTimer);
			GetWorldTimerManager().ClearTimer(AlertTimer);
			SetIsInGracePeriod(false);

			// ONLY enemies have waypoint paths!
			if (AEnemyBase* Enemy = Cast<AEnemyBase>(OwnedBaseChar))
			{
				if (Enemy->WaypointPath && OldState != EAIState::Patrol)
				{
					int32 ClosestIndex = 0;
					float MinDist = MAX_flt;

					for (int32 i = 0; i < Enemy->WaypointPath->Num(); ++i)
					{
						float Dist = FVector::Dist(Enemy->GetActorLocation(),
						                           Enemy->WaypointPath->GetLocationAtIndex(i));

						if (Dist < MinDist)
						{
							MinDist = Dist;
							ClosestIndex = i;
						}
					}
					SetWaypointIndex(ClosestIndex);
				}
			}
		}
		Blackboard->SetValueAsInt(BB_AIState, static_cast<int32>(NewState));

		// Walk / jog / sprint follows the AI state (set per enemy on AICharacterBase)
		if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
		{
			AIChar->ApplyMovementSpeedForState(NewState);
		}
	}
}

void AEnemyAIController::SetCanSeeTarget(bool bCanSee)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bCanSeeTarget, bCanSee);
}

void AEnemyAIController::SetHeardSound(
	bool bHeard, const FVector& Location)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bHeardSound, bHeard);
	if (bHeard)
	{
		Blackboard->SetValueAsVector(BB_SoundLocation, Location);
	}
}

void AEnemyAIController::SetWaypointIndex(int32 Index)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsInt(BB_WaypointIndex, Index);
}

void AEnemyAIController::SetPingPongForward(bool bForward)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bPingPongForward, bForward);
}

void AEnemyAIController::SetIsInGracePeriod(bool bInGrace)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsBool(BB_bIsInGracePeriod, bInGrace);
}

void AEnemyAIController::SetPreferredRange(float Range)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsFloat(BB_PreferredRange, Range);
}

EAIState AEnemyAIController::GetAIState() const
{
	if (!Blackboard) return EAIState::Patrol;
	return static_cast<EAIState>(
		Blackboard->GetValueAsInt(BB_AIState));
}

void AEnemyAIController::OnEnemyDied()
{
	LogAIStatus(TEXT("EnemyDied"));

	ClearStealthUI();

	// Freeze control rotation where it is so it never snaps and triggers a turn!
	bManualControlRotation = true;
	ClearTarget();

	SetAIState(EAIState::Dead);
	GetWorldTimerManager().ClearTimer(GracePeriodTimer);
	GetWorldTimerManager().ClearTimer(GlobalAlertDelayTimer);
	GetWorldTimerManager().ClearTimer(AlertTimer);
	GetWorldTimerManager().ClearTimer(TargetSweepTimer);

	if (AIPerception)
	{
		AIPerception->Deactivate();
	}

	if (UBehaviorTreeComponent* BTComp = FindComponentByClass<UBehaviorTreeComponent>())
	{
		BTComp->StopTree();
	}
	StopMovement();
}

#pragma endregion BlackboardHelpers

void AEnemyAIController::SetEnemyType(int32 Type)
{
	if (!Blackboard) return;
	Blackboard->SetValueAsInt(BB_EnemyType, Type);
}

void AEnemyAIController::LogAIStatus(const FString& Context) const
{
	if (!OwnedBaseChar) return;

	// Get player
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PC ?
		PC->GetPawn() : nullptr;

	FVector AILocation = OwnedBaseChar->GetActorLocation();
	FRotator AIRotation = OwnedBaseChar->GetActorRotation();

	FString PlayerInfo = TEXT("No Player");
	float DistanceToPlayer = -1.f;

	if (PlayerPawn)
	{
		FVector PlayerLocation = PlayerPawn->GetActorLocation();
		FRotator PlayerRotation = PlayerPawn->GetActorRotation();
		DistanceToPlayer = FVector::Dist(AILocation, PlayerLocation);

		PlayerInfo = FString::Printf(
			TEXT("Player Loc: X=%.1f Y=%.1f Z=%.1f | "
				"Player Rot: P=%.1f Y=%.1f | "
				"Distance: %.1fcm"),
			PlayerLocation.X, PlayerLocation.Y, PlayerLocation.Z,
			PlayerRotation.Pitch, PlayerRotation.Yaw,
			DistanceToPlayer);
	}

	FString AIInfo = FString::Printf(
		TEXT("AI Loc: X=%.1f Y=%.1f Z=%.1f | "
			"AI Rot: P=%.1f Y=%.1f | "
			"State: %d | "
			"CanSee: %s | "
			"InGrace: %s"),
		AILocation.X, AILocation.Y, AILocation.Z,
		AIRotation.Pitch, AIRotation.Yaw,
		static_cast<int32>(GetAIState()),
		Blackboard && Blackboard->GetValueAsBool(BB_bCanSeeTarget)
			? TEXT("YES")
			: TEXT("NO"),
		Blackboard && Blackboard->GetValueAsBool(BB_bIsInGracePeriod)
			? TEXT("YES")
			: TEXT("NO"));

	UE_LOG(LogAI, Warning,
	       TEXT("[Status | %s]\n  %s\n  %s"),
	       *Context, *AIInfo, *PlayerInfo);
}

void AEnemyAIController::StartAlertTimer()
{
	GetWorldTimerManager().ClearTimer(AlertTimer);

	GetWorldTimerManager().SetTimer(
		AlertTimer,
		this,
		&AEnemyAIController::OnAlertExpired,
		10.f,
		false
	);
}

void AEnemyAIController::OnAlertExpired()
{
	if (GetAIState() == EAIState::Alert)
	{
		SetAIState(EAIState::Patrol);
		AlertPhase     = EAlertPhase::Unaware;
		DetectionMeter = 0.f;
		SuspectedActor = nullptr;
        
		bHasCalledForBackup = false;
       
		// Reset the spam counter because they gave up the search
		CurrentDistractionCount = 0;

		// Force explicit UI clear!
		ClearStealthUI();
		// Allow the UI to be triggered again later
		bIsStealthUIHidden = false; 
	}
}

void AEnemyAIController::UpdateDetectionMeter(float DeltaTime)
{
    // Stop dead bodies from running stealth math and keeping the UI alive
	if (GetAIState() == EAIState::Dead || IsInStasis() || !ShouldUseStealthMeter()) return;
	
	if (AlertPhase == EAlertPhase::Alert) return;

    // 1. UPDATE THE METER MATH
    if (SuspectedActor && bIsVisuallySensingTarget)
    {
       DetectionMeter = FMath::Min(1.f, DetectionMeter + CalculateDetectionRate(SuspectedActor) * DeltaTime);
    }
    // Only decay the meter if they are NOT actively investigating
    else if (GetAIState() != EAIState::Alert)
    {
       DetectionMeter = FMath::Max(0.f, DetectionMeter - DetectionDecayRate * DeltaTime);

       if (DetectionMeter <= 0.f && AlertPhase != EAlertPhase::Unaware)
       {
          SetAlertPhase(EAlertPhase::Unaware);
          SuspectedActor = nullptr;
          
          // Use our new explicit clear function!
          ClearStealthUI();
          // Allow it to be drawn again next time we see them
          bIsStealthUIHidden = false; 
       }
       // Handle UI turning back to yellow if it drops from orange
       else if (DetectionMeter < CautionThreshold && DetectionMeter >= SuspiciousThreshold && AlertPhase == EAlertPhase::Caution)
       {
          SetAlertPhase(EAlertPhase::Suspicious);
       }
    }

    // 2. CHECK PHASE TRANSITIONS (Going Up)
    if (DetectionMeter >= 1.f)
    {
       SetAlertPhase(EAlertPhase::Alert);
       SetTargetActor(SuspectedActor);
        
       // "Surprise" Freeze 
       StopMovement(); 
       SetAIState(EAIState::Engage);
        
       // Wait 1 second before hiding the fully-red UI
       GetWorldTimerManager().SetTimer(ClearUITimer, this, &AEnemyAIController::ClearStealthUI, StealthUIClearTime, false);

       if (!bHasCalledForBackup && !GetWorldTimerManager().IsTimerActive(GlobalAlertDelayTimer))
       {
          GetWorldTimerManager().SetTimer(GlobalAlertDelayTimer, this, &AEnemyAIController::BroadcastGlobalAlert, TimeToCallForBackup, false);
       }

       if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
          BTComp->RestartTree();

    }
    else if (DetectionMeter >= CautionThreshold && AlertPhase < EAlertPhase::Caution)
    {
       SetAlertPhase(EAlertPhase::Caution);

    }
    else if (DetectionMeter >= SuspiciousThreshold && AlertPhase < EAlertPhase::Suspicious)
    {
       SetAlertPhase(EAlertPhase::Suspicious);

       // BREAK PATROL AND INVESTIGATE
       if (SuspectedActor && GetAIState() != EAIState::Engage)
       {
          SetLastKnownLocation(SuspectedActor->GetActorLocation());
          SetAIState(EAIState::Alert);
          StartAlertTimer();

          if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
          {
             BTComp->RestartTree();
          }
       }
    }

	// 3. HYBRID UI UPDATE
	if (!bIsStealthUIHidden)
	{
		bool bIsOnScreen = false;
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			if (OwnedBaseChar)
			{
				FVector2D ScreenPos;
				// ProjectWorldLocationToScreen returns true if it is in front of the camera
				if (PC->ProjectWorldLocationToScreen(OwnedBaseChar->GetActorLocation(), ScreenPos, true))
				{
					int32 ViewX, ViewY;
					PC->GetViewportSize(ViewX, ViewY);
					// Check if it is actually within the visible borders of the screen
					if (ScreenPos.X >= 0.f && ScreenPos.X <= ViewX && ScreenPos.Y >= 0.f && ScreenPos.Y <= ViewY)
					{
						bIsOnScreen = true;
					}
				}
			}
		}

		UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>();

		if (bIsOnScreen)
		{
			// ON-SCREEN: Hide the HUD ring arrow, show the floating head widget
			if (Sub) Sub->UnregisterDetection(this);

			if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
			{
				if (AIChar->StealthIndicatorWidget && DetectionMeter > 0.f)
				{
					if (UDirectionalIndicatorWidget* Indicator = Cast<UDirectionalIndicatorWidget>(AIChar->StealthIndicatorWidget->GetUserWidgetObject()))
					{
						Indicator->UpdateVisuals(DetectionMeter, static_cast<uint8>(AlertPhase));
					}
				}
			}
		}
		else
		{
			// OFF-SCREEN: Hide the floating head widget, show the HUD ring arrow
			if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
			{
				if (AIChar->StealthIndicatorWidget)
				{
					if (UDirectionalIndicatorWidget* Indicator = Cast<UDirectionalIndicatorWidget>(AIChar->StealthIndicatorWidget->GetUserWidgetObject()))
					{
						Indicator->UpdateVisuals(0.f, 0); // Hide it
					}
				}
			}

			if (Sub && DetectionMeter > 0.f)
			{
				Sub->RegisterDetection(this, DetectionMeter, static_cast<uint8>(AlertPhase));
			}
		}
	}
}

float AEnemyAIController::CalculateDetectionRate(AActor* TargetActor) const
{
    APawn* OwnerPawn = GetPawn();
    if (!OwnerPawn || !TargetActor) return 0.f;

    FVector ToTarget = (TargetActor->GetActorLocation() - OwnerPawn->GetActorLocation()).GetSafeNormal();
    float Dot = FVector::DotProduct(OwnerPawn->GetActorForwardVector(), ToTarget);

    // --- VISION CONE GATE ---
    // Anything outside the peripheral vision angle contributes zero detection.
    float HalfAngleDeg = SightConfig ? SightConfig->PeripheralVisionAngleDegrees : 60.f;
    float ConeDot = FMath::Cos(FMath::DegreesToRadians(HalfAngleDeg));
    if (Dot < ConeDot) return 0.f;

    // --- LINE OF SIGHT GATE ---
    // If geometry sits between the AI's eyes and the target, no detection.
    FHitResult Hit;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerPawn);
    QueryParams.AddIgnoredActor(TargetActor);
    const FVector EyeOffset(0.f, 0.f, 64.f);

    bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit,
        OwnerPawn->GetActorLocation() + EyeOffset,
        TargetActor->GetActorLocation() + EyeOffset,
        ECC_Visibility,
        QueryParams);

    if (bBlocked) return 0.f;

    float Rate = BaseDetectionRate;

    // Distance falloff — full rate up close, near-zero at the sight radius boundary
    float Dist = FVector::Dist(OwnerPawn->GetActorLocation(), TargetActor->GetActorLocation());
    float SightRadius = SightConfig ? SightConfig->SightRadius : 2000.f;
    Rate *= FMath::Clamp(1.f - (Dist / SightRadius), 0.05f, 1.f);

    // Angle falloff — 30% at the cone edge, 100% dead ahead (no longer bleeds outside cone)
    float AngleFactor = FMath::Clamp((Dot - ConeDot) / (1.f - ConeDot), 0.f, 1.f);
    Rate *= FMath::Lerp(0.3f, 1.f, AngleFactor);

    // Player stealth exposure (crouching, sprinting, etc.)
    if (UStealthComponent* SC = TargetActor->FindComponentByClass<UStealthComponent>())
        Rate *= SC->GetExposureLevel();

    return Rate;
}

void AEnemyAIController::SetAlertPhase(EAlertPhase NewPhase)
{
	if (AlertPhase == NewPhase) return;
	AlertPhase = NewPhase;

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsEnum(FName("AlertPhase"), (uint8)AlertPhase);
		if (SuspectedActor && NewPhase >= EAlertPhase::Suspicious)
			BB->SetValueAsVector(FName("InvestigateLocation"), SuspectedActor->GetActorLocation());
	}
}

void AEnemyAIController::OnGlobalAlert(AActor* AlertInstigator, float AlertRadius)
{
	// --- Ignore radio calls while in stasis ---
	if (IsInStasis()) return;
	
	// If we are ALREADY engaging the player, or we are dead, ignore the radio call
	if (GetAIState() == EAIState::Engage || GetAIState() == EAIState::Dead) return;

	bHasCalledForBackup = true;
    
	APawn* OwnerPawn = GetPawn();
	if (!OwnerPawn || !AlertInstigator) return;

	if (AlertRadius > 0.f)
	{
		float Dist = FVector::Dist(OwnerPawn->GetActorLocation(), AlertInstigator->GetActorLocation());
		if (Dist > AlertRadius) return;
	}
	
	// 1. We know SOMEONE is there, but we DO NOT have visual confirmation yet!
	SuspectedActor = AlertInstigator;
	
	// 2. Bring them to High Suspicion so they are actively looking, but DO NOT push to 1.0!
	DetectionMeter = FMath::Max(DetectionMeter, 0.85f);
	SetAlertPhase(EAlertPhase::Caution);

	// 3. Tell the Behavior Tree where the radio call came from
	SetLastKnownLocation(AlertInstigator->GetActorLocation());

	// 4. Put them in Search/Alert mode, NOT Engage mode
	SetAIState(EAIState::Alert);
	StartAlertTimer();

	// 5. Wake up the Behavior Tree so they drop their patrol and run to investigate
	if (UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(GetBrainComponent()))
	{
		BTComp->RestartTree();
	}

	// --- Hide the UI after 1 second so the "!" doesn't linger on screen forever! ---
	GetWorldTimerManager().SetTimer(ClearUITimer, this, &AEnemyAIController::ClearStealthUI, StealthUIClearTime, false);
}

void AEnemyAIController::ClearStealthUI()
{
	bIsStealthUIHidden = true;

	// 1. Clear Screen-Space (HUD)
	if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
	{
		Sub->UnregisterDetection(this); 
	}

	// 2. Clear World-Space (Above Head)
	if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
	{
		if (AIChar->StealthIndicatorWidget)
		{
			if (UDirectionalIndicatorWidget* Indicator = Cast<UDirectionalIndicatorWidget>(AIChar->StealthIndicatorWidget->GetUserWidgetObject()))
			{
				Indicator->UpdateVisuals(0.f, 0);
			}
		}
	}

	// 3. FORCE the update to all human players explicitly right now.
	UWorld* World = GetWorld();
	if (World)
	{
		APawn* UIBody = OwnedBaseChar ? OwnedBaseChar : CachedUIBody;
		if (UIBody)
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				if (ACorePlayerController* PC = Cast<ACorePlayerController>(It->Get()))
				{
					PC->Client_UpdateDetectionHUD(UIBody, 0.f, 0);
				}
			}
		}
	}
}


void AEnemyAIController::BroadcastGlobalAlert()
{
	if (GetAIState() == EAIState::Dead || !OwnedBaseChar) return;

	// Directly check if the physical body is dead
	if (AAICharacterBase* AIChar = Cast<AAICharacterBase>(OwnedBaseChar))
	{
		if (!AIChar->IsAlive()) return; // Dead men tell no tales!
	}

	bHasCalledForBackup = true;

	if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
	{
		AActor* Target = Cast<AActor>(Blackboard->GetValueAsObject(BB_TargetActor));

		if (!Target) Target = SuspectedActor;

		if (Target)
		{
			if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("AI: CONTACT! SEND BACKUP!"));
			Sub->TriggerGlobalAlert(Target, AlertPropagationRadius);
		}
	}
}

bool AEnemyAIController::IsInStasis() const
{
	if (!Blackboard) return false;
    
	// If they are frozen, caged, in stasis, OR electrocuted, their perception is paused!
	return Blackboard->GetValueAsBool(BB_bIsFrozen) || 
		   Blackboard->GetValueAsBool(BB_bIsCaged) || 
		   Blackboard->GetValueAsBool(BB_bIsInStasis) ||
		   Blackboard->GetValueAsBool(BB_bIsElectrocuted);
}


void AEnemyAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	if (bManualControlRotation) return;
	Super::UpdateControlRotation(DeltaTime, bUpdatePawn);
}