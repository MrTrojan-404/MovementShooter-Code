
// Fill out your copyright notice in the Description page of Project Settings.

#include "MovementShooter/UI/Widgets/CharacterOverlay.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "MovementShooter/UI/Widgets/DirectionalIndicatorWidget.h"
#include "MovementShooter/Character/BaseCharacter.h"
#include "MovementShooter/Dialogue/DialogueSubsystem.h"

void UCharacterOverlay::ShowHitMarker(bool bHeadshot)
{
	if (!HitMarkerImage) return;

	// Tint red for headshots, white for normal hits
	HitMarkerImage->SetColorAndOpacity(
		bHeadshot
		? FLinearColor::Red
		: FLinearColor::White
	);

	HitMarkerImage->SetVisibility(ESlateVisibility::Visible);

	GetWorld()->GetTimerManager().ClearTimer(HitMarkerTimer);

	GetWorld()->GetTimerManager().SetTimer(
		HitMarkerTimer,
		[this]()
		{
			if (HitMarkerImage)
			{
				HitMarkerImage->SetVisibility(ESlateVisibility::Hidden);
			}
		},
		0.15f,
		false
	);
}

void UCharacterOverlay::SetAmmoUIVisibility(bool bVisible)
{
	ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;

	if (WeaponAmmoAmount) WeaponAmmoAmount->SetVisibility(NewVisibility);
	if (CarriedAmmoAmount) CarriedAmmoAmount->SetVisibility(NewVisibility);
}

void UCharacterOverlay::NativeConstruct()
{
	Super::NativeConstruct();

	if (HitMarkerImage)
	{
		HitMarkerImage->SetVisibility(ESlateVisibility::Hidden);
	}

	// 1. Hide subtitles initially
	if (SpeakerText) SpeakerText->SetVisibility(ESlateVisibility::Hidden);
	if (SubtitleText) SubtitleText->SetVisibility(ESlateVisibility::Hidden);

	// 2. Bind to the Dialogue Subsystem!
	if (GetGameInstance())
	{
		if (UDialogueSubsystem* DialogueSubsystem = GetGameInstance()->GetSubsystem<UDialogueSubsystem>())
		{
			DialogueSubsystem->OnDialogueUpdated.AddDynamic(this, &UCharacterOverlay::UpdateSubtitle);
			DialogueSubsystem->OnDialogueCleared.AddDynamic(this, &UCharacterOverlay::ClearSubtitle);
		}
	}
}

void UCharacterOverlay::UpdateInteractionPrompt(bool bShow, const FString& PromptText)
{
	if (InteractionText)
	{
		if (bShow)
		{
			InteractionText->SetText(FText::FromString(PromptText));
			
			// HitTestInvisible ensures the text doesn't block mouse clicks
			InteractionText->SetVisibility(ESlateVisibility::HitTestInvisible); 
		}
		else
		{
			// Hide the text when not looking at an interactable object
			InteractionText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UCharacterOverlay::UpdateSubtitle(const FDialogueEntry& Entry)
{
	if (SpeakerText)
	{
		SpeakerText->SetText(FText::FromString(Entry.SpeakerName));
		SpeakerText->SetVisibility(ESlateVisibility::Visible);
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Entry.SubtitleText);
		SubtitleText->SetVisibility(ESlateVisibility::Visible);
	}
}

void UCharacterOverlay::ClearSubtitle()
{
	if (SpeakerText) SpeakerText->SetVisibility(ESlateVisibility::Hidden);
	if (SubtitleText) SubtitleText->SetVisibility(ESlateVisibility::Hidden);
}

float UCharacterOverlay::CalculateDirectionalUIAngle(APlayerController* PC, AActor* EnemyActor)
{
    if (!PC || !PC->GetPawn() || !EnemyActor) return 0.f;

    APawn* PlayerPawn = PC->GetPawn();
    
    FVector PlayerLoc = PlayerPawn->GetActorLocation();
    FVector EnemyLoc = EnemyActor->GetActorLocation();

    FVector DirToEnemy = (EnemyLoc - PlayerLoc).GetSafeNormal();

    FRotator CamRot = PC->PlayerCameraManager->GetCameraRotation();
    FVector CamForward = CamRot.Vector();
    FVector CamRight = FRotationMatrix(CamRot).GetScaledAxis(EAxis::Y);

    FVector2D Forward2D(CamForward.X, CamForward.Y);
    FVector2D Right2D(CamRight.X, CamRight.Y);
    FVector2D Dir2D(DirToEnemy.X, DirToEnemy.Y);

    Forward2D.Normalize();
    Right2D.Normalize();
    Dir2D.Normalize();

    float DotForward = FVector2D::DotProduct(Forward2D, Dir2D);
    float DotRight = FVector2D::DotProduct(Right2D, Dir2D);

    return FMath::RadiansToDegrees(FMath::Atan2(DotRight, DotForward)); 
}

void UCharacterOverlay::UpdateStealthHUD(AActor* DetectingEnemy, float DetectionLevel, uint8 AlertPhaseRaw)
{
    if (!DetectingEnemy || !DetectionCanvas || !DirectionalIndicatorClass) return;

    // 1. If detection is 0, remove the widget and clear it from the map
    if (DetectionLevel <= 0.01f)
    {
        if (UDirectionalIndicatorWidget** FoundWidget = ActiveStealthIndicators.Find(DetectingEnemy))
        {
            if (*FoundWidget)
            {
                (*FoundWidget)->RemoveFromParent();
            }
            ActiveStealthIndicators.Remove(DetectingEnemy);
        }
        return;
    }

    // 2. Get or Create the Indicator Widget
    UDirectionalIndicatorWidget* IndicatorWidget = nullptr;
    if (UDirectionalIndicatorWidget** FoundWidget = ActiveStealthIndicators.Find(DetectingEnemy))
    {
        IndicatorWidget = *FoundWidget;
    }
    else
    {
        // Spawn a new one!
        IndicatorWidget = CreateWidget<UDirectionalIndicatorWidget>(GetOwningPlayer(), DirectionalIndicatorClass);
        if (IndicatorWidget)
        {
            UCanvasPanelSlot* CanvasSlot = DetectionCanvas->AddChildToCanvas(IndicatorWidget);
            if (CanvasSlot)
            {
                // Anchor it to the exact center of the screen
                CanvasSlot->SetAutoSize(true);
                CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
                CanvasSlot->SetAnchors(FAnchors(0.5f)); 
                CanvasSlot->SetPosition(FVector2D::ZeroVector);
            }
            ActiveStealthIndicators.Add(DetectingEnemy, IndicatorWidget);
        }
    }

	// 3. Update Rotation, Visuals, and POSITION
	if (IndicatorWidget)
	{
		// Calculate where the enemy is relative to our camera
		float Angle = CalculateDirectionalUIAngle(GetOwningPlayer(), DetectingEnemy);
        
		// Rotate the widget to point at them
		IndicatorWidget->SetRenderTransformAngle(Angle);

		// === NEW RADIAL POSITIONING MATH ===
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(IndicatorWidget->Slot))
		{
			// Convert Angle back to radians for the math functions
			float RadAngle = FMath::DegreesToRadians(Angle);
            
			// In Unreal UMG, the Y-axis goes DOWN, so we must invert the Cosine!
			// 0 degrees = Up (X:0, Y:-Radius)
			// 90 degrees = Right (X:Radius, Y:0)
			float X = FMath::Sin(RadAngle) * IndicatorRadius;
			float Y = -FMath::Cos(RadAngle) * IndicatorRadius;
            
			CanvasSlot->SetPosition(FVector2D(X, Y));
		}

		// Update the fill bar and colors
		IndicatorWidget->UpdateVisuals(DetectionLevel, AlertPhaseRaw);
	}
}

void UCharacterOverlay::SetUltimateCharge(float CurrentCharge, float MaxCharge)
{
	if (UltimateBar)
	{
		UltimateBar->SetPercent(MaxCharge > 0.f ? (CurrentCharge / MaxCharge) : 0.f);
	}
}

void UCharacterOverlay::SetEnergySegments(int32 CurrentSegments, int32 MaxSegments)
{
	if (!EnergySegmentBox) return;

	// 1. If we increased our Max Energy, spawn new blocks dynamically!
	while (EnergySegmentBox->GetChildrenCount() < MaxSegments)
	{
		UImage* NewSegment = NewObject<UImage>(this);

		// Give the raw image a solid white brush so it actually renders!
		FSlateBrush SolidBrush;
		SolidBrush.DrawAs = ESlateBrushDrawType::Box;
		NewSegment->SetBrush(SolidBrush);

		UHorizontalBoxSlot* BoxSlot = EnergySegmentBox->AddChildToHorizontalBox(NewSegment);
		
		BoxSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 0.f));
		BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	// 2. If we somehow lost Max Energy, remove the excess blocks
	while (EnergySegmentBox->GetChildrenCount() > MaxSegments)
	{
		EnergySegmentBox->RemoveChildAt(EnergySegmentBox->GetChildrenCount() - 1);
	}

	// 3. Color them based on how much energy we currently have
	for (int32 i = 0; i < MaxSegments; i++)
	{
		if (UImage* SegmentImage = Cast<UImage>(EnergySegmentBox->GetChildAt(i)))
		{
			SegmentImage->SetBrushTintColor(i < CurrentSegments ? EnergyFullColor : EnergyEmptyColor);
		}
	}
}

void UCharacterOverlay::SetHUDMedkits(int32 MedkitCount)
{
	if (MedkitCountText)
	{
		FString CountText = FString::Printf(TEXT("%d"), MedkitCount);
		MedkitCountText->SetText(FText::FromString(CountText));
	}
}

