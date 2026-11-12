
// Fill out your copyright notice in the Description page of Project Settings.


#include "MovementShooter/Animation/BaseAnimInstance.h"

#include "AIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MovementShooter/Component/ShooterMovementComponent.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/Component/CombatComponent.h"
#include "MovementShooter/GameVariables/CombatState.h"
#include "MovementShooter/Interface/AnimInterface.h"
#include "MovementShooter/Interface/CombatInterface.h"
#include "Kismet/KismetMathLibrary.h"


void UBaseAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    APawn* PawnOwner = TryGetPawnOwner();
    if (PawnOwner && PawnOwner->GetClass()->ImplementsInterface(UAnimInterface::StaticClass()))
    {
       AnimOwner.SetObject(PawnOwner);
       AnimOwner.SetInterface(Cast<IAnimInterface>(PawnOwner));
    }

   
}

void UBaseAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (!AnimOwner) return;

    APawn* Pawn = Cast<APawn>(AnimOwner.GetObject());
    if (!Pawn) return;

    UCombatComponent* CombatComp = IAnimInterface::Execute_GetCombatComponent(Pawn);
    USkeletalMeshComponent* CombatMesh = ICombatInterface::Execute_GetCombatMesh(Pawn);
    
    if (!CombatComp || !CombatMesh) return;

    // Fetch the specific idle pose from the equipped weapon!
    if (Pawn->Implements<UAnimInterface>())
    {
       if (AWeapon* PrimaryWeapon = IAnimInterface::Execute_GetPrimaryWeapon(Pawn))
       {
          CurrentWeaponIdlePose = PrimaryWeapon->CustomIdlePose;
       }
       else
       {
          CurrentWeaponIdlePose = nullptr; // Clear it if we drop the weapon
       }
    }

   const bool bGameplayDisabled = IAnimInterface::Execute_GetIsGameplayDisbled(Pawn);

   if (bIsDowned || bGameplayDisabled)
   {
      // Force off all aiming, IK, and offsets when downed or dead!
      bUseFABRIK = false;
      bShouldLookAtTarget = false;
      bUseAimOffsets = false;
      bTransformRightHand = false;
      bEnableFootIK = false;
      LookAtAlpha = 0.f;
   }
   
    EquippedWeaponType = IAnimInterface::Execute_GetEquippedWeaponType(Pawn);
    bAiming = IAnimInterface::Execute_GetAiming(Pawn);
    
    bFiring = IAnimInterface::Execute_GetIsFiring(Pawn);
    bIsWallRunning = IAnimInterface::Execute_GetIsWallRunning(Pawn);
    bIsRightWall = IAnimInterface::Execute_GetIsRightWall(Pawn);

    // === FOOT IK LOGIC ===
    bool bIsInAir = false;
    bool bIsDashing = false;
    bool bIsTraversing = false;
    bool bIsDiving = false;
   

    if (ABaseCharacter* BaseChar = Cast<ABaseCharacter>(Pawn))
    {
       // Dodge/dive was removed from the project.
       bIsDiving = false;

       // 1. In-air check straight from CharacterMovement
       bIsInAir = BaseChar->GetCharacterMovement()->IsFalling();

       // 2. Dashing, reported by the movement component
       if (const UShooterMovementComponent* ShooterMove = Cast<UShooterMovementComponent>(BaseChar->GetCharacterMovement()))
       {
          bIsDashing = ShooterMove->IsDashing();
       }
    }

   bOneHandedWeapon = EquippedWeaponType == EWeaponType::EWT_SubmachineGun || EquippedWeaponType == EWeaponType::EWT_Pistol || bIsDiving ;

   
    // 3. Check Traversing
    // Your CombatComponent already tracks bIsTraversing, so we can pull it from there!
    if (CombatComp)
    {
       bIsTraversing = CombatComp->GetIsTraversing();
       bIsMeleeing = CombatComp->GetCombatState() == ECombatState::ECS_Melee;
    }

    // 4. Final Evaluation
    // Turn off Foot IK if doing any extreme movement
    if (bIsInAir || bIsWallRunning || bIsDashing || bIsTraversing)
    {
       bEnableFootIK = false;
    }
    else
    {
       bEnableFootIK = true;
    }
    
    //  Calculate exactly how much time has passed since the Multicast registered a shot
    float TimeSinceAction = GetWorld()->GetTimeSeconds() - CombatComp->LastWeaponActionTime;
   
    
       // LEFT HAND IK (FABRIK)
       if (EquippedWeaponType != EWeaponType::EWT_EmptyHand && IAnimInterface::Execute_GetPrimaryWeapon(Pawn)->GetItemMesh())
       {
          USkeletalMeshComponent* WeaponMesh = IAnimInterface::Execute_GetPrimaryWeapon(Pawn)->GetItemMesh();
   
          // Each character picks its own socket on the weapon
          FName LeftHandSocketName = FName("LeftHandSocket");
          if (const ABaseCharacter* OwnerChar = Cast<ABaseCharacter>(Pawn))
          {
             LeftHandSocketName = OwnerChar->GetLeftHandSocketName();
          }
          // Weapon has no socket for this character: use the default one
          if (!WeaponMesh->DoesSocketExist(LeftHandSocketName))
          {
             LeftHandSocketName = FName("LeftHandSocket");
          }
   
          // 1. Get both transforms in World Space just ONCE to figure out the math
          FTransform SocketWorld = WeaponMesh->GetSocketTransform(LeftHandSocketName, RTS_World);
          FTransform HandWorld = CombatMesh->GetSocketTransform(FName("hand_r"), RTS_World);
   
          // 2. Calculate the RELATIVE difference between them.
          LeftHandTransform = SocketWorld.GetRelativeTransform(HandWorld);
       }
   
    // LOCOMOTION & AIM OFFSETS
    FRotator AimRotation = Pawn->GetBaseAimRotation();
    FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(Pawn->GetVelocity());
    FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
    DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f);
    YawOffset = DeltaRotation.Yaw;
    
    CharacterRotationLastFrame = CharacterRotation;
    CharacterRotation = Pawn->GetActorRotation();

    // Locomotion values for the blend spaces
    const FVector MoveVelocity = Pawn->GetVelocity();
    Speed = MoveVelocity.Size2D();
    if (Speed > 3.f)
    {
       const FRotator MoveRotation = UKismetMathLibrary::MakeRotFromX(FVector(MoveVelocity.X, MoveVelocity.Y, 0.f));
       Direction = UKismetMathLibrary::NormalizedDeltaRotator(MoveRotation, CharacterRotation).Yaw;
    }
    else
    {
       Direction = 0.f;
    }
    YawRate = DeltaSeconds > KINDA_SMALL_NUMBER
       ? FRotator::NormalizeAxis(CharacterRotation.Yaw - CharacterRotationLastFrame.Yaw) / DeltaSeconds
       : 0.f;
AO_Yaw = IAnimInterface::Execute_GetAOYaw(Pawn);
    AO_Pitch = IAnimInterface::Execute_GetAOPitch(Pawn);

    float CameraYaw  = Pawn->GetBaseAimRotation().Yaw;
    float CharYaw    = Pawn->GetActorRotation().Yaw;
    float AimDelta   = FMath::Abs(FRotator::NormalizeAxis(CameraYaw - CharYaw));
    bAlignedToAim = AimDelta < CombatComp->AimAlignThreshold;
    
    // --- RIGHT HAND AIMING (PREPPING FOR 'LOOK AT' NODE) ---
    
    // 1. Grab the raw target from the Combat Component
    TraceHitTarget = CombatComp->GetHitTarget();
    
   // 2. Determine who owns this body
   bool bIsAI = !Pawn->IsPlayerControlled();
   bool bIsRemoteHuman = !Pawn->IsLocallyControlled() && !bIsAI;

   // === AI COVER STATE ===
   // Default to false every frame, then overwrite if it is an AI with a valid Blackboard
   bIsInCover = false;
   bCoverRight = false;

   if (bIsAI)
   {
      if (AAIController* AICon = Cast<AAIController>(Pawn->GetController()))
      {
         if (UBlackboardComponent* Blackboard = AICon->GetBlackboardComponent())
         {
            bIsInCover = Blackboard->GetValueAsBool(FName("IsInCover"));
            bCoverRight = Blackboard->GetValueAsBool(FName("CoverRight"));
         }
      }
   }

   // 3. THE FALLBACK LOGIC
   if (TraceHitTarget.IsNearlyZero() || bIsRemoteHuman)
    {
       FVector HeadLocation = Pawn->GetActorLocation() + FVector(0.f, 0.f, 70.f); 
       FVector AimDir = bIsAI ? Pawn->GetActorForwardVector() : Pawn->GetBaseAimRotation().Vector();
       TraceHitTarget = HeadLocation + (AimDir * 5000.f);
    }
    
    if (bLocallyControlled)
    {
       // 1. Local Player uses their exact pixel-perfect crosshair trace
       TraceHitTarget = CombatComp->GetHitTarget(); 
    }
    else
    {
       // 2. Replicate the aiming for Server & Remote Clients!
       FVector HeadLocation = Pawn->GetActorLocation() + FVector(0.f, 0.f, 70.f); 
       TraceHitTarget = HeadLocation + (Pawn->GetBaseAimRotation().Vector() * 5000.f);
    }

    // --- CHECK DOWNED STATE ---
    bIsDowned = false; // revive/downed system was removed

    // --- OVERRIDE LOGIC IF DOWNED ---
    if (bIsDowned)
    {
        // Force off all aiming, IK, and offsets while crawling!
        bUseFABRIK = false;
        bShouldLookAtTarget = false;
        bUseAimOffsets = false;
        bTransformRightHand = false;
        LookAtAlpha = FMath::FInterpTo(LookAtAlpha, 0.f, DeltaSeconds, LookAtAlphaInterpSpeed);
    }
    else
    {
       //LEFT HAND LOGIC: Stays active 100% of the time
       bUseFABRIK = CombatComp->GetCombatState() == ECombatState::ECS_Unoccupied;

       // Override logic for Left Hand
       bool bFABRIKOverride = Pawn->IsLocallyControlled() && CombatComp->GetCombatState() !=
           ECombatState::ECS_ThrowingGrenade && CombatComp->bFinishedSwapping;

        if (bFABRIKOverride)
        {
            bUseFABRIK = !CombatComp->bLocallyReloading;
        }

        // RIGHT HAND LOGIC: Always track target while aiming OR mid-dive!
        bShouldLookAtTarget = (bUseFABRIK && bAlignedToAim) || bIsDiving;

        //Convert that boolean into a smoothly interpolating float
        float TargetAlpha = bShouldLookAtTarget ? 1.f : 0.f;

        // Interpolate towards the target.
        LookAtAlpha = FMath::FInterpTo(LookAtAlpha, TargetAlpha, DeltaSeconds, LookAtAlphaInterpSpeed);

        // Aim Offsets & Transform flags
        bUseAimOffsets = CombatComp->GetCombatState() == ECombatState::ECS_Unoccupied && !IAnimInterface::Execute_GetIsGameplayDisbled(Pawn);
        bTransformRightHand = CombatComp->GetCombatState() == ECombatState::ECS_Unoccupied && !IAnimInterface::Execute_GetIsGameplayDisbled(Pawn);
    }

    // Grab the visual recoil from the combat component
    if (Pawn->IsLocallyControlled())
    {
       bool bIsFlamethrower = false;
       if (Pawn->Implements<UAnimInterface>())
       {
          if (AWeapon* PrimaryWeapon = IAnimInterface::Execute_GetPrimaryWeapon(Pawn))
          {
             bIsFlamethrower = PrimaryWeapon->GetWeaponCategory() == EWeaponCategory::EWC_Special;
          }
       }

       // Zero out visual recoil for the flamethrower so the stream stays perfectly smooth
       if (bIsFlamethrower)
       {
          VisualRecoil = FRotator::ZeroRotator;
          VisualRecoilLocation = FVector::ZeroVector;
       }
       else
       {
          VisualRecoil = CombatComp->GetVisualRecoil();
          VisualRecoilLocation = CombatComp->GetVisualRecoilLocation();
       }
    }

    // --- HIT REACT DECAY ---
    // Decay Spine
    if (SpineHitIntensity > 0.001f)
    {
       SpineHitIntensity = FMath::FInterpTo(SpineHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
    }
    // Decay Head
    if (HeadHitIntensity > 0.001f)
    {
       HeadHitIntensity = FMath::FInterpTo(HeadHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
    }
    if (LeftArmHitIntensity > 0.001f)
    {
       LeftArmHitIntensity = FMath::FInterpTo(LeftArmHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
    }
    if (RightArmHitIntensity > 0.001f)
    {
       RightArmHitIntensity = FMath::FInterpTo(RightArmHitIntensity, 0.001f, DeltaSeconds, HitFlinchDecaySpeed);
    }
    
    // Note: We leave CalculatedHitRotation at full strength. 
    // The Alpha pins on the Spring Controllers will handle scaling it down!
    CalculatedHitRotation = TargetHitRotation;
    
    // Pass the inverted arm data
    ArmCalculatedHitRotation = ArmTargetHitRotation;
}

void UBaseAnimInstance::TriggerHitReact(const FVector& HitWorldDirection, FName HitBone)
{
    APawn* Pawn = TryGetPawnOwner();
    if (!Pawn) return;

    // ==========================================
    // THE FLAMETHROWER FILTER (EARLY EXIT)
    // ==========================================
    if (Pawn->Implements<UAnimInterface>())
    {
       if (AWeapon* PrimaryWeapon = IAnimInterface::Execute_GetPrimaryWeapon(Pawn))
       {
          if (PrimaryWeapon->GetWeaponCategory() == EWeaponCategory::EWC_Special)
          {
             return; // Kills the function here. No flinch while holding the heavy flamethrower!
          }
       }
    }
    
    // 1. Grab the bone name immediately
    FString BoneString = HitBone.ToString().ToLower();

    // ==========================================
    // THE LEG FILTER (EARLY EXIT)
    // ==========================================
    if (BoneString.Contains("thigh") || BoneString.Contains("calf") || 
       BoneString.Contains("foot")  || BoneString.Contains("pelvis") || 
       BoneString.Contains("leg"))
    {
       return; // Kills the function here. No intensities get spiked!
    }

    // 2. THE BASE MATH
    FVector LocalHitDir = Pawn->GetActorTransform().InverseTransformVectorNoScale(HitWorldDirection);
    
    float PitchAngle = LocalHitDir.Y * 60.f; 
    float RollAngle  = LocalHitDir.X * 60.f;  
    float YawAngle   = 0.f; // Default to 0

    // 3. THE SELECTIVE ZONE ROUTER
    if (BoneString.Contains("head") || BoneString.Contains("neck"))
    {
       HeadHitIntensity = 1.0f; 
    }
    // --- LEFT SIDE ---
    else if (BoneString.Contains("clavicle_l") || BoneString.Contains("hand_l"))
    {
       LeftArmHitIntensity = 1.0f; 
       SpineHitIntensity = 0.5f; // Wake up the spine!
       YawAngle = LocalHitDir.X * 45.f; // Apply the left twist!
    }
    else if (BoneString.Contains("arm_l")) 
    {
       LeftArmHitIntensity = 1.0f; // Pure arm flinch, NO twist!
    }
    // --- RIGHT SIDE ---
    else if (BoneString.Contains("clavicle_r") || BoneString.Contains("hand_r"))
    {
       RightArmHitIntensity = 1.0f; 
       SpineHitIntensity = 0.5f; // Wake up the spine!
       YawAngle = LocalHitDir.X * -45.f; // Apply the right twist!
    }
    else if (BoneString.Contains("arm_r"))
    {
       RightArmHitIntensity = 1.0f; // Pure arm flinch, NO twist!
    }
    else
    {
       SpineHitIntensity = 1.0f; // Pure bodyshot
    }

    // 4. SET THE TARGETS
    TargetHitRotation = FRotator(PitchAngle, YawAngle, RollAngle);
    ArmTargetHitRotation = FRotator(-PitchAngle, 0.f, -RollAngle);
}

