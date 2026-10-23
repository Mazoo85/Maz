#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"

class FWarRoomShadersModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// Lets materials include "/Project/Globe.ush" from the project's Shaders folder.
		const FString ShaderDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders")));
		if (!AllShaderSourceDirectoryMappings().Contains(TEXT("/Project")))
		{
			AddShaderSourceDirectoryMapping(TEXT("/Project"), ShaderDir);
		}
	}
};

IMPLEMENT_MODULE(FWarRoomShadersModule, WarRoomShaders);
