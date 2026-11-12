#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterOverlay.generated.h"

class UCanvasPanel;
class UDirectionalIndicatorWidget;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
struct FDialogueEntry;

// The in-game HUD. Removed from the old overlay: the squad/companion list, the downed/revive text,
// the mend cooldown and the high-ping icon (they only existed for the co-op game).
UCLASS()
class MOVEMENTSHOOTER_API UCharacterOverlay : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void UpdateInteractionPrompt(bool bShow, const FString& PromptText);

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealthText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* WeaponAmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CarriedAmmoAmount;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* GrenadesText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* MedkitCountText;

	void SetHUDMedkits(int32 MedkitCount);

	UPROPERTY(meta = (BindWidget))
	UImage* HitMarkerImage;

	UFUNCTION(BlueprintCallable)
	void ShowHitMarker(bool bHeadshot);

	FTimerHandle HitMarkerTimer;

	// --- Subtitles ---
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* SpeakerText;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* SubtitleText;

	UFUNCTION()
	void UpdateSubtitle(const FDialogueEntry& Entry);

	UFUNCTION()
	void ClearSubtitle();

	// --- Interaction prompt ---
	UPROPERTY(meta = (BindWidget))
	UTextBlock* InteractionText;

	// --- Stealth detection indicators ---
	UPROPERTY(meta = (BindWidgetOptional))
	UCanvasPanel* DetectionCanvas;

	UPROPERTY(EditDefaultsOnly, Category = "Stealth")
	TSubclassOf<UDirectionalIndicatorWidget> DirectionalIndicatorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Stealth")
	float IndicatorRadius = 250.f;

	UPROPERTY()
	TMap<AActor*, UDirectionalIndicatorWidget*> ActiveStealthIndicators;

	float CalculateDirectionalUIAngle(APlayerController* PC, AActor* EnemyActor);

	/**
	 * Called by the player controller (Client_UpdateDetectionHUD).
	 * @param DetectionLevel  0.0 - 1.0 (the enemy's meter fill)
	 * @param AlertPhaseRaw   uint8 cast of EAlertPhase (0=Unaware ... 3=Alert)
	 */
	void UpdateStealthHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void SetAmmoUIVisibility(bool bVisible);

	// --- Gadgets / spells ---
	UPROPERTY(meta = (BindWidgetOptional))
	UProgressBar* UltimateBar;

	UPROPERTY(meta = (BindWidgetOptional))
	UHorizontalBox* EnergySegmentBox;

	UPROPERTY(EditDefaultsOnly, Category = "Spells|Energy")
	FSlateColor EnergyFullColor = FSlateColor(FLinearColor(0.0f, 0.8f, 1.0f, 1.0f));

	UPROPERTY(EditDefaultsOnly, Category = "Spells|Energy")
	FSlateColor EnergyEmptyColor = FSlateColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f));

	void SetUltimateCharge(float CurrentCharge, float MaxCharge);
	void SetEnergySegments(int32 CurrentSegments, int32 MaxSegments);
};
