#include "CustomHLSLShaderWatcher.h"

#include "CustomHLSLShaderUtils.h"
#include "CustomHLSLToolsLog.h"
#include "CustomHLSLToolsSettings.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialCachedData.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "CustomHLSLTools"

FCustomHLSLShaderWatcher::~FCustomHLSLShaderWatcher()
{
	Stop();
}

void FCustomHLSLShaderWatcher::Start()
{
	FDirectoryWatcherModule& WatcherModule = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>("DirectoryWatcher");
	IDirectoryWatcher* DirectoryWatcher = WatcherModule.Get();
	if (!DirectoryWatcher)
	{
		return;
	}

	// The same folders the engine can map as /Project and /Plugin/<Name>. Engine plugins are skipped on purpose.
	TArray<FString> Directories;
	Directories.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Shaders")));
	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		if (Plugin->GetType() != EPluginType::Engine)
		{
			Directories.Add(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
		}
	}

	for (FString& Directory : Directories)
	{
		Directory = FPaths::ConvertRelativePathToFull(Directory);
		FPaths::NormalizeDirectoryName(Directory);
		if (!IFileManager::Get().DirectoryExists(*Directory))
		{
			continue;
		}

		FWatchedRoot& Root = WatchedRoots.AddDefaulted_GetRef();
		Root.Directory = Directory;
		const bool bRegistered = DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(
			Directory,
			IDirectoryWatcher::FDirectoryChanged::CreateRaw(this, &FCustomHLSLShaderWatcher::OnDirectoryChanged, Directory),
			Root.Handle);
		if (bRegistered)
		{
			UE_LOGF(LogCustomHLSLTools, Log, "Watching shader directory '%ls'.", *Directory);
		}
		else
		{
			WatchedRoots.Pop();
		}
	}

	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCustomHLSLShaderWatcher::Tick), 0.25f);
}

void FCustomHLSLShaderWatcher::Stop()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	TickHandle.Reset();

	if (FDirectoryWatcherModule* WatcherModule = FModuleManager::GetModulePtr<FDirectoryWatcherModule>("DirectoryWatcher"))
	{
		if (IDirectoryWatcher* DirectoryWatcher = WatcherModule->Get())
		{
			for (const FWatchedRoot& Root : WatchedRoots)
			{
				DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(Root.Directory, Root.Handle);
			}
		}
	}
	WatchedRoots.Reset();
	ChangedRoots.Reset();
}

void FCustomHLSLShaderWatcher::OnDirectoryChanged(const TArray<FFileChangeData>& Changes, FString Root)
{
	for (const FFileChangeData& Change : Changes)
	{
		const FString Extension = FPaths::GetExtension(Change.Filename).ToLower();
		if (Extension == TEXT("ush") || Extension == TEXT("usf"))
		{
			ChangedRoots.Add(Root);
			LastChangeTime = FPlatformTime::Seconds();
			return;
		}
	}
}

bool FCustomHLSLShaderWatcher::Tick(float DeltaTime)
{
	if (ChangedRoots.IsEmpty())
	{
		return true;
	}

	const UCustomHLSLToolsSettings* Settings = GetDefault<UCustomHLSLToolsSettings>();
	if (!Settings->bAutoRecompileOnSave)
	{
		ChangedRoots.Reset();
		return true;
	}

	if (FPlatformTime::Seconds() - LastChangeTime >= Settings->RecompileDelaySeconds)
	{
		RecompileAffectedMaterials();
		ChangedRoots.Reset();
	}
	return true;
}

void FCustomHLSLShaderWatcher::RecompileAffectedMaterials()
{
	// Pick up the new file contents so the include hash used by the shader map ID changes.
	FlushShaderFileCache();

	TArray<UMaterial*> Affected;
	for (TObjectIterator<UMaterial> It; It; ++It)
	{
		UMaterial* Material = *It;
		if (!IsValid(Material) || Material->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
		{
			continue;
		}

		const FMaterialCachedExpressionData& CachedData = Material->GetCachedExpressionData();
		if (!CachedData.EditorOnlyData)
		{
			continue;
		}

		bool bUsesChangedRoot = false;
		for (const FString& VirtualPath : CachedData.EditorOnlyData->ExpressionIncludeFilePaths)
		{
			const FString RealPath = GetShaderSourceFilePath(VirtualPath);
			if (RealPath.IsEmpty())
			{
				continue;
			}

			const FString AbsolutePath = FPaths::ConvertRelativePathToFull(RealPath);
			for (const FString& Root : ChangedRoots)
			{
				if (FPaths::IsUnderDirectory(AbsolutePath, Root))
				{
					bUsesChangedRoot = true;
					break;
				}
			}
			if (bUsesChangedRoot)
			{
				break;
			}
		}

		if (bUsesChangedRoot)
		{
			Affected.Add(Material);
		}
	}

	if (Affected.IsEmpty())
	{
		return;
	}

	UE_LOGF(LogCustomHLSLTools, Log, "Shader file changed, recompiling %d material(s).", Affected.Num());

	int32 NumWithErrors = 0;
	for (UMaterial* Material : Affected)
	{
		const TArray<FString> Errors = UMaterialEditingLibrary::RecompileMaterial(Material);
		if (!Errors.IsEmpty())
		{
			++NumWithErrors;
			for (const FString& Error : Errors)
			{
				UE_LOGF(LogCustomHLSLTools, Warning, "%ls: %ls", *Material->GetPathName(), *Error);
			}
		}
	}

	if (NumWithErrors > 0)
	{
		CustomHLSLTools::ShowNotification(
			FText::Format(LOCTEXT("RecompiledWithErrors", "Recompiled {0} material(s), {1} with shader errors (see Output Log)."), Affected.Num(), NumWithErrors), false);
	}
	else
	{
		CustomHLSLTools::ShowNotification(
			FText::Format(LOCTEXT("Recompiled", "Recompiled {0} material(s) after shader file change."), Affected.Num()), true);
	}
}

#undef LOCTEXT_NAMESPACE
