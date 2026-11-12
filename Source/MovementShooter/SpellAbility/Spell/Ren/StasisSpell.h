
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/SpellAbility/SpellBase.h"
#include "StasisSpell.generated.h"

class AWeapon;
class AHitScanWeapon;

UCLASS()
class MOVEMENTSHOOTER_API UStasisSpell : public USpellBase
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(ABaseCharacter* Caster) override;

	void TickStasis(float RealDeltaSeconds);
	bool IsActive() const { return bStasisActive; }

	void LocalActivate(ABaseCharacter* Caster, float Duration);
	void LocalDeactivate();

	UPROPERTY()
	TArray<TWeakObjectPtr<AWeapon>> FlaggedWeapons;

	void RegisterWeaponForStasis(AWeapon* NewWeapon);
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Stasis")
	float StasisDuration = 5.f;

	// ==========================================
	// NEW — BULLET TIME
	// ==========================================
	// The world/global time dilation applied on activate. This is what slows enemies, projectiles,
	// barrels, timers, etc. The caster alone gets CustomTimeDilation = 1 / this value so they stay at normal speed.
	UPROPERTY(EditDefaultsOnly, Category = "Stasis|BulletTime")
	float BulletTimeGlobalDilation = 0.4f;

	// ==========================================
	// AUDIO
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Audio")
	class USoundBase* CastSound;

	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Audio")
	class USoundBase* LoopSound;

	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Audio")
	class USoundBase* EndSound;

	// ==========================================
	// VISUAL FX
	// ==========================================
	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Visuals")
	FPostProcessSettings BulletTimePostProcess;

	// How fast the screen effect fades in
	UPROPERTY(EditDefaultsOnly, Category = "Stasis|Visuals")
	float PostProcessFadeSpeed = 8.0f;
	
private:
	UPROPERTY()
	ABaseCharacter* CachedCaster = nullptr;

	float ElapsedRealTime = 0.f;
	bool bStasisActive = false;

	FVector StasisOrigin;

	UPROPERTY()
	class UAudioComponent* ActiveLoopSound;

	float CurrentPPWeight = 0.0f;
};
