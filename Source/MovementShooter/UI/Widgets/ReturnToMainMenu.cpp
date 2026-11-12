#include "MovementShooter/UI/Widgets/ReturnToMainMenu.h"

#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "MovementShooter/PlayerController/CorePlayerController.h"

void UReturnToMainMenu::MenuSetup()
{
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);

	if (ReturnButton) ReturnButton->SetIsEnabled(true);
	if (RestartLevelButton) RestartLevelButton->SetIsEnabled(true);
	if (RestartCheckpointButton) RestartCheckpointButton->SetIsEnabled(true);

	if (UWorld* World = GetWorld())
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			FInputModeGameAndUI InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	if (ReturnButton && !ReturnButton->OnClicked.IsBound())
	{
		ReturnButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	if (RestartLevelButton && !RestartLevelButton->OnClicked.IsBound())
	{
		RestartLevelButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::RestartLevelClicked);
	}
	if (RestartCheckpointButton && !RestartCheckpointButton->OnClicked.IsBound())
	{
		RestartCheckpointButton->OnClicked.AddDynamic(this, &UReturnToMainMenu::RestartCheckpointClicked);
	}

	// Single-player: every button is always available.
	if (RestartLevelButton) RestartLevelButton->SetVisibility(ESlateVisibility::Visible);
	if (RestartCheckpointButton) RestartCheckpointButton->SetVisibility(ESlateVisibility::Visible);

	if (bPauseGameWhileOpen)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}
}

void UReturnToMainMenu::MenuTearDown()
{
	if (bPauseGameWhileOpen)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}

	RemoveFromParent();

	if (UWorld* World = GetWorld())
	{
		PlayerController = PlayerController == nullptr ? World->GetFirstPlayerController() : PlayerController;
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}

	if (ReturnButton && ReturnButton->OnClicked.IsBound())
	{
		ReturnButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::ReturnButtonClicked);
	}
	if (RestartLevelButton && RestartLevelButton->OnClicked.IsBound())
	{
		RestartLevelButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::RestartLevelClicked);
	}
	if (RestartCheckpointButton && RestartCheckpointButton->OnClicked.IsBound())
	{
		RestartCheckpointButton->OnClicked.RemoveDynamic(this, &UReturnToMainMenu::RestartCheckpointClicked);
	}
}

void UReturnToMainMenu::RestartLevelClicked()
{
	// Disable to prevent spamming
	if (RestartLevelButton) RestartLevelButton->SetIsEnabled(false);

	if (ACorePlayerController* CorePC = Cast<ACorePlayerController>(PlayerController))
	{
		MenuTearDown(); // un-pause and close before the level reloads
		CorePC->ServerRestartLevel();
	}
}

void UReturnToMainMenu::RestartCheckpointClicked()
{
	// Disable to prevent spamming
	if (RestartCheckpointButton) RestartCheckpointButton->SetIsEnabled(false);

	if (ACorePlayerController* CorePC = Cast<ACorePlayerController>(PlayerController))
	{
		MenuTearDown(); // un-pause first so the restore runs on a live world
		CorePC->ServerRestartCheckpoint();
	}
}

void UReturnToMainMenu::ReturnButtonClicked()
{
	if (ReturnButton) ReturnButton->SetIsEnabled(false);

	MenuTearDown();
	UGameplayStatics::OpenLevel(this, MainMenuLevelName);
}
