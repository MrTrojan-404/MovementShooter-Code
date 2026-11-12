
#include "MovementShooter/SpellAbility/Spell/Ren/StasisSpell.h"

#include "MovementShooter/Character/BaseCharacter.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "MovementShooter/Actor/Weapon/Projectile/Projectile.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/SpellAbility/SpellComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "MovementShooter/Actor/Weapon/HitScanWeapon.h"
#include "MovementShooter/Character/PlayerCharacter.h"

void UStasisSpell::Execute_Implementation(ABaseCharacter* Caster)
{
	if (!Caster || !Caster->HasAuthority() || bStasisActive) return;

	if (USpellComponent* SpellComp = Caster->FindComponentByClass<USpellComponent>())
	{
		SpellComp->Multicast_ActivateStasis(StasisDuration);
	}
}

void UStasisSpell::TickStasis(float RealDeltaSeconds)
{
	if (!CachedCaster || !bStasisActive) return;

	// 1. Smoothly fade in the Post Process Effect on the local player's screen
	if (CachedCaster && CachedCaster->IsLocallyControlled())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(CachedCaster))
		{
			if (UCameraComponent* Cam = Player->GetFirstPersonCamera())
			{
				CurrentPPWeight = FMath::FInterpTo(CurrentPPWeight, 1.0f, RealDeltaSeconds, PostProcessFadeSpeed);
				Cam->PostProcessBlendWeight = CurrentPPWeight;
			}
		}
	}
	
	// Only the Server tracks the timer (No more looping through projectiles!)
	if (CachedCaster->HasAuthority())
	{
		ElapsedRealTime += RealDeltaSeconds;
		if (ElapsedRealTime >= StasisDuration)
		{
			if (USpellComponent* SpellComp = CachedCaster->FindComponentByClass<USpellComponent>())
			{
				SpellComp->Multicast_DeactivateStasis();
			}
		}
	}
}

void UStasisSpell::LocalActivate(ABaseCharacter* Caster, float Duration)
{
	CachedCaster = Caster;
	bStasisActive = true;
	ElapsedRealTime = 0.f;
	StasisDuration = Duration;

	Caster->OnStasisActive(true);
	StasisOrigin = Caster->GetActorLocation(); 

	// 1. GLOBAL BULLET TIME (world slows down, the caster is boosted back to normal speed)
	if (UWorld* World = Caster->GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, BulletTimeGlobalDilation);
		
		float PlayerBoost = 1.0f / BulletTimeGlobalDilation;
		Caster->CustomTimeDilation = PlayerBoost;

		TArray<AActor*> AttachedActors;
		Caster->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			Attached->CustomTimeDilation = PlayerBoost;
		}
	}

	// 2. APPLY POST PROCESS SETTINGS TO CAMERA
	CurrentPPWeight = 0.f;
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(Caster))
	{
		if (UCameraComponent* Cam = Player->GetFirstPersonCamera())
		{
			Cam->PostProcessSettings = BulletTimePostProcess;
			Cam->PostProcessBlendWeight = 0.f;
		}
	}
	
	// 2. AUDIO
	if (CastSound) UGameplayStatics::PlaySoundAtLocation(Caster, CastSound, StasisOrigin);
	if (LoopSound) ActiveLoopSound = UGameplayStatics::SpawnSoundAtLocation(Caster, LoopSound, StasisOrigin);

	// 3. BUFFER CASTER'S WEAPONS
	if (UCombatComponent* Combat = Caster->FindComponentByClass<UCombatComponent>())
	{
		FlaggedWeapons.Empty();
		if (AWeapon* Primary = Combat->GetPrimaryWeapon()) RegisterWeaponForStasis(Primary);
		if (AWeapon* Secondary = Combat->GetSecondaryWeapon()) RegisterWeaponForStasis(Secondary);
	}
}

void UStasisSpell::LocalDeactivate()
{
	bStasisActive = false;

	// --- Reset the screen effect instantly ---
	if (CachedCaster && CachedCaster->IsLocallyControlled())
	{
		if (APlayerCharacter* Player = Cast<APlayerCharacter>(CachedCaster))
		{
			if (UCameraComponent* Cam = Player->GetFirstPersonCamera())
			{
				Cam->PostProcessBlendWeight = 0.f; // Snap the effect off
			}
		}
	}
	
	// 1. RESTORE NORMAL TIME
	if (CachedCaster && CachedCaster->GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(CachedCaster->GetWorld(), 1.f);
		CachedCaster->CustomTimeDilation = 1.f;
		
		TArray<AActor*> AttachedActors;
		CachedCaster->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			Attached->CustomTimeDilation = 1.f;
		}
        
        CachedCaster->OnStasisActive(false);
	}

	// 2. RELEASE EVERYTHING THAT WAS FLAGGED AS "IN STASIS"
	// Projectiles fired during the spell get SetStasis(true) from the weapons. Clear the flag again here so
	// rockets stop being redirectable and grenade/gravity timers get unpaused.
	if (CachedCaster && CachedCaster->GetWorld())
	{
		for (TActorIterator<AProjectile> It(CachedCaster->GetWorld()); It; ++It)
		{
			if (It->bIsInStasis)
			{
				It->SetStasis(false);
			}
		}
	}

	// 3. AUDIO
	if (ActiveLoopSound)
	{
		ActiveLoopSound->Stop();
		ActiveLoopSound = nullptr;
	}
	if (EndSound && CachedCaster) UGameplayStatics::PlaySoundAtLocation(CachedCaster, EndSound, StasisOrigin);

	// 4. RELEASE BUFFERED SHOTS
	for (TWeakObjectPtr<AWeapon> WeaponPtr : FlaggedWeapons)
	{
		if (AWeapon* Weapon = WeaponPtr.Get())
		{
			Weapon->bIsWeaponInStasis = false;

			if (AHitScanWeapon* HitScan = Cast<AHitScanWeapon>(Weapon))
			{
				HitScan->ReleaseBufferedShots();
			}
		}
	}
	FlaggedWeapons.Empty();
	CachedCaster = nullptr;
}

void UStasisSpell::RegisterWeaponForStasis(AWeapon* NewWeapon)
{
	if (!bStasisActive || !NewWeapon) return;
	NewWeapon->bIsWeaponInStasis = true;
	FlaggedWeapons.AddUnique(NewWeapon);
}
