#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

struct FFileChangeData;

/**
 * Watches the project and plugin Shaders folders. When a .ush/.usf file is saved, flushes the shader file cache
 * and recompiles the loaded materials whose Custom HLSL nodes include a file from the changed folder.
 * When a file is removed while Custom HLSL nodes still include it, offers to recreate it.
 */
class FCustomHLSLShaderWatcher
{
public:
	~FCustomHLSLShaderWatcher();

	void Start();
	void Stop();

private:
	void OnDirectoryChanged(const TArray<FFileChangeData>& Changes, FString Root);
	bool Tick(float DeltaTime);
	void RecompileAffectedMaterials();
	void NotifyMissingShaderFiles();

	struct FWatchedRoot
	{
		FString Directory;
		FDelegateHandle Handle;
	};

	TArray<FWatchedRoot> WatchedRoots;
	TSet<FString> ChangedRoots;
	bool bFileRemoved = false;
	double LastChangeTime = 0.0;
	FTSTicker::FDelegateHandle TickHandle;
};
