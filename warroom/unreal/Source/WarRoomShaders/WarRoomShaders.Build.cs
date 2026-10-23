using UnrealBuildTool;

// Tiny module that only tells the renderer where Shaders/Globe.ush lives. It has to load
// early (PostConfigInit), before materials compile, so it is kept apart from the game.
public class WarRoomShaders : ModuleRules
{
	public WarRoomShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "Projects" });
	}
}
