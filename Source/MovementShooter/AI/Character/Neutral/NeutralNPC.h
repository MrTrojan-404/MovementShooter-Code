#pragma once

#include "CoreMinimal.h"
#include "MovementShooter/AI/Character/EnemyBase.h"
#include "NeutralNPC.generated.h"

class UDialogueSequenceData;

UCLASS()
class MOVEMENTSHOOTER_API ANeutralNPC : public AEnemyBase
{
	GENERATED_BODY()

public:
	ANeutralNPC();

	virtual void Tick(float DeltaTime) override;

	// Marks this NPC as an ally so player melee swings, bullets, and spells ignore them
	virtual bool IsPlayerAlly() const override { return true; }

	// --- INTERACTION INTERFACE (Inherited via ABaseCharacter) ---
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionText_Implementation() const override;

	// If true, this NPC can be spoken to by pressing Interact
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative")
	bool bCanInteract = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative", meta = (EditCondition = "bCanInteract"))
	FText InteractionPrompt = FText::FromString(TEXT("Talk"));

	// Dialogue asset to play when interacted with or when arriving at a waypoint
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Narrative")
	UDialogueSequenceData* CurrentDialogue = nullptr;

	// Blueprint hook to connect to your Dialogue UI/Audio player
	UFUNCTION(BlueprintImplementableEvent, Category = "NPC|Narrative")
	void OnPlayDialogue(UDialogueSequenceData* DialogueToPlay);

	// --- LEVEL 1 PATROL & STEALTH ---
	// False = Purely passive (ignores player completely, e.g., Hideout or Leader).
	// True  = Runs stealth sight/hearing in Chapter 1 without shooting.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NPC|Behavior")
	bool bEnableLevel1StealthDetection = false;

	// --- LEADER GUIDING (Walk-and-Talk) ---
	// If true, the NPC pauses its patrol when the player falls too far behind
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Leader Guiding")
	bool bWaitForPlayerWhenGuiding = false;

	// Distance at which the Leader stops and waits for the player to catch up
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Leader Guiding", meta = (EditCondition = "bWaitForPlayerWhenGuiding"))
	float MaxLeadDistance = 750.f;

	// Distance at which the Leader resumes walking once the player gets close
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC|Leader Guiding", meta = (EditCondition = "bWaitForPlayerWhenGuiding"))
	float ResumeWalkDistance = 450.f;

	// Call from Blueprints/Triggers to make the Leader start walking a new path
	UFUNCTION(BlueprintCallable, Category = "NPC|Leader Guiding")
	void StartGuidingAlongPath(AWaypointPath* NewPath);

protected:
	virtual void BeginPlay() override;

private:
	bool bIsWaitingForPlayer = false;
	void ConfigurePassiveBrain();
};