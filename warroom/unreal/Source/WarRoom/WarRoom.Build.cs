using UnrealBuildTool;

// The Unreal game: globe, camera, interface and saves around the WarRoomSim simulation.
public class WarRoom : ModuleRules
{
	public WarRoom(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore",
			"ProceduralMeshComponent", "RenderCore", "RHI", "ImageWrapper", "Projects", "WarRoomSim"
		});
	}
}
