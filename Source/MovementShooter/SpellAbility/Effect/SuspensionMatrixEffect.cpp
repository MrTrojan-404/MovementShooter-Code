
#include "MovementShooter/SpellAbility/Effect/SuspensionMatrixEffect.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "MovementShooter/AI/Character/AICharacterBase.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "Kismet/GameplayStatics.h"

ASuspensionMatrixEffect::ASuspensionMatrixEffect()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	ZoneSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneSphere"));
	RootComponent = ZoneSphere;
	ZoneSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	DomeMesh->SetupAttachment(RootComponent);
	
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DomeMesh->SetCollisionProfileName(TEXT("NoCollision"));
	DomeMesh->SetGenerateOverlapEvents(false);
	DomeMesh->CanCharacterStepUpOn = ECB_No;
    
	ActiveZoneVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ActiveZoneVFX"));
	ActiveZoneVFX->SetupAttachment(RootComponent);
	ActiveZoneVFX->bAutoActivate = false; // We activate this manually after expansion!

	ActiveLoopSound = CreateDefaultSubobject<UAudioComponent>(TEXT("ActiveLoopSound"));
	ActiveLoopSound->SetupAttachment(RootComponent);
	ActiveLoopSound->bAutoActivate = false;
}

void ASuspensionMatrixEffect::Initialize(ABaseCharacter* InCaster)
{
	Caster = InCaster;
}

void ASuspensionMatrixEffect::BeginPlay()
{
	Super::BeginPlay();

	ZoneSphere->SetSphereRadius(ZoneRadius);

	// 1. Start the Dome at near-zero scale
	DomeMesh->SetWorldScale3D(FVector(0.01f));

	// 2. Play the initial expansion sound
	if (DeploySound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeploySound, GetActorLocation());
	}

	if (!HasAuthority()) return;

	GetWorldTimerManager().SetTimer(ExpirationTimerHandle, this, &ASuspensionMatrixEffect::CollapseZone, ZoneDuration, false);
}

void ASuspensionMatrixEffect::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// ==========================================
	// 1. EXPAND THE DOME (Runs on ALL Clients)
	// ==========================================
	FVector CurrentScale = DomeMesh->GetComponentScale();
    
	// Standard Unreal Spheres are 100 units across (50 radius). 
	// We divide our desired radius by 50 to get the correct multiplier.
	float TargetScaleFloat = ZoneRadius / 50.f;
	FVector TargetScale(TargetScaleFloat);

	if (!CurrentScale.Equals(TargetScale, 0.05f))
	{
		DomeMesh->SetWorldScale3D(FMath::VInterpTo(CurrentScale, TargetScale, DeltaTime, DomeExpansionSpeed));
        
		// Did we just finish expanding this frame?
		if (DomeMesh->GetComponentScale().Equals(TargetScale, 0.05f))
		{
			DomeMesh->SetWorldScale3D(TargetScale);
            
			// Activate the loops now that the dome is fully formed!
			ActiveZoneVFX->Activate();
			ActiveLoopSound->Play();
		}
	}

	// ==========================================
	// 2. APPLY ZERO-G (Server Only)
	// ==========================================
	if (HasAuthority())
	{
		MaintainZeroG();
	}
}

void ASuspensionMatrixEffect::MaintainZeroG()
{
	FVector Epicenter = GetActorLocation();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	
	// 1. SUSPEND CHARACTERS
	TArray<FOverlapResult> PawnOverlaps;
	GetWorld()->OverlapMultiByObjectType(PawnOverlaps, Epicenter, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(ZoneRadius), Params);

	for (const FOverlapResult& Overlap : PawnOverlaps)
	{
		ABaseCharacter* Target = Cast<ABaseCharacter>(Overlap.GetActor());
		if (!Target || Target->IsPlayerAlly()) continue;

		bool bIsHeavy = false;
		if (AAICharacterBase* AITarget = Cast<AAICharacterBase>(Target))
		{
			bIsHeavy = AITarget->bIsHeavyEnemy;
		}

		if (bIsHeavy)
		{
			MaintainHeavyHover(Target);
		}
		else if (!SuspendedCharacters.Contains(Target))
		{
			Target->BeginRagdoll(FVector(0.f, 0.f, 1.f), 300.f);
			Multicast_SuspendTarget(Target, true);
		}
	}

	// 2. SUSPEND PHYSICS PROPS 
	TArray<FOverlapResult> PhysicsOverlaps;
	FCollisionObjectQueryParams PhysicsObjectParams;
	PhysicsObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	PhysicsObjectParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	GetWorld()->OverlapMultiByObjectType(PhysicsOverlaps, Epicenter, FQuat::Identity, PhysicsObjectParams, FCollisionShape::MakeSphere(ZoneRadius), Params);

	for (const FOverlapResult& Overlap : PhysicsOverlaps)
	{
		if (UPrimitiveComponent* PrimComp = Overlap.GetComponent())
		{
			if (PrimComp->IsSimulatingPhysics() && !SuspendedPhysicsComponents.Contains(PrimComp))
			{
				AActor* HitActor = Overlap.GetActor();
				if (HitActor) Multicast_SuspendTarget(HitActor, false);
			}
		}
	}
}

void ASuspensionMatrixEffect::MaintainHeavyHover(ABaseCharacter* Target)
{
	if (!Target) return;

	if (!SuspendedHeavies.Contains(Target))
	{
		SuspendedHeavies.Add(Target);
		Multicast_BeginHeavyHover(Target);

		if (AEnemyAIController* AICon = Cast<AEnemyAIController>(Target->GetController()))
		{
			if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
			{
				BB->SetValueAsBool(AEnemyAIController::BB_bIsSuspended, true);
			}
		}
	}

	if (UCharacterMovementComponent* TargetMove = Target->GetCharacterMovement())
	{
		if (TargetMove->MovementMode != MOVE_Flying)
		{
			TargetMove->SetMovementMode(MOVE_Flying);
		}
		TargetMove->Velocity = FVector(FMath::RandRange(-20.f, 20.f), FMath::RandRange(-20.f, 20.f), UpwardDriftSpeed);
	}
}

void ASuspensionMatrixEffect::Multicast_BeginHeavyHover_Implementation(AActor* TargetActor)
{
	if (ABaseCharacter* TargetChar = Cast<ABaseCharacter>(TargetActor))
	{
		if (TargetChar->GetCapsuleComponent())
		{
			TargetChar->GetCapsuleComponent()->SetEnableGravity(false);
		}
	}
}

void ASuspensionMatrixEffect::Multicast_SuspendTarget_Implementation(AActor* TargetActor, bool bIsCharacter)
{
	if (!TargetActor) return;
	
	if (bIsCharacter)
	{
		if (ABaseCharacter* CharTarget = Cast<ABaseCharacter>(TargetActor))
		{
			// Add to local array so the Client can track them!
			SuspendedCharacters.Add(CharTarget); 
			
			if (USkeletalMeshComponent* Mesh = CharTarget->FindComponentByClass<USkeletalMeshComponent>())
			{
				Mesh->SetEnableGravity(false);
				FVector Nudge = FVector(FMath::RandRange(-50.f, 50.f), FMath::RandRange(-50.f, 50.f), UpwardDriftSpeed * 3.f);
				Mesh->AddImpulse(Nudge, NAME_None, true);
			}
		}
	}
	else
	{
		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(TargetActor->GetRootComponent()))
		{
			// Add to local array so the Client can track them!
			SuspendedPhysicsComponents.Add(PrimComp);
			
			PrimComp->SetEnableGravity(false);
			FVector Nudge = FVector(FMath::RandRange(-20.f, 20.f), FMath::RandRange(-20.f, 20.f), UpwardDriftSpeed * 2.f);
			PrimComp->AddImpulse(Nudge, NAME_None, true);
		}
	}
}

void ASuspensionMatrixEffect::CollapseZone()
{
	Destroy();
}

void ASuspensionMatrixEffect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// 1. Play Collapse FX & SFX
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (CollapseVFX)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), CollapseVFX, GetActorLocation());
		}
		if (CollapseSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, CollapseSound, GetActorLocation());
		}
	}

	// 2. Safely deactivate the loops
	ActiveZoneVFX->Deactivate();
	ActiveLoopSound->Stop();

	// 3. Restore gravity locally on their own screen.
	for (UPrimitiveComponent* PrimComp : SuspendedPhysicsComponents)
	{
		if (IsValid(PrimComp)) PrimComp->SetEnableGravity(true);
	}
	
	for (ABaseCharacter* SuspendedTarget : SuspendedCharacters)
	{
		if (IsValid(SuspendedTarget))
		{
			if (USkeletalMeshComponent* Mesh = SuspendedTarget->FindComponentByClass<USkeletalMeshComponent>())
			{
				Mesh->SetEnableGravity(true);
			}
			if (HasAuthority())
			{
				SuspendedTarget->RecoverFromRagdoll();
			}
		}
	}

	for (ABaseCharacter* Heavy : SuspendedHeavies)
	{
		if (IsValid(Heavy))
		{
			if (Heavy->GetCapsuleComponent())
			{
				Heavy->GetCapsuleComponent()->SetEnableGravity(true);
			}
			if (UCharacterMovementComponent* HeavyMove = Heavy->GetCharacterMovement())
			{
				// Hover is over: drop back to the ground (the Mover version fell on its own once the layered move ended).
				HeavyMove->SetMovementMode(MOVE_Falling);
			}

			if (HasAuthority())
			{
				if (AEnemyAIController* AICon = Cast<AEnemyAIController>(Heavy->GetController()))
				{
					if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
					{
						BB->SetValueAsBool(AEnemyAIController::BB_bIsSuspended, false);
					}
				}
			}
		}
	}
	SuspendedHeavies.Empty();
	
	SuspendedPhysicsComponents.Empty();
	SuspendedCharacters.Empty();
}

void ASuspensionMatrixEffect::Multicast_PlayCastFX_Implementation(ABaseCharacter* InCaster)
{
	if (!InCaster) return;

	if (CastVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			CastVFX, InCaster->CharacterMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true
		);
	}
}

