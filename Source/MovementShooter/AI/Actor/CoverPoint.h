#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CoverPoint.generated.h"

UCLASS()
class MOVEMENTSHOOTER_API ACoverPoint : public AActor
{
	GENERATED_BODY()
	
public:	
	ACoverPoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	class UArrowComponent* DirectionArrow;

	// Check this in the editor if the left side of this wall is open
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	bool bCanPeekLeft = true;

	// Check this in the editor if the right side of this wall is open
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	bool bCanPeekRight = true;
};