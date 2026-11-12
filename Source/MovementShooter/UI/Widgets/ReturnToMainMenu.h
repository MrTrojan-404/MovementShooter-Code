#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ReturnToMainMenu.generated.h"

class UButton;

// The pause menu: restart from the last checkpoint, restart the level, or go back to the main menu.
// (Ported from the multiplayer version: no session handling, no host/client button rules.)
UCLASS()
class MOVEMENTSHOOTER_API UReturnToMainMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	void MenuSetup();
	void MenuTearDown();

	// Name of the main menu map opened by the Return button
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	FName MainMenuLevelName = FName("MainMenu");

	// Freezes the game while the menu is open. Needs the pause input action to have
	// "Trigger when paused" enabled, otherwise the pause key cannot close the menu again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu")
	bool bPauseGameWhileOpen = false;

private:
	UPROPERTY(meta = (BindWidget))
	UButton* ReturnButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RestartLevelButton;

	UPROPERTY(meta = (BindWidgetOptional))
	UButton* RestartCheckpointButton;

	UFUNCTION()
	void ReturnButtonClicked();

	UFUNCTION()
	void RestartLevelClicked();

	UFUNCTION()
	void RestartCheckpointClicked();

	UPROPERTY()
	APlayerController* PlayerController = nullptr;
};
