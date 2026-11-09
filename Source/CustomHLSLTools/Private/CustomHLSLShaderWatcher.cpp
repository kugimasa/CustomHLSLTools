#include "CustomHLSLShaderWatcher.h"

#include "CustomHLSLNodeSetup.h"
#include "CustomHLSLShaderUtils.h"
#include "CustomHLSLToolsLog.h"
#include "CustomHLSLToolsSettings.h"
#include "DirectoryWatcherModule.h"
#include "IDirectoryWatcher.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialCachedData.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
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
			bFileRemoved |= Change.Action == FFileChangeData::FCA_Removed;
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
	if (FPlatformTime::Seconds() - LastChangeTime < Settings->RecompileDelaySeconds)
	{
		return true;
	}

	if (bFileRemoved)
	{
		NotifyMissingShaderFiles();
		bFileRemoved = false;
	}
	if (Settings->bAutoRecompileOnSave)
	{
		RecompileAffectedMaterials();
	}
	ChangedRoots.Reset();
	return true;
}

void FCustomHLSLShaderWatcher::NotifyMissingShaderFiles()
{
	// Include files that Custom nodes still point to but that no longer exist, e.g. deleted in the file explorer.
	TArray<TPair<TWeakObjectPtr<UMaterialExpressionCustom>, FString>> Missing;
	TSet<FString> MissingPaths;
	for (TObjectIterator<UMaterialExpressionCustom> It; It; ++It)
	{
		UMaterialExpressionCustom* Expression = *It;
		if (!IsValid(Expression) || Expression->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !Expression->GetOuter() || Expression->GetOutermost() == GetTransientPackage())
		{
			continue;
		}

		for (const FString& VirtualPath : Expression->IncludeFilePaths)
		{
			if (!VirtualPath.IsEmpty() && CustomHLSLTools::IsShaderFileMissing(VirtualPath))
			{
				bool bAlreadyListed = false;
				MissingPaths.Add(VirtualPath, &bAlreadyListed);
				if (!bAlreadyListed)
				{
					Missing.Emplace(Expression, VirtualPath);
				}
			}
		}
	}

	if (Missing.IsEmpty())
	{
		return;
	}

	for (const TPair<TWeakObjectPtr<UMaterialExpressionCustom>, FString>& Entry : Missing)
	{
		UE_LOGF(LogCustomHLSLTools, Warning, "Shader file '%ls' is used by a Custom node but does not exist.", *Entry.Value);
	}

	FNotificationInfo Info(FText::Format(
		LOCTEXT("MissingFiles", "{0} shader file(s) used by Custom nodes no longer exist, so the materials will fail to compile."), Missing.Num()));
	Info.bFireAndForget = false;
	Info.bUseLargeFont = false;
	Info.ExpireDuration = 0.0f;
	Info.FadeOutDuration = 0.5f;

	const TSharedRef<TWeakPtr<SNotificationItem>> NotificationHolder = MakeShared<TWeakPtr<SNotificationItem>>();
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("RecreateMissing", "Recreate"),
		LOCTEXT("RecreateMissingTooltip", "Writes the missing files again from the Custom nodes' inputs and output types."),
		FSimpleDelegate::CreateLambda([Missing, NotificationHolder]()
		{
			int32 NumRecreated = 0;
			for (const TPair<TWeakObjectPtr<UMaterialExpressionCustom>, FString>& Entry : Missing)
			{
				FText Error;
				if (UMaterialExpressionCustom* Expression = Entry.Key.Get())
				{
					if (CustomHLSLTools::RecreateShaderFile(*Expression, Entry.Value, Error))
					{
						++NumRecreated;
					}
					else
					{
						CustomHLSLTools::ShowNotification(Error, false);
					}
				}
			}
			if (TSharedPtr<SNotificationItem> Item = NotificationHolder->Pin())
			{
				Item->SetCompletionState(SNotificationItem::CS_Success);
				Item->ExpireAndFadeout();
			}
			if (NumRecreated > 0)
			{
				CustomHLSLTools::ShowNotification(FText::Format(LOCTEXT("RecreatedFiles", "Recreated {0} shader file(s)."), NumRecreated), true);
			}
		}),
		SNotificationItem::CS_None));
	Info.ButtonDetails.Add(FNotificationButtonInfo(
		LOCTEXT("DismissMissing", "Dismiss"),
		FText(),
		FSimpleDelegate::CreateLambda([NotificationHolder]()
		{
			if (TSharedPtr<SNotificationItem> Item = NotificationHolder->Pin())
			{
				Item->SetCompletionState(SNotificationItem::CS_None);
				Item->ExpireAndFadeout();
			}
		}),
		SNotificationItem::CS_None));

	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(SNotificationItem::CS_Fail);
		*NotificationHolder = Item;
	}
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
		bool bHasMissingFile = false;
		for (const FString& VirtualPath : CachedData.EditorOnlyData->ExpressionIncludeFilePaths)
		{
			if (CustomHLSLTools::IsShaderFileMissing(VirtualPath))
			{
				bHasMissingFile = true;
				UE_LOGF(LogCustomHLSLTools, Warning, "%ls includes '%ls' which does not exist, skipping recompile.", *Material->GetPathName(), *VirtualPath);
				break;
			}

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

		if (bUsesChangedRoot && !bHasMissingFile)
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
