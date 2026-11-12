#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/GameVariables/WeaponType.h"
#include "MovementShooter/Actor/Weapon.h"
#include "MovementShooter/Objective/ObjectiveTypes.h"
#include "CheckpointTypes.generated.h"

class AEnemyBase;

UENUM(BlueprintType)
enum class ECheckpointType : uint8
{
    // Restoring rewinds everything: player, enemies, dropped weapons and objectives.
    Snapshot    UMETA(DisplayName = "Snapshot (Rewinds Time)"),
    // Restoring only brings the player back; the world stays as it is.
    Persistent  UMETA(DisplayName = "Persistent (Respawn Only)")
};

USTRUCT(BlueprintType)
struct FWeaponSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftClassPtr<AWeapon> WeaponClass = nullptr;

    UPROPERTY()
    int32 CurrentAmmo = 0;
};

USTRUCT(BlueprintType)
struct FWorldWeaponSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftClassPtr<AWeapon> WeaponClass = nullptr;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    int32 CurrentAmmo = 0;

    UPROPERTY()
    EWeaponState SavedState = EWeaponState::EWS_Initial;
};

// ENEMY SAVE DATA
USTRUCT(BlueprintType)
struct FEnemySaveData
{
    GENERATED_BODY()

    UPROPERTY()
    TSoftClassPtr<AEnemyBase> EnemyClass = nullptr;

    UPROPERTY()
    TSoftClassPtr<AWeapon> EquippedWeaponClass = nullptr;

    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    UPROPERTY()
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY()
    FVector PatrolOrigin = FVector::ZeroVector;

    UPROPERTY()
    float Health = 100.f;

    UPROPERTY()
    bool bWasAlive = false;

    UPROPERTY()
    FName EventTag = NAME_None;

    UPROPERTY()
    class AWaypointPath* WaypointPath = nullptr;
};

// PLAYER SAVE DATA
USTRUCT(BlueprintType)
struct FPlayerSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    float Health = 100.f;

    UPROPERTY()
    FWeaponSaveData PrimaryWeapon;

    UPROPERTY()
    FWeaponSaveData SecondaryWeapon;

    UPROPERTY()
    bool bHasPrimary = false;

    UPROPERTY()
    bool bHasSecondary = false;

    UPROPERTY()
    int32 GrenadeCount = 0;

    UPROPERTY()
    TArray<FWeaponSaveData> SavedInventory;

    UPROPERTY()
    TMap<EWeaponType, int32> CarriedAmmoMap;
};

USTRUCT(BlueprintType)
struct FCheckpointSaveData
{
    GENERATED_BODY()

    UPROPERTY()
    int32 CheckpointIndex = 0;

    UPROPERTY()
    bool bHasPlayer = false;

    UPROPERTY()
    FPlayerSaveData Player;

    UPROPERTY()
    TArray<FEnemySaveData> Enemies;

    UPROPERTY()
    TArray<FWorldWeaponSaveData> WorldWeapons;

    UPROPERTY()
    FVector RestoreLocation = FVector::ZeroVector;

    UPROPERTY()
    FRotator RestoreRotation = FRotator::ZeroRotator;

    UPROPERTY()
    FName ActiveObjectiveID = NAME_None;

    UPROPERTY()
    TMap<FName, EObjectiveState> ObjectiveStates;

    UPROPERTY()
    TMap<FName, FObjectiveConditionData> ObjectiveProgress;

    bool IsValid() const
    {
        return bHasPlayer;
    }
};
