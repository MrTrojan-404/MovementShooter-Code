
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaManager.generated.h"

class ABossBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBossPhaseChanged);

UCLASS()
class MOVEMENTSHOOTER_API AArenaManager : public AActor
{
	GENERATED_BODY()
	
public:	
	AArenaManager();
	
	// Called by the Boss Character when its health hits 0
	UFUNCTION(BlueprintCallable, Category = "Boss Arena")
	void AdvancePhase();

	UPROPERTY(BlueprintAssignable, Category = "Boss Arena")
	FOnBossPhaseChanged OnPhaseChanged;

	// Pass the player in so the Manager can tell the bosses who to kill
	UFUNCTION(BlueprintCallable, Category = "Boss Arena")
	void StartBossFight(AActor* InstigatorPlayer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss Arena")
	class UBoxComponent* ArenaTriggerBox;

	UFUNCTION()
	void OnArenaTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditInstanceOnly, Category = "Boss Arena")
	int32 CurrentPhase = 0;
	
	// Blueprint Events so you can visually trigger Shields, Generators, and Cinematics!
	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Arena")
	void BlueprintStartFight();

	UFUNCTION(BlueprintImplementableEvent, Category = "Boss Arena")
	void BlueprintTriggerPhase(int32 PhaseIndex);

	// Array of all bosses in this specific arena (e.g., Commander 1 and Commander 2)
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss Arena")
	TArray<ABossBase*> ArenaBosses;
	

	// Drag and drop your Laser Turrets into this array in the Editor
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Boss Arena")
	TArray<class ALaserTurret*> InnerTurrets;
	
};

