using UnrealBuildTool;

public class MovementShooter : ModuleRules
{
	public MovementShooter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PhysicsCore",
			"InputCore",
			"EnhancedInput",

			// AI
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",

			// Niagara
			"Niagara",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// UMG for widgets
			"UMG",
			"Slate",
			"SlateCore",

			// Audio
			"AudioMixer",
		});

		// Removed vs. GameAnimationSample: Mover, MotionWarping (only the takedown used it),
		// OnlineSubsystem / OnlineSubsystemUtils / CoreOnline / MultiplayerSessions (single-player),
		// GameplayCameras (nothing in the code referenced it).
	}
}
