
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MovementShooter/GameVariables/WeaponType.h"
#include "MovementShooter/Interface/AnimInterface.h"
#include "BaseAnimInstance.generated.h"

class IAnimInterface;
/**
 * 
 */
UCLASS()
class MOVEMENTSHOOTER_API UBaseAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    virtual void NativeInitializeAnimation() override;

    virtual void TriggerHitReact(const FVector& HitWorldDirection, FName HitBone);

protected:
    // What type of Weapon Does Player have?
    UPROPERTY(BlueprintReadOnly)
    EWeaponType EquippedWeaponType;

    // Holds the current weapon's idle animation to feed into the AnimGraph
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Animation")
    class UAnimSequence* CurrentWeaponIdlePose;
    
    // Is Player Aiming?
    UPROPERTY(BlueprintReadWrite)
    bool bAiming = false;

    // Is Player Firing?
    UPROPERTY(BlueprintReadOnly)
    bool bFiring = false;

    UPROPERTY(BlueprintReadOnly)
    bool bUseFABRIK;
    
    UPROPERTY(BlueprintReadOnly)
    bool bUseAimOffsets;

    UPROPERTY(BlueprintReadOnly)
    bool bTransformRightHand;

    UPROPERTY(BlueprintReadOnly)
    FTransform LeftHandTransform;
    
    UPROPERTY(BlueprintReadOnly)
    FRotator RightHandRotation;

    UPROPERTY(BlueprintReadOnly)
    bool bLocallyControlled;
    
    FRotator CharacterRotationLastFrame = FRotator::ZeroRotator;
    FRotator CharacterRotation = FRotator::ZeroRotator;
    FRotator DeltaRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly)
    float AO_Yaw;

    UPROPERTY(BlueprintReadOnly)
    float AO_Pitch;

    UPROPERTY(BlueprintReadOnly)
    float YawOffset;

    // --- LOCOMOTION (feed these into the blend spaces) ---
    // Horizontal speed in units per second
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float Speed = 0.f;

    // Movement direction relative to where the character faces, -180 to 180
    // (0 forward, 90 right, -90 left, 180 backward). Stays 0 while standing still.
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float Direction = 0.f;

    // How fast the character is turning, in degrees per second (positive = turning right). Handy for arm sway.
    UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
    float YawRate = 0.f;

    UPROPERTY(BlueprintReadOnly)
    bool  bAlignedToAim;

    UPROPERTY(BlueprintReadOnly)
    bool bOneHandedWeapon;
    
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    FVector TraceHitTarget;
    
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bShouldLookAtTarget;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    float LookAtAlpha = 0.f;

    // The speed at which the character lowers/raises their weapon when looking too far
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float LookAtAlphaInterpSpeed = 5.f;
    
    // Is Player Downed?
    UPROPERTY(BlueprintReadOnly, Category = "State")
    bool bIsDowned = false;

    // --- HIT REACT VARIABLES ---
    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    FVector LocalHitDirection;

    // How fast the character recovers from the flinch
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat|HitReact")
    float HitFlinchDecaySpeed = 6.f;
    
    // The final, mathematically calculated rotation to feed directly into the AnimGraph
    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    FRotator CalculatedHitRotation;

    //  Dedicated Rotator just for the arms
    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    FRotator ArmCalculatedHitRotation;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    float HitIntensity = 0.f;

    //Zone-specific intensities
    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    float SpineHitIntensity = 0.001f;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    float HeadHitIntensity = 0.001f;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    float LeftArmHitIntensity = 0.001f;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|HitReact")
    float RightArmHitIntensity = 0.001f;
    
    // Timer to track how long since we last shot or aimed
    float TimeSinceLastWeaponAction = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    bool bIsWallRunning = false;

    UPROPERTY(BlueprintReadOnly, Category = "Traversal")
    bool bIsRightWall = false;

    // Tells the AnimGraph if Foot IK traces should be running
    UPROPERTY(BlueprintReadOnly, Category = "IK")
    bool bEnableFootIK = true;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Recoil")
    FRotator VisualRecoil;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Recoil")
    FVector VisualRecoilLocation;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsMeleeing = false;

    // --- AI COVER STATE ---
    UPROPERTY(BlueprintReadOnly, Category = "AI|Cover")
    bool bIsInCover = false;

    UPROPERTY(BlueprintReadOnly, Category = "AI|Cover")
    bool bCoverRight = false;
    
private:
    UPROPERTY()
    TScriptInterface<IAnimInterface> AnimOwner;

    FRotator TargetHitRotation = FRotator::ZeroRotator;
    FRotator ArmTargetHitRotation = FRotator::ZeroRotator;
    
};

