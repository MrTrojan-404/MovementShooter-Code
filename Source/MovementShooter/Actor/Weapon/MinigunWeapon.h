#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/Actor/Weapon/ProjectileWeapon.h"
#include "MinigunWeapon.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API AMinigunWeapon : public AProjectileWeapon
{
	GENERATED_BODY()

public:
	AMinigunWeapon();
	virtual void Fire(const FVector& HitTarget) override;

protected:
	// Tracks which barrel is up next
	int32 CurrentBarrelIndex = 1;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|TriBarrel")
	FString BarrelSocketPrefix = TEXT("MuzzleFlash_");

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|TriBarrel")
	int32 TotalBarrels = 3;
};