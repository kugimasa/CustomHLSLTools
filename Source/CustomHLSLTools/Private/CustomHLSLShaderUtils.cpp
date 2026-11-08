#include "CustomHLSLShaderUtils.h"

#include "CustomHLSLToolsLog.h"
#include "CustomHLSLToolsSettings.h"
#include "ISourceCodeAccessModule.h"
#include "ISourceCodeAccessor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ShaderCompilerCore.h"
#include "ShaderCore.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CustomHLSLTools"

namespace CustomHLSLTools
{

FString ResolveVirtualShaderPath(const FString& VirtualPath, FText& OutError)
{
	TArray<FShaderCompilerError> Errors;
	const FString RealPath = GetShaderSourceFilePath(VirtualPath, &Errors);
	if (RealPath.IsEmpty())
	{
		OutError = FText::Format(
			LOCTEXT("CannotMapPath", "Cannot map '{0}' to a shader directory. Check the extension (.ush/.usf) and that the directory is registered (/Project requires a Shaders folder at editor startup)."),
			FText::FromString(VirtualPath));
		return FString();
	}

	const FString AbsolutePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*RealPath);
	if (!FPaths::FileExists(AbsolutePath))
	{
		OutError = FText::Format(LOCTEXT("FileMissing", "File not found: {0}"), FText::FromString(AbsolutePath));
		return FString();
	}

	return AbsolutePath;
}

bool OpenFilesInExternalEditor(const TArray<FString>& AbsoluteFilePaths)
{
	const UCustomHLSLToolsSettings* Settings = GetDefault<UCustomHLSLToolsSettings>();
	bool bAnyOpened = false;

	switch (Settings->OpenMode)
	{
	case ECustomHLSLOpenMode::SourceCodeAccessor:
	{
		ISourceCodeAccessModule& Module = FModuleManager::LoadModuleChecked<ISourceCodeAccessModule>("SourceCodeAccess");
		ISourceCodeAccessor& Accessor = Module.GetAccessor();
		for (const FString& File : AbsoluteFilePaths)
		{
			bAnyOpened |= Accessor.OpenFileAtLine(File, 1);
		}
		break;
	}
	case ECustomHLSLOpenMode::SystemDefault:
		for (const FString& File : AbsoluteFilePaths)
		{
			FPlatformProcess::LaunchFileInDefaultExternalApplication(*File);
			bAnyOpened = true;
		}
		break;
	case ECustomHLSLOpenMode::CustomCommand:
	{
		const FString& Executable = Settings->CustomEditorPath.FilePath;
		if (Executable.IsEmpty())
		{
			UE_LOGF(LogCustomHLSLTools, Warning, "OpenMode is CustomCommand but CustomEditorPath is empty.");
			break;
		}
		for (const FString& File : AbsoluteFilePaths)
		{
			const FString Arguments = Settings->CustomEditorArguments.Replace(TEXT("{File}"), *File);
			FProcHandle Proc = FPlatformProcess::CreateProc(*Executable, *Arguments, true, false, false, nullptr, 0, nullptr, nullptr);
			if (Proc.IsValid())
			{
				FPlatformProcess::CloseProc(Proc);
				bAnyOpened = true;
			}
			else
			{
				UE_LOGF(LogCustomHLSLTools, Warning, "Failed to launch '%ls %ls'.", *Executable, *Arguments);
			}
		}
		break;
	}
	}

	return bAnyOpened;
}

void ShowNotification(const FText& Message, bool bSuccess)
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = 4.0f;
	Info.bUseLargeFont = false;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}

}

#undef LOCTEXT_NAMESPACE
