#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "MovementShooter/Objective/ObjectiveTypes.h"
#include "MovementShooter/Objective/Checkpoint/CheckpointTypes.h"
#include "MissionGameMode.generated.h"

class ABaseCharacter;
class AObjectiveActor;
class ACheckpointActor;
class APlayerCharacter;

namespace MissionMatchState
{
    extern MOVEMENTSHOOTER_API const FName MissionSuccess;
    extern MOVEMENTSHOOTER_API const FName MissionFailed;
}

/**
 * Single-player game mode: objective chain, checkpoints, mission success/fail and save/load.
 * When the player dies the mission fails, and after a short delay everything is restored from
 * the last checkpoint.
 */
UCLASS()
class MOVEMENTSHOOTER_API AMissionGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    AMissionGameMode();

    // The player died: start the death sequence, fail the mission and, after a delay,
    // restore from the last checkpoint. (Respawning is the checkpoint restore, never a random start.)
    void PlayerEliminated(APlayerCharacter* ElimmedCharacter);

    // Called by an AObjectiveActor delegate
    UFUNCTION()
    void OnObjectiveStateChanged(AObjectiveActor* Objective, EObjectiveState NewState);

    // Called by an ACheckpointActor delegate
    UFUNCTION()
    void OnCheckpointActivated(ACheckpointActor* Checkpoint);

    void RegisterObjectiveChain(AObjectiveActor* First);

    TMap<FName, FObjectiveConditionData> BuildProgressSnapshot() const;

    // Manually force a checkpoint reload (pause menu)
    void ForceRestartCheckpoint();

#pragma region Configuration
    // Delay between the mission failing and the restore
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
    float MissionFailedRestoreDelay = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mission")
    ECheckpointType CheckpointType = ECheckpointType::Snapshot;
#pragma endregion Configuration

    UPROPERTY()
    ACheckpointActor* LatestCheckpoint = nullptr;

    void SetLatestGlobalCheckpoint(ACheckpointActor* Checkpoint);

    UFUNCTION(BlueprintCallable, Category = "Save System")
    void SaveGameToDisk();

    UFUNCTION(BlueprintCallable, Category = "Save System")
    void LoadGameFromDisk();

protected:
    virtual void BeginPlay() override;

private:
#pragma region Objectives
    // All objectives in the chain, filled in by RegisterObjectiveChain
    UPROPERTY()
    TArray<AObjectiveActor*> AllObjectives;

    // Currently active objective
    UPROPERTY()
    AObjectiveActor* ActiveObjective = nullptr;

    void CheckMissionComplete();

    // Snapshot of every objective's state
    TMap<FName, EObjectiveState> BuildObjectiveSnapshot() const;
#pragma endregion Objectives

#pragma region Checkpoints
    // All checkpoints sorted by index
    UPROPERTY()
    TArray<ACheckpointActor*> AllCheckpoints;

    void RegisterCheckpoints();
    void RestoreFromCheckpoint(bool bIsManualRestart = false);

    // Guards against LoadGameFromDisk <-> RestoreFromCheckpoint calling each other forever
    bool bLoadingFromDisk = false;

    void ResetObjectivesToSnapshot(
        const TMap<FName, EObjectiveState>& Snapshot,
        const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
        FName ActiveObjectiveID);
#pragma endregion Checkpoints

#pragma region MissionState
    FTimerHandle MissionFailedRestoreTimer;

    void TriggerMissionSuccess();
    void TriggerMissionFailed();
    void ExecuteRestore();
    void UnlockPlayer();

    // The HUD listens through the player controller
    void BroadcastMissionState(FName State);
#pragma endregion MissionState
};
