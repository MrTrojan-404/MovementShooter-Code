
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuspensionMatrixProjectile.generated.h"

class ABaseCharacter;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class MOVEMENTSHOOTER_API ASuspensionMatrixProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	ASuspensionMatrixProjectile();

	void Initialize(ABaseCharacter* InCaster);

protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Explode(AActor* HitTargetActor);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ProjectileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// The effect to spawn upon impact
	UPROPERTY(EditDefaultsOnly, Category = "Suspension Matrix")
	TSubclassOf<class ASuspensionMatrixEffect> MatrixEffectClass;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;
	
private:
	UPROPERTY()
	ABaseCharacter* Caster;

	bool bHasExploded = false;
};

