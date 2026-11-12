#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DirectionalIndicatorWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class MOVEMENTSHOOTER_API UDirectionalIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(meta = (BindWidget))
	UProgressBar* DetectionBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* PhaseIndicator;

	void UpdateVisuals(float DetectionLevel, uint8 AlertPhaseRaw);

protected:
	virtual void NativeConstruct() override;
};