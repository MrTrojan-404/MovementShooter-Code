#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MovementShooter/Objective/Checkpoint/CheckpointTypes.h"
#include "MovementShooter/Interface/InteractionInterface.h"
#include "CheckpointActor.generated.h"

class UWidgetComponent;
class UBoxComponent;
class ABaseCharacter;
class APlayerCharacter;
class ACorePlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointActivated, ACheckpointActor*, Checkpoint);

UCLASS()
class MOVEMENTSHOOTER_API ACheckpointActor : public AActor, public IInteractionInterface
{
    GENERATED_BODY()

public:
    ACheckpointActor();

    // Called by the game mode or on overlap
    void Activate();

    // Called by the game mode when the mission fails or the player restarts from the checkpoint
    void RestoreAllPlayers(bool bForceRespawn = false);

    // Called by the game mode after Activate()
    void SetObjectiveSnapshot(
         const TMap<FName, EObjectiveState>& Snapshot,
         const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
         FName ActiveObjID);

    // The game mode listens to this
    UPROPERTY(BlueprintAssignable, Category = "Checkpoint")
    FOnCheckpointActivated OnCheckpointActivated;

    FORCEINLINE int32 GetCheckpointIndex() const { return CheckpointIndex; }
    FORCEINLINE bool IsActivated() const { return bActivated; }
    FORCEINLINE FCheckpointSaveData GetSaveData() const { return SavedData; }

#pragma region InteractionInterface
    virtual void Interact_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionText_Implementation() const override;
#pragma endregion InteractionInterface

#pragma region Configuration
    // If true, the checkpoint turns off permanently after the first touch.
    // If false, the player can return here and save again as often as they like.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bTriggerOnce = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    int32 CheckpointIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    bool bActivateOnOverlap = false;

    // Where the player restores to
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    FVector RestoreLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    FRotator RestoreRotation = FRotator::ZeroRotator;
#pragma endregion Configuration

    void ActivateCheckpoint(ABaseCharacter* TriggeringBody);

    // Lets the game mode inject the data loaded from disk into this actor
    void InjectHardDriveData(const FCheckpointSaveData& InData) { SavedData = InData; }

protected:
    virtual void BeginPlay() override;

    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor, UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult);

private:
    UPROPERTY(VisibleAnywhere, Category = "Checkpoint")
    UBoxComponent* OverlapBox = nullptr;

    // The physical mesh for the checkpoint
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint", meta = (AllowPrivateAccess = "true"))
    UStaticMeshComponent* CheckpointMesh = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
    UWidgetComponent* InteractWidgetComponent = nullptr;

    FCheckpointSaveData SavedData;

    bool bActivated = false;

    // The player can only save while alive
    bool IsSafeToSave(ABaseCharacter* TriggeringBody) const;

    void SavePlayer();
    void RestorePlayer(const FPlayerSaveData& SaveData, bool bForceRespawn);

    void SaveEnemies();
    void RestoreEnemies();

    void SaveWorldWeapons();
    void RestoreWorldWeapons();
};
