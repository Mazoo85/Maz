using UnrealBuildTool;

// The engine-independent simulation. Plain C++20 and the standard library only, so the
// same sources build and test with g++ (see Tests/Makefile) and inside Unreal.
public class WarRoomSim : ModuleRules
{
	public WarRoomSim(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		bUseUnity = false;
		bEnableExceptions = false;
		PublicDependencyModuleNames.AddRange(new string[] { "Core" });
		PublicDefinitions.Add("JSON_NOEXCEPTION=1");
		PublicIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Public"));
	}
}
