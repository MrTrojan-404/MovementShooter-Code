
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "MovementShooter/Objective/Checkpoint/CheckpointTypes.h" // Update this path to match yours!
#include "ShooterSaveGame.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API UShooterSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// This is the master payload that gets written to your hard drive
	UPROPERTY(VisibleAnywhere, Category = "Save Data")
	FCheckpointSaveData SavedCheckpointData;
};

