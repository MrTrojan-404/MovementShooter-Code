#include "CoverPoint.h"
#include "Components/ArrowComponent.h"

ACoverPoint::ACoverPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	RootComponent = DirectionArrow;
	
	// Make it a nice highly visible color in the editor
	DirectionArrow->ArrowColor = FColor::Cyan;
	DirectionArrow->ArrowSize = 2.0f;
}