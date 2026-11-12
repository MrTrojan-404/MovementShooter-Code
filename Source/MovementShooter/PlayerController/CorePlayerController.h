// MovementShooter - single-player player controller (HUD, combat/UI inputs, mission state).
// Removed from the old controller: server time sync and ping, tag-team and character switching, squad commands,
// mark target, camera shoulder swap, scoreboard, spectator and the networked RPC plumbing.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CorePlayerController.generated.h"

class AShooterHUD;
class USoundBase;
class UUserWidget;
class UCharacterOverlay;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class UReturnToMainMenu;
class UWeaponRadialMenu;

UCLASS()
class MOVEMENTSHOOTER_API ACorePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ACorePlayerController();

	// --- HUD ---
	void SetHUDHealth(float Health, float MaxHealth);
	void SetHUDWeaponAmmo(int32 Ammo);
	void SetHUDCarriedAmmo(int32 Ammo);
	void SetHUDGrenades(int32 Grenades);
	void SetHUDUltimate(float CurrentCharge, float MaxCharge);
	void SetHUDEnergy(int32 CurrentSegments, int32 MaxSegments);
	void SetHUDMedkits(int32 Medkits);

	virtual void OnPossess(APawn* InPawn) override;

	void ShowReturnToMainMenu();

	// Hit marker from a bullet/laser hit (plain functions now; the names are kept so the weapons did not change)
	void Client_ShowHitMarker(bool bHeadshot);

	// Tells the HUD what the highest enemy detection level currently is.
	void Client_UpdateDetectionHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw);

	// The mission game mode calls this on success, failure and when play resumes. Implement the
	// victory/defeat UI in Blueprint (the old C++ body was empty).
	UFUNCTION(BlueprintNativeEvent, Category = "Mission")
	void ClientNotifyMissionState(FName State);

	// Pause menu buttons
	void ServerRestartLevel();
	void ServerRestartCheckpoint();

	// Called by the weapon wheel widget when it closes
	void WeaponWheelStarted();
	void WeaponWheelReleased();

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	virtual void PollInit();
	virtual void SetupInitialHUD();

#pragma region Inputs
	// Every key binding lives here (movement, camera and combat). The pawn only exposes functions.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* MoveAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* LookAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* JumpAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* CrouchAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* SprintAction;

	void MoveTriggered(const FInputActionValue& Value);
	void LookTriggered(const FInputActionValue& Value);
	void JumpButtonPressed();
	void JumpButtonReleased();
	void CrouchButtonPressed();
	void CrouchButtonReleased();
	void SprintButtonPressed();
	void SprintButtonReleased();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* InteractAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* FireAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* ReloadAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* SwapWeaponAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* DropAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* AimAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* ThrowGrenadeAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* ThrowDistractionAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* PauseAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* MeleeAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* WeaponWheelAction;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input) UInputAction* ToggleSuppressorAction;
	UPROPERTY(EditDefaultsOnly, Category = Input) UInputAction* DashAction;
	UPROPERTY(EditDefaultsOnly, Category = Input) UInputAction* ConsumeMedkitAction;
	UPROPERTY(EditDefaultsOnly, Category = Input) UInputAction* Spell1Action;
	UPROPERTY(EditDefaultsOnly, Category = Input) UInputAction* Spell2Action;
	UPROPERTY(EditDefaultsOnly, Category = Input) UInputAction* UltimateAction;
	UPROPERTY(EditDefaultsOnly, Category = Input) UInputAction* GroundSmashAction;
	
	void InteractButtonPressed();
	void InteractButtonReleased();
	void FireButtonPressed();
	void FireButtonReleased();
	void ReloadButtonPressed();
	void SwapButtonPressed();
	void DropButtonPressed();
	void AimButtonPressed();
	void AimButtonReleased();
	void ThrowGrenadeButtonPressed();
	void ThrowDistractionButtonPressed();
	void MeleeButtonPressed();
	void ToggleSuppressorButtonPressed();
	void DashButtonPressed();
	void ConsumeMedkitButtonPressed();
	void Spell1ButtonPressed();
	void Spell2ButtonPressed();
	void Spell2ButtonReleased();
	void UltimateButtonPressed();
	void GroundSmashButtonPressed();
	
#pragma endregion Inputs

	UPROPERTY()
	AShooterHUD* ShooterHUD;

	UPROPERTY()
	UCharacterOverlay* CharacterOverlay;

	// Values cached until the overlay widget exists
	bool bInitializeCharacterOverlay = false;
	float HUDHealth = 0.f;
	float HUDMaxHealth = 0.f;
	int32 HUDGrenades = 0;
	float HUDCarriedAmmo = 0.f;
	bool bInitializeCarriedAmmo = false;
	float HUDWeaponAmmo = 0.f;
	bool bInitializeWeaponAmmo = false;

	UPROPERTY(EditAnywhere, Category = HUD)
	TSubclassOf<UUserWidget> ReturnToMainMenuWidget;

	UPROPERTY()
	UReturnToMainMenu* ReturnToMainMenu;

	UPROPERTY(EditAnywhere)
	USoundBase* HitMarkerSound;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UWeaponRadialMenu> WeaponWheelClass;

	UPROPERTY()
	UWeaponRadialMenu* WeaponWheelInstance;

private:
	void PushHUDFromPawn(APawn* InPawn);

	float StealthHUDUpdateTimer = 0.f;
	int8 AmmoUIState = -1;   // -1 unknown, 0 hidden, 1 shown
	static constexpr float StealthHUDUpdateInterval = 0.1f;   // 10 Hz
};
