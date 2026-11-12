
#pragma once

#include "CoreMinimal.h"
#include "EnemyAITypes.generated.h"

UENUM(BlueprintType)
enum class EAIState : uint8
{
	Patrol      UMETA(DisplayName = "Patrol"),
	Alert       UMETA(DisplayName = "Alert"),
	Engage      UMETA(DisplayName = "Engage"),
	Dead        UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class EAIMovementSpeed : uint8
{
	Walk    UMETA(DisplayName = "Walk"),
	Jog     UMETA(DisplayName = "Jog"),
	Sprint  UMETA(DisplayName = "Sprint")
};

