#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DestructiblePillar.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API ADestructiblePillar : public AActor
{
	GENERATED_BODY()

public:
	ADestructiblePillar();
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* PillarMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RubbleMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pillar")
	float MaxHealth = 400.f;

	float CurrentHealth = 400.f;
	bool bDestroyed = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "Pillar")
	void OnPillarDestroyed();

	virtual void BeginPlay() override;
};