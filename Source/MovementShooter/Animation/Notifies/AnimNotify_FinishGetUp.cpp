
// Fill out your copyright notice in the Description page of Project Settings.

#include "MovementShooter/Animation/Notifies/AnimNotify_FinishGetUp.h"
#include "MovementShooter/Character/BaseCharacter.h"

void UAnimNotify_FinishGetUp::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (MeshComp && MeshComp->GetOwner())
	{
		
	}
}

