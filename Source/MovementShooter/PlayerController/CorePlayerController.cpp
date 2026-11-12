#include "MovementShooter/PlayerController/CorePlayerController.h"

#include "AIController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "Engine/LocalPlayer.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/AI/Controller/EnemyAIController.h"
#include "MovementShooter/Actor/Weapon.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/Character/PlayerCharacter.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/Component/InteractionComponent.h"
#include "MovementShooter/GameMode/MissionGameMode.h"
#include "MovementShooter/SpellAbility/SpellComponent.h"
#include "MovementShooter/Subsystem/StealthSubsystem.h"
#include "MovementShooter/UI/HUD/ShooterHUD.h"
#include "MovementShooter/UI/Widgets/CharacterOverlay.h"
#include "MovementShooter/UI/Widgets/ReturnToMainMenu.h"
#include "MovementShooter/UI/Widgets/COOP/WeaponRadialMenu/WeaponRadialMenu.h"

ACorePlayerController::ACorePlayerController()
{
}

void ACorePlayerController::BeginPlay()
{
	Super::BeginPlay();

	ShooterHUD = Cast<AShooterHUD>(GetHUD());

	// Register the input mapping context (all actions live in it)
	if (IsLocalPlayerController() && DefaultMappingContext)
	{
		if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ACorePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	PollInit();

	// The ammo counters show whenever a gun is equipped (and hide when the hands are empty)
	if (CharacterOverlay)
	{
		const ABaseCharacter* AmmoChar = Cast<ABaseCharacter>(GetPawn());
		const UCombatComponent* AmmoCombat = AmmoChar ? AmmoChar->GetCombatComponent() : nullptr;
		const bool bHasGun = AmmoCombat && AmmoCombat->GetPrimaryWeapon();
		const int8 WantedState = bHasGun ? 1 : 0;
		if (AmmoUIState != WantedState)
		{
			AmmoUIState = WantedState;
			CharacterOverlay->SetAmmoUIVisibility(bHasGun);
		}
	}

	// Safety net: if the aim button is no longer down but the character is still aiming (a missed release
	// event), let go of the aim so the zoom always comes back.
	if (AimAction && IsLocalPlayerController())
	{
		ABaseCharacter* AimChar = Cast<ABaseCharacter>(GetPawn());
		UCombatComponent* AimCombat = AimChar ? AimChar->GetCombatComponent() : nullptr;
		if (AimCombat && AimCombat->GetIsAiming())
		{
			bool bAimHeld = true;
			if (ULocalPlayer* LP = GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					if (UEnhancedPlayerInput* MyPlayerInput = Sub->GetPlayerInput())
					{
						bAimHeld = MyPlayerInput->GetActionValue(AimAction).IsNonZero();
					}
				}
			}
			if (!bAimHeld) AimButtonReleased();
		}
	}

	// ── Stealth detection HUD: poll the subsystem and push to the overlay ──
	StealthHUDUpdateTimer += DeltaTime;
	if (StealthHUDUpdateTimer >= StealthHUDUpdateInterval)
	{
		StealthHUDUpdateTimer = 0.f;

		if (UStealthSubsystem* Sub = GetWorld()->GetSubsystem<UStealthSubsystem>())
		{
			for (const auto& Pair : Sub->GetActiveDetections())
			{
				if (AEnemyAIController* AICon = Cast<AEnemyAIController>(Pair.Key))
				{
					// Use the permanent cached body, because GetPawn() is null after death
					if (APawn* EnemyPawn = AICon->CachedUIBody)
					{
						// Only filter by the suspected actor while the meter is active (> 0).
						// A meter at 0 must bypass this check so the clear signal always goes through.
						if (Pair.Value.DetectionLevel > 0.f)
						{
							if (AICon->GetSuspectedActor() != GetPawn())
							{
								continue;
							}
						}

						Client_UpdateDetectionHUD(EnemyPawn, Pair.Value.DetectionLevel, Pair.Value.AlertPhase);
					}
				}
			}

			// Clean up the memory now that the overlay has received its updates
			Sub->RemoveZeroDetections();
		}
	}
}

#pragma region HUD
void ACorePlayerController::PollInit()
{
	// 1. Safety fetch the HUD in case BeginPlay was too early
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;

	// 2. Spawn the overlay
	if (ShooterHUD && ShooterHUD->CharacterOverlay == nullptr)
	{
		SetupInitialHUD();
	}

	// 3. Initialize the cached values once the overlay exists
	if (CharacterOverlay == nullptr)
	{
		if (ShooterHUD && ShooterHUD->CharacterOverlay)
		{
			CharacterOverlay = ShooterHUD->CharacterOverlay;

			if (GetPawn())
			{
				PushHUDFromPawn(GetPawn());
			}

			if (bInitializeCharacterOverlay)
			{
				SetHUDHealth(HUDHealth, HUDMaxHealth);
				bInitializeCharacterOverlay = false;
			}
			if (bInitializeCarriedAmmo)
			{
				SetHUDCarriedAmmo(HUDCarriedAmmo);
				bInitializeCarriedAmmo = false;
			}
			if (bInitializeWeaponAmmo)
			{
				SetHUDWeaponAmmo(HUDWeaponAmmo);
				bInitializeWeaponAmmo = false;
			}

			APlayerCharacter* PlayerCharacter = Cast<APlayerCharacter>(GetPawn());
			if (PlayerCharacter && PlayerCharacter->GetCombatComponent())
			{
				SetHUDGrenades(PlayerCharacter->GetCombatComponent()->GetGrenades());
			}
		}
	}
}

void ACorePlayerController::SetupInitialHUD()
{
	if (ShooterHUD && ShooterHUD->CharacterOverlay == nullptr)
	{
		ShooterHUD->AddCharacterOverlay();
	}
}

void ACorePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ABaseCharacter* BaseChar = Cast<ABaseCharacter>(InPawn);
	if (BaseChar)
	{
		if (BaseChar->GetCombatComponent())
		{
			SetHUDGrenades(BaseChar->GetCombatComponent()->GetGrenades());
		}
		SetHUDHealth(BaseChar->GetHealth(), BaseChar->GetMaxHealth());
	}
}

void ACorePlayerController::PushHUDFromPawn(APawn* InPawn)
{
	ABaseCharacter* BaseChar = Cast<ABaseCharacter>(InPawn);
	if (!BaseChar) return;

	// Health and medkits
	SetHUDHealth(BaseChar->GetHealth(), BaseChar->GetMaxHealth());
	SetHUDMedkits(BaseChar->GetMedkitCount());

	// Ammo and grenades
	UCombatComponent* Combat = BaseChar->GetCombatComponent();
	if (Combat)
	{
		SetHUDGrenades(Combat->GetGrenades());

		if (Combat->GetPrimaryWeapon())
		{
			SetHUDWeaponAmmo(Combat->GetPrimaryWeapon()->GetAmmo());
			SetHUDCarriedAmmo(Combat->CarriedAmmo);
		}
		else
		{
			SetHUDWeaponAmmo(0);
			SetHUDCarriedAmmo(0);
		}
	}

	// Spells / gadgets
	if (USpellComponent* SpellComp = BaseChar->FindComponentByClass<USpellComponent>())
	{
		SetHUDEnergy(SpellComp->CurrentEnergySegments, SpellComp->GetMaxEnergySegments());

		// Percent is 0..1, so passing 1.0 as the max makes the math line up
		SetHUDUltimate(SpellComp->GetUltimateBarPercent(), 1.0f);
	}
}

void ACorePlayerController::SetHUDHealth(float Health, float MaxHealth)
{
	// Always update the cache so PollInit has fresh data when it fires
	HUDHealth = Health;
	HUDMaxHealth = MaxHealth;

	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	const bool bHUDValid = ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->HealthBar && ShooterHUD->CharacterOverlay->HealthText;

	if (bHUDValid)
	{
		const float HealthPercent = Health / MaxHealth;
		ShooterHUD->CharacterOverlay->HealthBar->SetPercent(HealthPercent);
		const FString HealthText = FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth));
		ShooterHUD->CharacterOverlay->HealthText->SetText(FText::FromString(HealthText));
	}
	else
	{
		bInitializeCharacterOverlay = true;
	}
}

void ACorePlayerController::SetHUDWeaponAmmo(int32 Ammo)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	const bool bHUDValid = ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->WeaponAmmoAmount;
	if (bHUDValid)
	{
		const FString AmmoText = FString::Printf(TEXT("%d / "), Ammo);
		ShooterHUD->CharacterOverlay->WeaponAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeWeaponAmmo = true;
		HUDWeaponAmmo = Ammo;
	}

}

void ACorePlayerController::SetHUDCarriedAmmo(int32 Ammo)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	const bool bHUDValid = ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->CarriedAmmoAmount;
	if (bHUDValid)
	{
		const FString AmmoText = FString::Printf(TEXT("%d"), Ammo);
		ShooterHUD->CharacterOverlay->CarriedAmmoAmount->SetText(FText::FromString(AmmoText));
	}
	else
	{
		bInitializeCarriedAmmo = true;
		HUDCarriedAmmo = Ammo;
	}

}

void ACorePlayerController::SetHUDGrenades(int32 Grenades)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	const bool bHUDValid = ShooterHUD && ShooterHUD->CharacterOverlay && ShooterHUD->CharacterOverlay->GrenadesText;
	if (bHUDValid)
	{
		const FString GrenadesText = FString::Printf(TEXT("%d"), Grenades);
		ShooterHUD->CharacterOverlay->GrenadesText->SetText(FText::FromString(GrenadesText));
	}
	else
	{
		HUDGrenades = Grenades;
	}
}

void ACorePlayerController::SetHUDUltimate(float CurrentCharge, float MaxCharge)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay)
	{
		ShooterHUD->CharacterOverlay->SetUltimateCharge(CurrentCharge, MaxCharge);
	}
}

void ACorePlayerController::SetHUDEnergy(int32 CurrentSegments, int32 MaxSegments)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay)
	{
		ShooterHUD->CharacterOverlay->SetEnergySegments(CurrentSegments, MaxSegments);
	}
}

void ACorePlayerController::SetHUDMedkits(int32 Medkits)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay)
	{
		ShooterHUD->CharacterOverlay->SetHUDMedkits(Medkits);
	}
}

void ACorePlayerController::Client_ShowHitMarker(bool bHeadshot)
{
	if (ShooterHUD && ShooterHUD->CharacterOverlay)
	{
		ShooterHUD->CharacterOverlay->ShowHitMarker(bHeadshot);
	}

	if (HitMarkerSound)
	{
		UGameplayStatics::PlaySound2D(this, HitMarkerSound);
	}
}

void ACorePlayerController::Client_UpdateDetectionHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw)
{
	ShooterHUD = ShooterHUD == nullptr ? Cast<AShooterHUD>(GetHUD()) : ShooterHUD;
	if (ShooterHUD && ShooterHUD->CharacterOverlay)
	{
		// Pass the enemy into the overlay so it can work out the direction angle
		ShooterHUD->CharacterOverlay->UpdateStealthHUD(DetectingEnemy, DetectionLevel, AlertPhaseRaw);
	}
}
#pragma endregion HUD

#pragma region Mission
void ACorePlayerController::ClientNotifyMissionState_Implementation(FName State)
{
	// Intentionally empty. Implement the victory/defeat UI in Blueprint.
}

void ACorePlayerController::ShowReturnToMainMenu()
{
	if (ReturnToMainMenuWidget == nullptr) return;

	if (ReturnToMainMenu == nullptr)
	{
		ReturnToMainMenu = CreateWidget<UReturnToMainMenu>(this, ReturnToMainMenuWidget);
	}

	if (ReturnToMainMenu)
	{
		if (!ReturnToMainMenu->IsInViewport())
		{
			ReturnToMainMenu->MenuSetup();
		}
		else
		{
			ReturnToMainMenu->MenuTearDown();
		}
	}
}

void ACorePlayerController::ServerRestartLevel()
{
	GetWorld()->ServerTravel("?Restart", false);
}

void ACorePlayerController::ServerRestartCheckpoint()
{
	if (AMissionGameMode* GM = Cast<AMissionGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->ForceRestartCheckpoint();
	}
}
#pragma endregion Mission

#pragma region Inputs
void ACorePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Movement and camera
		if (MoveAction) Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACorePlayerController::MoveTriggered);
		if (LookAction) Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACorePlayerController::LookTriggered);
		if (JumpAction)
		{
			Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACorePlayerController::JumpButtonPressed);
			Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACorePlayerController::JumpButtonReleased);
		}
		if (CrouchAction)
		{
			Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &ACorePlayerController::CrouchButtonPressed);
			Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ACorePlayerController::CrouchButtonReleased);
		}
		if (SprintAction)
		{
			Input->BindAction(SprintAction, ETriggerEvent::Started, this, &ACorePlayerController::SprintButtonPressed);
			Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ACorePlayerController::SprintButtonReleased);
			Input->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ACorePlayerController::SprintButtonReleased);
		}

		// Combat, gadgets and UI
		if (InteractAction)
		{
			Input->BindAction(InteractAction, ETriggerEvent::Started, this, &ACorePlayerController::InteractButtonPressed);
			Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &ACorePlayerController::InteractButtonReleased);
		}
		if (SwapWeaponAction)       Input->BindAction(SwapWeaponAction, ETriggerEvent::Triggered, this, &ACorePlayerController::SwapButtonPressed);
		if (ReloadAction)           Input->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &ACorePlayerController::ReloadButtonPressed);
		if (ThrowGrenadeAction)     Input->BindAction(ThrowGrenadeAction, ETriggerEvent::Triggered, this, &ACorePlayerController::ThrowGrenadeButtonPressed);
		if (DropAction)             Input->BindAction(DropAction, ETriggerEvent::Triggered, this, &ACorePlayerController::DropButtonPressed);
		if (AimAction)
		{
			Input->BindAction(AimAction, ETriggerEvent::Started, this, &ACorePlayerController::AimButtonPressed);
			Input->BindAction(AimAction, ETriggerEvent::Completed, this, &ACorePlayerController::AimButtonReleased);
			Input->BindAction(AimAction, ETriggerEvent::Canceled, this, &ACorePlayerController::AimButtonReleased); // Hold/Tap triggers end this way
		}
		if (FireAction)
		{
			Input->BindAction(FireAction, ETriggerEvent::Started, this, &ACorePlayerController::FireButtonPressed);
			Input->BindAction(FireAction, ETriggerEvent::Completed, this, &ACorePlayerController::FireButtonReleased);
		}
		if (MeleeAction)            Input->BindAction(MeleeAction, ETriggerEvent::Triggered, this, &ACorePlayerController::MeleeButtonPressed);
		if (PauseAction)            Input->BindAction(PauseAction, ETriggerEvent::Triggered, this, &ACorePlayerController::ShowReturnToMainMenu);
		if (WeaponWheelAction)
		{
			Input->BindAction(WeaponWheelAction, ETriggerEvent::Started, this, &ACorePlayerController::WeaponWheelStarted);
			Input->BindAction(WeaponWheelAction, ETriggerEvent::Completed, this, &ACorePlayerController::WeaponWheelReleased);
		}
		if (ToggleSuppressorAction) Input->BindAction(ToggleSuppressorAction, ETriggerEvent::Triggered, this, &ACorePlayerController::ToggleSuppressorButtonPressed);
		if (ThrowDistractionAction) Input->BindAction(ThrowDistractionAction, ETriggerEvent::Triggered, this, &ACorePlayerController::ThrowDistractionButtonPressed);
		if (DashAction)             Input->BindAction(DashAction, ETriggerEvent::Started, this, &ACorePlayerController::DashButtonPressed);
		if (ConsumeMedkitAction)    Input->BindAction(ConsumeMedkitAction, ETriggerEvent::Started, this, &ACorePlayerController::ConsumeMedkitButtonPressed);
		if (Spell1Action)           Input->BindAction(Spell1Action, ETriggerEvent::Started, this, &ACorePlayerController::Spell1ButtonPressed);
		if (Spell2Action)
		{
			Input->BindAction(Spell2Action, ETriggerEvent::Started, this, &ACorePlayerController::Spell2ButtonPressed);
			Input->BindAction(Spell2Action, ETriggerEvent::Completed, this, &ACorePlayerController::Spell2ButtonReleased);
		}
		if (UltimateAction)         Input->BindAction(UltimateAction, ETriggerEvent::Started, this, &ACorePlayerController::UltimateButtonPressed);
		Input->BindAction(GroundSmashAction, ETriggerEvent::Started, this, &ACorePlayerController::GroundSmashButtonPressed);
	}
}

void ACorePlayerController::MoveTriggered(const FInputActionValue& Value)
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->MoveInput(Value.Get<FVector2D>());
	}
}

void ACorePlayerController::LookTriggered(const FInputActionValue& Value)
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->LookInput(Value.Get<FVector2D>());
	}
}

void ACorePlayerController::JumpButtonPressed()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn())) PlayerChar->JumpPressed();
}

void ACorePlayerController::JumpButtonReleased()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn())) PlayerChar->JumpReleased();
}

void ACorePlayerController::CrouchButtonPressed()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn())) PlayerChar->CrouchPressed();
}

void ACorePlayerController::CrouchButtonReleased()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn())) PlayerChar->CrouchReleased();
}

void ACorePlayerController::SprintButtonPressed()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn())) PlayerChar->SprintPressed();
}

void ACorePlayerController::SprintButtonReleased()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn())) PlayerChar->SprintReleased();
}

void ACorePlayerController::FireButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->HandleFire(true);
		}
	}
}

void ACorePlayerController::FireButtonReleased()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->HandleFire(false);
		}
	}
}

void ACorePlayerController::AimButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			if (Combat->bLocallyReloading) return;
			Combat->SetAiming(true);

			if (Combat->GetPrimaryWeapon())
			{
				// A sniper has its own (slower) aim sensitivity
				BaseChar->ActiveMouseSensitivity = (Combat->GetPrimaryWeapon()->GetWeaponType() == EWeaponType::EWT_SniperRifle)
					? BaseChar->SniperAimSens
					: BaseChar->DefaultAimSensitivity;
			}
		}
	}
}

void ACorePlayerController::AimButtonReleased()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->SetAiming(false);

BaseChar->ActiveMouseSensitivity = BaseChar->BaseMouseSensitivity;
		}
	}
}

void ACorePlayerController::ReloadButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->Reload();
		}
	}
}

void ACorePlayerController::SwapButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->EquippedSwap();
		}
	}
}

void ACorePlayerController::ThrowGrenadeButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->ThrowGrenade();
		}
	}
}

void ACorePlayerController::ThrowDistractionButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->ThrowDistraction();
		}
	}
}

void ACorePlayerController::MeleeButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->Melee();
		}
	}
}

void ACorePlayerController::DropButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			Combat->ServerDropWeapon();
		}
	}
}

void ACorePlayerController::InteractButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UInteractionComponent* InteractComp = BaseChar->FindComponentByClass<UInteractionComponent>())
		{
			InteractComp->TryInteract();
		}
	}
}

void ACorePlayerController::InteractButtonReleased()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (UInteractionComponent* InteractComp = BaseChar->FindComponentByClass<UInteractionComponent>())
		{
			InteractComp->TryInteractReleased();
		}
	}
}

void ACorePlayerController::DashButtonPressed()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (PlayerChar->bDisableGameplay) return;
		PlayerChar->DashPressed();
	}
}

void ACorePlayerController::GroundSmashButtonPressed()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (PlayerChar->bDisableGameplay) return;
		PlayerChar->GroundSmashPressed();
	}
}

void ACorePlayerController::ConsumeMedkitButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;
		BaseChar->ConsumeMedkit();
	}
}

void ACorePlayerController::Spell1ButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->TryCastSpell1();
		}
	}
}

void ACorePlayerController::Spell2ButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->TryCastSpell2();
		}
	}
}

void ACorePlayerController::Spell2ButtonReleased()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->ReleaseSpell2();
		}
	}
}

void ACorePlayerController::UltimateButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (BaseChar->bDisableGameplay) return;

		if (USpellComponent* Spell = BaseChar->FindComponentByClass<USpellComponent>())
		{
			Spell->TryCastUltimate();
		}
	}
}

void ACorePlayerController::ToggleSuppressorButtonPressed()
{
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		if (UCombatComponent* Combat = BaseChar->GetCombatComponent())
		{
			if (AWeapon* PrimaryWeapon = Combat->GetPrimaryWeapon())
			{
				if (!PrimaryWeapon->HasSuppressor()) return; // this weapon type can't take a suppressor
				PrimaryWeapon->ToggleSuppressor();
			}
		}
	}
}

void ACorePlayerController::WeaponWheelStarted()
{
	ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn());
	if (!BaseChar || BaseChar->bDisableGameplay || !WeaponWheelClass) return;

	UCombatComponent* CombatComp = BaseChar->GetCombatComponent();
	if (CombatComp) CombatComp->bShowCrosshairs = false;

	if (!WeaponWheelInstance)
	{
		WeaponWheelInstance = CreateWidget<UWeaponRadialMenu>(this, WeaponWheelClass);
	}

	if (WeaponWheelInstance && !WeaponWheelInstance->IsInViewport())
	{
		// Give the UI a reference to the body so it can read the inventory
		WeaponWheelInstance->BaseCharacter = BaseChar;

		WeaponWheelInstance->AddToViewport();

		SetShowMouseCursor(true);
		SetInputMode(FInputModeGameAndUI());

		int32 ViewX, ViewY;
		GetViewportSize(ViewX, ViewY);
		SetMouseLocation(ViewX / 2, ViewY / 2);
	}
}

void ACorePlayerController::WeaponWheelReleased()
{
	UCombatComponent* CombatComp = nullptr;
	if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(GetPawn()))
	{
		CombatComp = BaseChar->GetCombatComponent();
		if (CombatComp) CombatComp->bShowCrosshairs = true;
	}

	if (WeaponWheelInstance && WeaponWheelInstance->IsInViewport())
	{
		// Equip the weapon the player picked
		if (WeaponWheelInstance->HoveredWeapon && CombatComp && !WeaponWheelInstance->bJustEquippedToSecondary)
		{
			CombatComp->ServerEquipFromMenuToPrimary(WeaponWheelInstance->HoveredWeapon);
		}

		WeaponWheelInstance->RemoveFromParent();

		SetShowMouseCursor(false);
		SetInputMode(FInputModeGameOnly());
	}
}
#pragma endregion Inputs
