#include "WarRoomModule.h"

#include "Misc/Paths.h"
#include "ShaderCore.h"

DEFINE_LOG_CATEGORY(LogWarRoom);

void FWarRoomModule::StartupModule()
{
	// Lets materials include "/Project/Globe.ush" from the project's Shaders folder.
	const FString ShaderDir = FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders"));
	if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Project")))
	{
		AddShaderSourceDirectoryMapping(TEXT("/Project"), ShaderDir);
	}
}

void FWarRoomModule::ShutdownModule() {}

IMPLEMENT_PRIMARY_GAME_MODULE(FWarRoomModule, WarRoom, "WarRoom");
