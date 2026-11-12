#include "MovementShooter/Actor/Weapon/MinigunWeapon.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/Actor/Weapon/Projectile/Projectile.h"

AMinigunWeapon::AMinigunWeapon()
{
	PrimaryActorTick.bCanEverTick = false; // No more ticking needed!
	bAutomatic = true;
	FireDelay = 0.15f; 
}

void AMinigunWeapon::Fire(const FVector& HitTarget)
{
	// 1. Base AWeapon::Fire for sound, casing, animation, and ammo
	AWeapon::Fire(HitTarget);

	// 2. Cycle barrel sockets (MuzzleFlash_1 .. TotalBarrels)
	FString SocketNameString = BarrelSocketPrefix + FString::FromInt(CurrentBarrelIndex);
	FName MuzzleSocketName = FName(*SocketNameString);

	CurrentBarrelIndex++;
	if (CurrentBarrelIndex > TotalBarrels)
	{
		CurrentBarrelIndex = 1;
	}

	const USkeletalMeshSocket* BarrelSocket = GetItemMesh()->GetSocketByName(MuzzleSocketName);
	if (!BarrelSocket) return;

	FTransform SocketTransform = BarrelSocket->GetSocketTransform(GetItemMesh());

	// 3. Spawn Muzzle Flash at active barrel
	if (MuzzleFlash && GetWorld())
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), MuzzleFlash, SocketTransform);
	}

	// 4. Spawn Projectile
	APawn* InstigatorPawn = Cast<APawn>(GetOwner());
	UWorld* World = GetWorld();
	if (InstigatorPawn && World && InstigatorPawn->HasAuthority())
	{
		FVector ToTarget = HitTarget - SocketTransform.GetLocation();
		FRotator TargetRotation = ToTarget.Rotation();

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = InstigatorPawn;

		AProjectile* SpawnedProjectile = World->SpawnActor<AProjectile>(
			ProjectileClass, SocketTransform.GetLocation(), TargetRotation, SpawnParams
		);

		if (SpawnedProjectile)
		{
			SpawnedProjectile->Damage = Damage;
			SpawnedProjectile->HeadShotDamage = HeadShotDamage;
			SpawnedProjectile->bUseServerSideRewind = false;
			if (bIsWeaponInStasis) SpawnedProjectile->SetStasis(true);
		}
	}
}