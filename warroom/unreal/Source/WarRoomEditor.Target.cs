using UnrealBuildTool;

public class WarRoomEditorTarget : TargetRules
{
	public WarRoomEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		ExtraModuleNames.AddRange(new string[] { "WarRoomSim", "WarRoomShaders", "WarRoom" });
	}
}
