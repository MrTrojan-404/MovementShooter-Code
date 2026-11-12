
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/Actor/Item.h"
#include "MovementShooter/GameVariables/WeaponType.h"
#include "Weapon.generated.h"

UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	EWS_Initial UMETA(DisplayName = "Initial State"),
	EWS_Equipped UMETA(DisplayName = "Equipped"),
	EWS_EquippedSecondary UMETA(DisplayName = "Equipped Secondary"),
	EWS_Dropped UMETA(DisplayName = "Unequipped"),
	EWS_Stored UMETA(DisplayName = "Stored in Backpack"),
	
	EWS_MAX UMETA(DisplayName = "DefaultMAX")
};


UENUM(BlueprintType)
enum class EFireType : uint8
{
	EFT_HitScan UMETA(DisplayName = "Hit Scan Weapon"),
	EFT_Projectile UMETA(DisplayName = "Projectile Weapon"),
	EFT_Shotgun UMETA(DisplayName = "Shotgun Weapon"),

	EFT_MAX UMETA(DisplayName = "DefaultMAX")
};

UENUM(BlueprintType)
enum class EWeaponCategory : uint8
{
	EWC_Sidearm   UMETA(DisplayName = "Sidearm (Pistols)"),       // Slice 0 (Top)
	EWC_Assault   UMETA(DisplayName = "Assault (AR/SMG)"),        // Slice 1 (Top Right)
	EWC_Shotgun   UMETA(DisplayName = "Shotgun"),                 // Slice 2 (Bottom Right)
	EWC_Sniper    UMETA(DisplayName = "Sniper"),                  // Slice 3 (Bottom)
	EWC_Heavy     UMETA(DisplayName = "Heavy (Launchers)"),       // Slice 4 (Bottom Left)
	EWC_Special   UMETA(DisplayName = "Special (Flamethrower)"),  // Slice 5 (Top Left)

	EWC_MAX       UMETA(DisplayName = "DefaultMAX")
};

UCLASS()
class MOVEMENTSHOOTER_API AWeapon : public AItem
{
	GENERATED_BODY()

public:
	AWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	void SetWeaponState(EWeaponState WeaponStatetoSet);
	void Dropped();

	UPROPERTY(EditAnywhere)
	EFireType FireType;

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	bool bUseScatter = false;

	bool bDestroyWeapon = false;

	virtual FText GetInteractionText_Implementation() const override;
	
	virtual void Fire(const FVector& HitTarget);
	FVector TraceEndWithScatter(const FVector& HitTarget, float SpreadMultiplier = 1.0f);
	void AddAmmo(int32 AmmoToAdd);
	virtual void OnRep_Owner() override;
	void SetHUDAmmo();
	
	float CalculateDamage(FName HitBoneName) const;

	UPROPERTY(EditAnywhere, Category = Crosshairs)
	class UTexture2D* CrosshairsCenter;

	UPROPERTY(EditAnywhere, Category = Crosshairs)
	UTexture2D* CrosshairsLeft;

	UPROPERTY(EditAnywhere, Category = Crosshairs)
	UTexture2D* CrosshairsRight;

	UPROPERTY(EditAnywhere, Category = Crosshairs)
	UTexture2D* CrosshairsTop;

	UPROPERTY(EditAnywhere, Category = Crosshairs)
	UTexture2D* CrosshairsBottom;

	UPROPERTY(EditAnywhere, Category = Combat)
	float FireDelay = .15f;

	UPROPERTY(EditAnywhere, Category = Combat)
	bool bAutomatic = true;

	UPROPERTY(EditAnywhere)
	USoundBase* EquipSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Settings")
	TSubclassOf<UCameraShakeBase> FireCameraShakeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon Scatter")
	float WeaponMinimumSpread = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon Properties")
	EWeaponCategory WeaponCategory = EWeaponCategory::EWC_Assault;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon UI")
	FString WeaponName = TEXT("Default Weapon");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon UI")
	class UTexture2D* WeaponIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon UI")
	FLinearColor WeaponUIColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Outlines")
	int32 SecondaryWeaponStencil = 250;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Outlines")
	int32 DroppedWeaponStencil = 251;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Suppressor")
	bool bHasSuppressor = false;
	
	UPROPERTY(ReplicatedUsing = OnRep_bSuppressed, BlueprintReadOnly, Category = "Weapon|Suppressor")
	bool bSuppressed = false;

	UPROPERTY(EditAnywhere, Category = "Weapon|Suppressor")
	USoundBase* SuppressedFireSound;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Suppressor")
	void ToggleSuppressor();

	// The target Field of View when aiming down sights with this specific weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon|Aiming")
	float AimFOV = 60.0f;

	// --- RECOIL (per-weapon tuning) ---
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float VisualRecoilPitchMin = -0.4f;

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float VisualRecoilPitchMax = -0.8f;

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float VisualRecoilYawMin = -0.5f;

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float VisualRecoilYawMax = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float RecoilRecoverySpeed = 15.f;

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float RecoilKickSpeed = 15.f;
	
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float VisualRecoilRollMin = -1.0f;

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float VisualRecoilRollMax = 1.0f;

	// How much the weapon physically kicks back into the character (X is forward/back, Y is left/right, Z is up/down)
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	FVector RecoilKickbackMin = FVector(-3.f, -1.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	FVector RecoilKickbackMax = FVector(-6.f, 1.f, 1.f);

	// Multiplier applied to recoil when aiming down sights (e.g. 0.5 = 50% less recoil)
	UPROPERTY(EditAnywhere, Category = "Weapon|Recoil")
	float AimRecoilMultiplier = 0.5f;
	
	
UPROPERTY(EditAnywhere)
	UAnimSequence* CustomIdlePose;

	// Base property so we never have to cast to check weapon type
	UPROPERTY(EditAnywhere, Category = Combat)
	bool bIsChargeWeapon = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Reload")
	bool bUsesShellByShellReload = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Stasis")
	bool bIsWeaponInStasis = false;

	
protected:
	// Suppressor Mesh & Socket
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Suppressor")
	UStaticMeshComponent* SuppressorMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Suppressor")
	FName SuppressorSocketName = FName("MuzzleFlash");
	
	UPROPERTY(EditAnywhere)
	EWeaponType WeaponType = EWeaponType::EWT_AssaultRifle;	

	UPROPERTY(Replicated, EditAnywhere)
	bool bUseServerSideRewind = false;

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float DistanceToSphere = 800.f;

	UPROPERTY(EditAnywhere, Category = "Weapon Scatter")
	float SphereRadius = 75.f;

	UPROPERTY(EditAnywhere)
	float Damage = 20.f;

	UPROPERTY(EditAnywhere)
	float HeadShotDamage = 40.f;

	UPROPERTY()
	class ABaseCharacter* ShooterOwnerCharacter;
	UPROPERTY()
	class ACorePlayerController* ShooterOwnerController;

	UFUNCTION()
	void OnPingTooHigh(bool bPingTooHigh);

	UPROPERTY(EditDefaultsOnly)
	FName PrimarySocketName;

	UPROPERTY(EditDefaultsOnly)
	FName SecondarySocketName;

	UPROPERTY(EditAnywhere, Category = "Weapon|Suppressor")
	UParticleSystem* SuppressedMuzzleFlash;

	UPROPERTY(EditAnywhere, Category = "Weapon|Effects")
	UParticleSystem* MuzzleFlash;

	UPROPERTY(EditAnywhere, Category = "Weapon|Effects")
	USoundBase* FireSound;
	
	void AttachToSocket(FName SocketName);
	void DisablePhysicsAndCollision();

	void SpendRound();
	
private:
	UPROPERTY(ReplicatedUsing= OnRep_WeaponState)
	EWeaponState WeaponState = EWeaponState::EWS_Initial;

	UFUNCTION()
	void OnRep_WeaponState();

	void OnWeaponStateSet();
	void OnEquipped();
	void OnDropped();
	void OnEquippedSecondary();

	UPROPERTY(EditAnywhere, Category = "Weapon Properties")
	class UAnimationAsset* FireAnimation;

	UPROPERTY(EditAnywhere)
	TSubclassOf<class ABulletCasing> CasingClass;

	UPROPERTY(EditAnywhere)
	int32 Ammo;

	UFUNCTION(Client, Reliable)
	void ClientUpdateAmmo(int32 ServerAmmo);

	UFUNCTION(Client, Reliable)
	void ClientAddAmmo(int32 AmmoToAdd);
	
	UPROPERTY(EditAnywhere)
	int32 MagCapacity;

	int32 Sequence = 0;

	void OnStored();

	UFUNCTION(Server, Reliable)
	void ServerToggleSuppressor();

	UFUNCTION()
	void OnRep_bSuppressed();
	
public:
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; };
	bool IsEmpty();
	bool IsFull();
	FORCEINLINE int32 GetAmmo() const { return Ammo; }
	FORCEINLINE int32 GetMagCapacity() const { return MagCapacity; }
	FORCEINLINE float GetDamage() const { return Damage; }
	FORCEINLINE float GetHeadShotDamage() const { return HeadShotDamage; }
	FORCEINLINE EWeaponCategory GetWeaponCategory() const { return WeaponCategory; }
	FORCEINLINE EWeaponState GetWeaponState() const { return WeaponState; }
	FORCEINLINE bool IsSuppressed()   const { return bSuppressed;   }
	FORCEINLINE bool HasSuppressor()  const { return bHasSuppressor; }

};
