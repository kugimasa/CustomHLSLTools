#pragma once

#include "CoreMinimal.h"

namespace CustomHLSLTools
{
	/**
	 * Resolves a virtual shader path (e.g. /Project/MyFunc.ush) to an absolute file path.
	 * Returns an empty string and fills OutError if the path cannot be mapped or the file does not exist.
	 */
	FString ResolveVirtualShaderPath(const FString& VirtualPath, FText& OutError);

	/** Opens the files using the method chosen in the plugin settings. Returns false if nothing could be opened. */
	bool OpenFilesInExternalEditor(const TArray<FString>& AbsoluteFilePaths);

	/** Shows a Slate toast notification. */
	void ShowNotification(const FText& Message, bool bSuccess);
}
