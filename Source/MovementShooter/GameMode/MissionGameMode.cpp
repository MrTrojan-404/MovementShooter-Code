#include "MovementShooter/GameMode/MissionGameMode.h"

#include "Kismet/GameplayStatics.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/Character/PlayerCharacter.h"
#include "MovementShooter/Objective/ObjectiveActor.h"
#include "MovementShooter/Objective/Checkpoint/CheckpointActor.h"
#include "MovementShooter/Objective/SaveGame/ShooterSaveGame.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"

namespace MissionMatchState
{
    const FName MissionSuccess = FName("MissionSuccess");
    const FName MissionFailed  = FName("MissionFailed");
}

AMissionGameMode::AMissionGameMode()
{
}

void AMissionGameMode::BeginPlay()
{
    Super::BeginPlay();
    RegisterCheckpoints();
}

#pragma region PlayerEliminated
void AMissionGameMode::PlayerEliminated(APlayerCharacter* ElimmedCharacter)
{
    if (!ElimmedCharacter) return;

    ElimmedCharacter->Elim(); // locks the player and plays the death sequence
    TriggerMissionFailed();
}
#pragma endregion PlayerEliminated

#pragma region Objectives
void AMissionGameMode::RegisterObjectiveChain(AObjectiveActor* First)
{
    if (!First) return;

    AObjectiveActor* Current = First;
    while (Current)
    {
        AllObjectives.AddUnique(Current);
        Current->OnObjectiveStateChanged.AddDynamic(this, &AMissionGameMode::OnObjectiveStateChanged);

        for (AObjectiveActor* Parallel : Current->ParallelObjectives)
        {
            if (Parallel)
            {
                AllObjectives.AddUnique(Parallel);
                Parallel->OnObjectiveStateChanged.AddDynamic(this, &AMissionGameMode::OnObjectiveStateChanged);
            }
        }

        Current = Current->NextObjective;
    }

    First->Activate();
    ActiveObjective = First;
}

void AMissionGameMode::OnObjectiveStateChanged(AObjectiveActor* Objective, EObjectiveState NewState)
{
    if (NewState == EObjectiveState::Active)
    {
        ActiveObjective = Objective;
    }
    else if (NewState == EObjectiveState::Completed)
    {
        CheckMissionComplete();
    }
    else if (NewState == EObjectiveState::Failed)
    {
        if (!Objective->IsOptional())
        {
            TriggerMissionFailed();
        }
    }
}

void AMissionGameMode::CheckMissionComplete()
{
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;
        if (Objective->IsOptional()) continue;
        if (Objective->GetObjectiveState() != EObjectiveState::Completed)
        {
            return;
        }
    }
    TriggerMissionSuccess();
}

TMap<FName, EObjectiveState> AMissionGameMode::BuildObjectiveSnapshot() const
{
    TMap<FName, EObjectiveState> Snapshot;
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;
        Snapshot.Emplace(Objective->ObjectiveID, Objective->GetObjectiveState());
    }
    return Snapshot;
}

TMap<FName, FObjectiveConditionData> AMissionGameMode::BuildProgressSnapshot() const
{
    TMap<FName, FObjectiveConditionData> Snapshot;
    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;
        Snapshot.Emplace(Objective->ObjectiveID, Objective->ConditionData);
    }
    return Snapshot;
}
#pragma endregion Objectives

#pragma region Checkpoints
void AMissionGameMode::RegisterCheckpoints()
{
    TArray<AActor*> CheckpointActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACheckpointActor::StaticClass(), CheckpointActors);

    for (AActor* Actor : CheckpointActors)
    {
        ACheckpointActor* Checkpoint = Cast<ACheckpointActor>(Actor);
        if (!Checkpoint) continue;

        AllCheckpoints.Add(Checkpoint);
        Checkpoint->OnCheckpointActivated.AddDynamic(this, &AMissionGameMode::OnCheckpointActivated);
    }

    AllCheckpoints.Sort([](const ACheckpointActor& A, const ACheckpointActor& B)
    {
        return A.GetCheckpointIndex() < B.GetCheckpointIndex();
    });
}

void AMissionGameMode::OnCheckpointActivated(ACheckpointActor* Checkpoint)
{
    if (!Checkpoint) return;

    // Never go backwards to an earlier checkpoint
    if (LatestCheckpoint && Checkpoint->GetCheckpointIndex() < LatestCheckpoint->GetCheckpointIndex()) return;

    LatestCheckpoint = Checkpoint;
    Checkpoint->SetObjectiveSnapshot(
        BuildObjectiveSnapshot(),
        BuildProgressSnapshot(),
        ActiveObjective ? ActiveObjective->ObjectiveID : NAME_None
    );

    SaveGameToDisk();
}

void AMissionGameMode::SetLatestGlobalCheckpoint(ACheckpointActor* Checkpoint)
{
    LatestCheckpoint = Checkpoint;
    Checkpoint->SetObjectiveSnapshot(BuildObjectiveSnapshot(), BuildProgressSnapshot(), ActiveObjective ? ActiveObjective->ObjectiveID : NAME_None);
}

void AMissionGameMode::RestoreFromCheckpoint(bool bIsManualRestart)
{
    if (!LatestCheckpoint || !LatestCheckpoint->GetSaveData().IsValid())
    {
        // Nothing in memory (for example right after launching the game): try the save on disk first.
        if (!bLoadingFromDisk && UGameplayStatics::DoesSaveGameExist(TEXT("MainSaveSlot"), 0))
        {
            UE_LOG(LogTemp, Warning, TEXT("Memory empty! Found save on disk. Loading..."));
            bLoadingFromDisk = true;
            LoadGameFromDisk(); // calls ForceRestartCheckpoint() once the data is in memory
            bLoadingFromDisk = false;
            return;
        }

        // No checkpoint at all: restart the level
        GetWorld()->ServerTravel("?Restart", false);
        return;
    }

    LatestCheckpoint->RestoreAllPlayers(bIsManualRestart);

    if (CheckpointType == ECheckpointType::Snapshot)
    {
        const FCheckpointSaveData& SaveData = LatestCheckpoint->GetSaveData();
        ResetObjectivesToSnapshot(
            SaveData.ObjectiveStates,
            SaveData.ObjectiveProgress,
            SaveData.ActiveObjectiveID
        );
    }
}

void AMissionGameMode::ResetObjectivesToSnapshot(
    const TMap<FName, EObjectiveState>& Snapshot,
    const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
    FName ActiveObjectiveID)
{
    AObjectiveActor* ObjectiveToActivate = nullptr;

    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (!Objective) continue;

        const EObjectiveState* SavedState = Snapshot.Find(Objective->ObjectiveID);
        const FObjectiveConditionData* SavedProgress = ProgressSnapshot.Find(Objective->ObjectiveID);

        if (!SavedState || !SavedProgress)
        {
            Objective->ResetObjective(EObjectiveState::Inactive, Objective->ConditionData);
            continue;
        }

        if (Objective->ObjectiveID == ActiveObjectiveID)
        {
            Objective->ResetObjective(EObjectiveState::Inactive, *SavedProgress);
            ObjectiveToActivate = Objective;
        }
        else
        {
            Objective->ResetObjective(*SavedState, *SavedProgress);
        }
    }

    for (AObjectiveActor* Objective : AllObjectives)
    {
        if (Objective)
        {
            Objective->SyncParallelStates();
        }
    }

    if (ObjectiveToActivate)
    {
        ObjectiveToActivate->Activate();
        ActiveObjective = ObjectiveToActivate;
    }
}
#pragma endregion Checkpoints

#pragma region MissionState
void AMissionGameMode::TriggerMissionSuccess()
{
    SetMatchState(MissionMatchState::MissionSuccess);
    BroadcastMissionState(MissionMatchState::MissionSuccess);
}

void AMissionGameMode::TriggerMissionFailed()
{
    // Already failing: don't start a second restore timer
    if (GetMatchState() == MissionMatchState::MissionFailed) return;

    SetMatchState(MissionMatchState::MissionFailed);
    BroadcastMissionState(MissionMatchState::MissionFailed);

    GetWorldTimerManager().SetTimer(
        MissionFailedRestoreTimer,
        this,
        &AMissionGameMode::ExecuteRestore,
        MissionFailedRestoreDelay,
        false
    );
}

void AMissionGameMode::ExecuteRestore()
{
    RestoreFromCheckpoint();
    UnlockPlayer();

    SetMatchState(MatchState::InProgress);
    BroadcastMissionState(MatchState::InProgress);
}

void AMissionGameMode::ForceRestartCheckpoint()
{
    RestoreFromCheckpoint(true);
    UnlockPlayer();

    SetMatchState(MatchState::InProgress);
    BroadcastMissionState(MatchState::InProgress);
}

void AMissionGameMode::UnlockPlayer()
{
    if (ABaseCharacter* Player = Cast<ABaseCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        Player->bDisableGameplay = false;
    }
}

void AMissionGameMode::BroadcastMissionState(FName State)
{
    if (ACorePlayerController* PC = Cast<ACorePlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        PC->ClientNotifyMissionState(State);
    }
}
#pragma endregion MissionState

#pragma region SaveLoad
void AMissionGameMode::SaveGameToDisk()
{
    if (!LatestCheckpoint) return;

    if (UShooterSaveGame* SaveInst = Cast<UShooterSaveGame>(UGameplayStatics::CreateSaveGameObject(UShooterSaveGame::StaticClass())))
    {
        SaveInst->SavedCheckpointData = LatestCheckpoint->GetSaveData();
        UGameplayStatics::SaveGameToSlot(SaveInst, TEXT("MainSaveSlot"), 0);

        UE_LOG(LogTemp, Warning, TEXT("Game Saved to Hard Drive!"));
        if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Green, TEXT("Checkpoint Saved to Disk!"));
    }
}

void AMissionGameMode::LoadGameFromDisk()
{
    if (!UGameplayStatics::DoesSaveGameExist(TEXT("MainSaveSlot"), 0)) return;

    if (UShooterSaveGame* LoadedGame = Cast<UShooterSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("MainSaveSlot"), 0)))
    {
        TArray<AActor*> Checkpoints;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACheckpointActor::StaticClass(), Checkpoints);

        for (AActor* Actor : Checkpoints)
        {
            if (ACheckpointActor* CP = Cast<ACheckpointActor>(Actor))
            {
                if (CP->GetCheckpointIndex() == LoadedGame->SavedCheckpointData.CheckpointIndex)
                {
                    LatestCheckpoint = CP;
                    LatestCheckpoint->InjectHardDriveData(LoadedGame->SavedCheckpointData);

                    ForceRestartCheckpoint();

                    UE_LOG(LogTemp, Warning, TEXT("Game Loaded from Hard Drive!"));
                    break;
                }
            }
        }
    }
}
#pragma endregion SaveLoad
