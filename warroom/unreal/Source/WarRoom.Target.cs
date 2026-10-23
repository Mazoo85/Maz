using UnrealBuildTool;

public class WarRoomTarget : TargetRules
{
	public WarRoomTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		ExtraModuleNames.AddRange(new string[] { "WarRoomSim", "WarRoomShaders", "WarRoom" });
	}
}
