#include "SVoxelPlugin.h"

#define LOCTEXT_NAMESPACE "FSVoxelPluginGeneratorModule"

void FSVoxelPluginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FSVoxelPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSVoxelPluginModule, SVoxelPlugin)