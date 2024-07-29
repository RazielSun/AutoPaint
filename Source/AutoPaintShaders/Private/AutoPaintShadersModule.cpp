#include "AutoPaintShadersModule.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h" // AddShaderSourceDirectoryMapping
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FAutoPaintShadersModule"

void FAutoPaintShadersModule::StartupModule()
{
	FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("AutoPaint"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/AutoPaint"), PluginShaderDir);
}

void FAutoPaintShadersModule::ShutdownModule()
{
    
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FAutoPaintShadersModule, AutoPaintShaders)