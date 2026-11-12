#include "MovementShooter/Objective/Checkpoint/CheckpointActor.h"

#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/Actor/Weapon.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "MovementShooter/Character/PlayerCharacter.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/GameMode/MissionGameMode.h"
#include "MovementShooter/Objective/ObjectiveTriggerComponent.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"
#include "MovementShooter/UI/Widgets/InteractWidget.h"

ACheckpointActor::ACheckpointActor()
{
    PrimaryActorTick.bCanEverTick = false;

    OverlapBox = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapBox"));
    RootComponent = OverlapBox;
    OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OverlapBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    OverlapBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    CheckpointMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CheckpointMesh"));
    CheckpointMesh->SetupAttachment(RootComponent);
    CheckpointMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    InteractWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractWidget"));
    InteractWidgetComponent->SetupAttachment(RootComponent);
    InteractWidgetComponent->SetVisibility(false);

    // Forces the widget to render crisply on the player's screen
    InteractWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
    InteractWidgetComponent->SetDrawAtDesiredSize(true);
}

void ACheckpointActor::BeginPlay()
{
    Super::BeginPlay();

    if (bTriggerOnce)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        OverlapBox->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointActor::OnOverlapBegin);
    }
}

#pragma region InteractionInterface

void ACheckpointActor::Interact_Implementation(AActor* Interactor)
{
    // Must be a body a human is driving
    ABaseCharacter* TriggeringBody = Cast<ABaseCharacter>(Interactor);
    if (!TriggeringBody || !TriggeringBody->GetController() || !TriggeringBody->GetController()->IsPlayerController()) return;

    if (!bTriggerOnce)
    {
        if (!IsSafeToSave(TriggeringBody))
        {
            if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Cannot save right now!"));
            return;
        }

        ActivateCheckpoint(TriggeringBody);
    }
}

FText ACheckpointActor::GetInteractionText_Implementation() const
{
    if (!bTriggerOnce)
    {
        ABaseCharacter* LocalBody = Cast<ABaseCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));

        if (!IsSafeToSave(LocalBody))
        {
            return FText::FromString(TEXT("Cannot Save Now!"));
        }
        return FText::FromString(TEXT("Save Game"));
    }

    return FText::GetEmpty();
}

#pragma endregion InteractionInterface

#pragma region Activation

void ACheckpointActor::Activate()
{
    if (bActivated && bTriggerOnce) return;

    bActivated = true;

    SavePlayer();
    SaveEnemies();
    SaveWorldWeapons();

    OnCheckpointActivated.Broadcast(this);

    if (bTriggerOnce && OverlapBox)
    {
        OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ACheckpointActor::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // Only a body driven by a human triggers the checkpoint
    ABaseCharacter* TriggeringBody = Cast<ABaseCharacter>(OtherActor);
    if (!TriggeringBody || !TriggeringBody->GetController() || !TriggeringBody->GetController()->IsPlayerController()) return;

    if (!IsSafeToSave(TriggeringBody)) return;

    ActivateCheckpoint(TriggeringBody);
}

void ACheckpointActor::ActivateCheckpoint(ABaseCharacter* TriggeringBody)
{
    AMissionGameMode* GM = Cast<AMissionGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM || !TriggeringBody) return;

    if (bActivated && bTriggerOnce) return;
    bActivated = true;

    SavePlayer();
    if (GM->CheckpointType == ECheckpointType::Snapshot)
    {
        SaveEnemies();
        SaveWorldWeapons();
    }

    GM->SetLatestGlobalCheckpoint(this);
    OnCheckpointActivated.Broadcast(this);

    if (bTriggerOnce && OverlapBox) OverlapBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    GM->SaveGameToDisk();
}

bool ACheckpointActor::IsSafeToSave(ABaseCharacter* TriggeringBody) const
{
    if (!TriggeringBody) return true;

    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(TriggeringBody))
    {
        if (Player->IsElimmed()) return false;
    }
    return TriggeringBody->GetHealth() > 0.f;
}

#pragma endregion Activation

#pragma region Save

void ACheckpointActor::SavePlayer()
{
    SavedData.CheckpointIndex = CheckpointIndex;
    SavedData.RestoreLocation = GetActorLocation();
    SavedData.RestoreRotation = GetActorRotation();
    SavedData.bHasPlayer = false;

    ABaseCharacter* Character = Cast<ABaseCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Character) return;

    FPlayerSaveData Data;
    Data.Health = Character->GetHealth();

    if (UCombatComponent* Combat = Character->GetCombatComponent())
    {
        Data.GrenadeCount = Combat->GetGrenades();
        Data.CarriedAmmoMap = Combat->CarriedAmmoMap;

        for (AWeapon* Wep : Combat->WeaponInventory)
        {
            if (Wep)
            {
                FWeaponSaveData WData;
                WData.WeaponClass = Wep->GetClass();
                WData.CurrentAmmo = Wep->GetAmmo();
                Data.SavedInventory.Add(WData);
            }
        }

        if (AWeapon* Primary = Combat->GetPrimaryWeapon())
        {
            Data.bHasPrimary = true;
            Data.PrimaryWeapon.WeaponClass = Primary->GetClass();
            Data.PrimaryWeapon.CurrentAmmo = Primary->GetAmmo();
        }
        if (AWeapon* Secondary = Combat->GetSecondaryWeapon())
        {
            Data.bHasSecondary = true;
            Data.SecondaryWeapon.WeaponClass = Secondary->GetClass();
            Data.SecondaryWeapon.CurrentAmmo = Secondary->GetAmmo();
        }
    }

    SavedData.Player = Data;
    SavedData.bHasPlayer = true;
}

void ACheckpointActor::SetObjectiveSnapshot(
    const TMap<FName, EObjectiveState>& Snapshot,
    const TMap<FName, FObjectiveConditionData>& ProgressSnapshot,
    FName ActiveObjID)
{
    SavedData.ObjectiveStates = Snapshot;
    SavedData.ObjectiveProgress = ProgressSnapshot; // Store the progress
    SavedData.ActiveObjectiveID = ActiveObjID;
}

void ACheckpointActor::SaveEnemies()
{
    SavedData.Enemies.Empty();

    TArray<AActor*> FoundEnemies;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), FoundEnemies);

    for (AActor* Actor : FoundEnemies)
    {
        if (AEnemyBase* Enemy = Cast<AEnemyBase>(Actor))
        {
            FEnemySaveData EData;
            EData.EnemyClass = Enemy->GetClass();
            EData.Location = Enemy->GetActorLocation();
            EData.Rotation = Enemy->GetActorRotation();
            EData.PatrolOrigin = Enemy->PatrolOrigin;
            EData.Health = Enemy->GetHealth();
            EData.bWasAlive = Enemy->IsAlive();

            // Record exactly what gun they are holding right now
            if (UCombatComponent* Combat = Enemy->GetCombatComponent())
            {
                if (AWeapon* Primary = Combat->GetPrimaryWeapon())
                {
                    EData.EquippedWeaponClass = Primary->GetClass();
                }
            }

            if (Enemy->ObjectiveTrigger)
            {
                EData.EventTag = Enemy->ObjectiveTrigger->EventTag;
            }

            // Save the path reference
            EData.WaypointPath = Enemy->WaypointPath;

            SavedData.Enemies.Add(EData);
        }
    }
}

void ACheckpointActor::SaveWorldWeapons()
{
    SavedData.WorldWeapons.Empty();
    TArray<AActor*> FoundWeapons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWeapon::StaticClass(), FoundWeapons);

    for (AActor* Actor : FoundWeapons)
    {
        AWeapon* Weapon = Cast<AWeapon>(Actor);
        // A weapon with no owner is either dropped on the floor or pre-placed
        if (Weapon && Weapon->GetOwner() == nullptr)
        {
            FWorldWeaponSaveData WData;
            WData.WeaponClass = Weapon->GetClass();
            WData.Location = Weapon->GetActorLocation();
            WData.Rotation = Weapon->GetActorRotation();
            WData.CurrentAmmo = Weapon->GetAmmo();

            // Capture the physical state of the weapon
            WData.SavedState = Weapon->GetWeaponState();

            SavedData.WorldWeapons.Add(WData);
        }
    }
}

#pragma endregion Save

#pragma region Restore

void ACheckpointActor::RestoreAllPlayers(bool bForceRespawn)
{
    if (!SavedData.IsValid()) return;

    AMissionGameMode* GM = Cast<AMissionGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    RestorePlayer(SavedData.Player, bForceRespawn);

    if (GM->CheckpointType == ECheckpointType::Snapshot)
    {
        RestoreEnemies();
        RestoreWorldWeapons();
    }
}

void ACheckpointActor::RestorePlayer(const FPlayerSaveData& SaveData, bool bForceRespawn)
{
    // Single-player: the player is simply whoever controller 0 is driving.
    ACorePlayerController* PC = Cast<ACorePlayerController>(UGameplayStatics::GetPlayerController(this, 0));
    if (!PC) return;

    ABaseCharacter* Character = Cast<ABaseCharacter>(PC->GetPawn());
    if (!Character) return;

    AMissionGameMode* GM = Cast<AMissionGameMode>(GetWorld()->GetAuthGameMode());
    if (!GM) return;

    bool bIsDead = Character->GetHealth() <= 0.f;
    if (const APlayerCharacter* Player = Cast<APlayerCharacter>(Character))
    {
        bIsDead = Player->IsElimmed();
    }

    // A persistent checkpoint only moves the player if they are dead
    bool bNeedsRespawn = true;
    if (GM->CheckpointType == ECheckpointType::Persistent && !bForceRespawn && !bIsDead)
    {
        bNeedsRespawn = false;
    }

    if (bNeedsRespawn)
    {
        // 1. Teleport
        const FVector SpawnOffset(0.f, 0.f, 50.f);
        Character->SetActorLocationAndRotation(SavedData.RestoreLocation + SpawnOffset, SavedData.RestoreRotation, false, nullptr, ETeleportType::TeleportPhysics);
        Character->ResetToAlive(); // re-enables the capsule at the new spot and restores the mesh
        Character->GetCharacterMovement()->StopMovementImmediately();
        PC->SetControlRotation(SavedData.RestoreRotation); // first-person view follows the controller
        Character->bDisableGameplay = false;

        // 2. Health depends on the checkpoint type
        if (GM->CheckpointType == ECheckpointType::Persistent)
        {
            Character->SetHealth(Character->GetMaxHealth());
        }
        else
        {
            // Snapshot restore: never come back with less than a quarter of your health
            const float MinHealth = Character->GetMaxHealth() * 0.25f;
            Character->SetHealth(FMath::Max(SaveData.Health, MinHealth));
        }

        PC->SetHUDHealth(Character->GetHealth(), Character->GetMaxHealth());
    }

    // --- INVENTORY REWIND (SNAPSHOT ONLY) ---
    if (GM->CheckpointType == ECheckpointType::Snapshot)
    {
        UCombatComponent* Combat = Character->GetCombatComponent();
        if (!Combat) return;

        Combat->Grenades = SaveData.GrenadeCount;
        Combat->UpdateHUDGrenades();
        Combat->CarriedAmmoMap = SaveData.CarriedAmmoMap;

        for (AWeapon* OldWep : Combat->WeaponInventory)
        {
            if (OldWep) OldWep->Destroy();
        }
        Combat->WeaponInventory.Empty();
        Combat->ClearWeaponPointers();

        AWeapon* RestoredPrimary = nullptr;
        AWeapon* RestoredSecondary = nullptr;

        for (const FWeaponSaveData& WData : SaveData.SavedInventory)
        {
            if (!WData.WeaponClass.IsNull())
            {
                UClass* ResolvedClass = WData.WeaponClass.LoadSynchronous();
                if (!ResolvedClass) continue;

                AWeapon* NewWep = GetWorld()->SpawnActor<AWeapon>(ResolvedClass, SavedData.RestoreLocation, SavedData.RestoreRotation);
                if (NewWep)
                {
                    NewWep->AddAmmo(WData.CurrentAmmo);
                    NewWep->SetWeaponState(EWeaponState::EWS_Stored);
                    Combat->AddWeaponToInventory(NewWep);

                    if (SaveData.bHasPrimary && WData.WeaponClass == SaveData.PrimaryWeapon.WeaponClass && !RestoredPrimary)
                        RestoredPrimary = NewWep;
                    else if (SaveData.bHasSecondary && WData.WeaponClass == SaveData.SecondaryWeapon.WeaponClass && !RestoredSecondary)
                        RestoredSecondary = NewWep;
                }
            }
        }

        if (RestoredPrimary) Combat->EquipWeapon(RestoredPrimary);
        if (RestoredSecondary)
        {
            Combat->EquipWeapon(RestoredSecondary);
        }

        if (APlayerCharacter* Player = Cast<APlayerCharacter>(Character)) Player->UpdateHUDAmmo();
    }
}

void ACheckpointActor::RestoreWorldWeapons()
{
    TArray<AActor*> FoundWeapons;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWeapon::StaticClass(), FoundWeapons);

    // 1. Wipe the floor clean of all dropped weapons
    for (AActor* Actor : FoundWeapons)
    {
        AWeapon* Weapon = Cast<AWeapon>(Actor);
        if (Weapon && Weapon->GetOwner() == nullptr)
        {
            Weapon->Destroy();
        }
    }

    // 2. Respawn the weapons exactly as they were
    for (const FWorldWeaponSaveData& WData : SavedData.WorldWeapons)
    {
        if (!WData.WeaponClass.IsNull())
        {
            UClass* ResolvedClass = WData.WeaponClass.LoadSynchronous();
            if (!ResolvedClass) continue;

            AWeapon* RestoredWeapon = GetWorld()->SpawnActor<AWeapon>(ResolvedClass, WData.Location, WData.Rotation);

            if (RestoredWeapon)
            {
                RestoredWeapon->AddAmmo(WData.CurrentAmmo);

                // Set it to the exact state it was in when saved.
                // Initial stays static; Dropped turns physics on.
                RestoredWeapon->SetWeaponState(WData.SavedState);
            }
        }
    }
}

void ACheckpointActor::RestoreEnemies()
{
    // 1. Wipe the board clean
    TArray<AActor*> ExistingEnemies;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), ExistingEnemies);

    for (AActor* Actor : ExistingEnemies)
    {
        if (AEnemyBase* Enemy = Cast<AEnemyBase>(Actor))
        {
            // Tell the enemy it is being deleted by the rewind so it doesn't trigger objectives
            Enemy->bIsResetting = true;

            // Destroy every weapon the enemy is holding
            if (UCombatComponent* Combat = Enemy->GetCombatComponent())
            {
                for (AWeapon* Wep : Combat->WeaponInventory)
                {
                    if (Wep) Wep->Destroy();
                }
                Combat->WeaponInventory.Empty();
                Combat->ClearWeaponPointers();
            }
        }

        Actor->Destroy();
    }

    // 2. Respawn the ones that were alive
    for (const FEnemySaveData& EData : SavedData.Enemies)
    {
        if (EData.bWasAlive && !EData.EnemyClass.IsNull())
        {
            UClass* ResolvedEnemyClass = EData.EnemyClass.LoadSynchronous();
            if (!ResolvedEnemyClass) continue;

            // Deferred spawn: halts the enemy's BeginPlay so variables can be injected first
            AEnemyBase* RespawnedEnemy = GetWorld()->SpawnActorDeferred<AEnemyBase>(
                        ResolvedEnemyClass, FTransform(EData.Rotation, EData.Location));

            if (RespawnedEnemy)
            {
                // Force them to hold the exact gun they had
                if (!EData.EquippedWeaponClass.IsNull())
                {
                    RespawnedEnemy->DefaultWeaponClass = EData.EquippedWeaponClass.LoadSynchronous();
                }

                RespawnedEnemy->SetHealth(EData.Health);
                RespawnedEnemy->PatrolOrigin = EData.PatrolOrigin;
                RespawnedEnemy->bIsResetting = false;

                if (RespawnedEnemy->ObjectiveTrigger)
                {
                    RespawnedEnemy->ObjectiveTrigger->EventTag = EData.EventTag;
                }

                // Hand the path back to the enemy before it wakes up
                RespawnedEnemy->WaypointPath = EData.WaypointPath;

                UGameplayStatics::FinishSpawningActor(RespawnedEnemy, FTransform(EData.Rotation, EData.Location));
            }
        }
    }
}

#pragma endregion Restore
