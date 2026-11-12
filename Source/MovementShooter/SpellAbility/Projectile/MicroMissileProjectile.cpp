#include "MovementShooter/SpellAbility/Projectile/MicroMissileProjectile.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

// Audio and VFX Includes based on your Rocket reference
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "Net/UnrealNetwork.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundCue.h"

AMicroMissileProjectile::AMicroMissileProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	RootComponent = CollisionSphere;
	CollisionSphere->SetSphereRadius(10.f);
	CollisionSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic")); 
	CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AMicroMissileProjectile::OnOverlap);
	CollisionSphere->OnComponentHit.AddDynamic(this, &AMicroMissileProjectile::OnHit);
	
	MissileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MissileMesh"));
	MissileMesh->SetupAttachment(RootComponent);
	MissileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->bRotationFollowsVelocity = true;
	
	// Start with homing OFF so we can shoot it upwards first
	ProjectileMovement->bIsHomingProjectile = false; 
	ProjectileMovement->ProjectileGravityScale = 1.0f; // Give it gravity so the upward launch arcs naturally
}

void AMicroMissileProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMicroMissileProjectile, Caster);
	DOREPLIFETIME(AMicroMissileProjectile, Target);
}



void AMicroMissileProjectile::Initialize(ABaseCharacter* InCaster, ABaseCharacter* InTarget)
{
	Caster = InCaster;
	Target = InTarget;
}

void AMicroMissileProjectile::SetStasis(bool bInStasis)
{
	// Speed is handled by the world's global time dilation; only the homing timer needs pausing.
	if (bInStasis)
	{
		GetWorldTimerManager().PauseTimer(HomingTimerHandle);
	}
	else
	{
		GetWorldTimerManager().UnPauseTimer(HomingTimerHandle);
	}
}

void AMicroMissileProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(6.f); 

	// 1. Play Launch Audio
	if (LaunchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, LaunchSound, GetActorLocation());
	}
	
	
	// If a guard is standing right next to Sera, they will hear the missiles launch!
	if (HasAuthority() && Caster)
	{
		UAISense_Hearing::ReportNoiseEvent(
			GetWorld(), 
			GetActorLocation(), 
			1.f, 
			Caster, 
			500.f, // 500 units = 5 meters
			FName("Distraction") 
		);
	}

	// 2. Attach the Niagara Trail
	if (TrailSystem)
	{
		ActiveTrail = UNiagaraFunctionLibrary::SpawnSystemAttached(
			TrailSystem, 
			RootComponent, 
			NAME_None, 
			FVector::ZeroVector, 
			FRotator(0.f, 180.f, 0.f), // FLIPPED 180 DEGREES
			EAttachLocation::KeepRelativeOffset, 
			true
		);
	}

	// 3. The Vertical Launch
	// Shoot mostly Up, but slightly Forward so it clears Sera's head safely
	FVector LaunchVelocity = (GetActorUpVector() * 1.5f) + (GetActorForwardVector() * 0.5f);
	LaunchVelocity.Normalize();
	
	ProjectileMovement->Velocity = LaunchVelocity * UpwardLaunchSpeed;

	// 4. Schedule the Dive
	GetWorldTimerManager().SetTimer(HomingTimerHandle, this, &AMicroMissileProjectile::ActivateHoming, HomingDelay, false);
}

void AMicroMissileProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Only relevant while the missile is actively homing (not during the upward launch, not after it has committed)
	if (!ProjectileMovement || !ProjectileMovement->bIsHomingProjectile) return;

	USceneComponent* HomingTarget = ProjectileMovement->HomingTargetComponent.Get();
	if (!HomingTarget)
	{
		// Target is gone (died / destroyed): don't keep steering at nothing, just fly straight
		StopHoming();
		return;
	}

	// Inside the break radius: stop tracking and commit to a straight line
	const float DistSq = FVector::DistSquared(GetActorLocation(), HomingTarget->GetComponentLocation());
	if (DistSq <= FMath::Square(HomingBreakRadius))
	{
		StopHoming();
	}
}

void AMicroMissileProjectile::StopHoming()
{
	if (!ProjectileMovement) return;

	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->HomingTargetComponent = nullptr;
	ProjectileMovement->HomingAccelerationMagnitude = 0.f;

	// Keep the current heading, at a constant dive speed. Gravity is already 0 after ActivateHoming,
	// so from here it is a perfectly straight shot until it hits something or the 6s lifespan ends.
	FVector Heading = ProjectileMovement->Velocity.GetSafeNormal();
	if (Heading.IsNearlyZero())
	{
		Heading = GetActorForwardVector();
	}
	ProjectileMovement->Velocity = Heading * DiveSpeed;
}

void AMicroMissileProjectile::ActivateHoming()
{
	// Always turn off gravity after the upward launch completes
	ProjectileMovement->ProjectileGravityScale = 0.f; 

	// DUMBFIRE DIVE TO CROSSHAIR 
	if (!IsValid(Target)) 
	{
		// Fallback default direction
		FVector DiveDir = Caster ? Caster->GetActorForwardVector() : GetActorForwardVector();

		// Grab the exact crosshair location directly from the caster
		if (Caster)
		{
			if (UCombatComponent* Combat = Caster->GetCombatComponent())
			{
				FVector CrosshairTarget = Combat->GetHitTarget();
				DiveDir = (CrosshairTarget - GetActorLocation()).GetSafeNormal();
			}
		}

		// Blast it straight toward the crosshair!
		ProjectileMovement->Velocity = DiveDir * DiveSpeed;
		return;
	}

	// Target is already inside the break radius (point-blank): no homing at all, just one straight dive at them.
	// Without this the missile would keep its upward launch heading and fly off into the sky.
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (ToTarget.SizeSquared() <= FMath::Square(HomingBreakRadius))
	{
		ProjectileMovement->Velocity = ToTarget.GetSafeNormal() * DiveSpeed;
		return;
	}

	// Turn on homing for locked targets
	ProjectileMovement->bIsHomingProjectile = true;
	ProjectileMovement->HomingTargetComponent = Target->GetRootComponent();
	
	// Apply massive turning and speed to force the downward dive
	ProjectileMovement->HomingAccelerationMagnitude = DiveTurnRate; 
	ProjectileMovement->MaxSpeed = DiveSpeed;
}

void AMicroMissileProjectile::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Explode(OtherActor);
}

void AMicroMissileProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Explode(OtherActor);
}

void AMicroMissileProjectile::Explode(AActor* HitActor)
{
	if (!HasAuthority()) return;
	if (HitActor == Caster) return;
	if (HitActor && HitActor->IsA(AMicroMissileProjectile::StaticClass())) return;

	// Safety check: Don't explode twice if Hit and Overlap trigger on the exact same frame!
	if (CollisionSphere->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;

	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.f, Caster, 3500.f, FName("Explosion"));
	
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this);
	if (Caster) IgnoredActors.Add(Caster);

	UGameplayStatics::ApplyRadialDamage(
		GetWorld(), ExplosionDamage, GetActorLocation(), ExplosionRadius,
		UDamageType::StaticClass(), IgnoredActors, this, Caster ? Caster->GetController() : nullptr, false, ECC_Visibility
	);

	Multicast_Explode(GetActorLocation());
}

void AMicroMissileProjectile::Destroyed()
{
	// Safely turn off the trail so it fades naturally
	if (ActiveTrail)
	{
		ActiveTrail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ActiveTrail->Deactivate();
	}

	Super::Destroyed();
}

void AMicroMissileProjectile::Multicast_Explode_Implementation(FVector ImpactLocation)
{
	if (ExplosionParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionParticle, ImpactLocation, FRotator::ZeroRotator);
	}
	
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ImpactLocation);
	}

	if (ActiveTrail)
	{
		ActiveTrail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ActiveTrail->Deactivate();
	}

	// This safely destroys the missile on both the Server and Client
	Destroy();
}