
#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/Dialogue/MSDialogueTypes.h"
#include "Engine/DataAsset.h"
#include "DialogueSequenceData.generated.h"

UCLASS(BlueprintType)
class MOVEMENTSHOOTER_API UDialogueSequenceData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dialogue")
	FDialogueSequence Sequence;
};

