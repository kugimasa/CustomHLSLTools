#pragma once

#include "CoreMinimal.h"

namespace CustomHLSLTools
{
	/**
	 * Resolves a virtual shader path (e.g. /Project/MyFunc.ush) to an absolute file path.
	 * Returns an empty string and fills OutError if the path cannot be mapped or the file does not exist.
	 */
	FString ResolveVirtualShaderPath(const FString& VirtualPath, FText& OutError);

	/** Maps a virtual shader path to an absolute file path without checking that the file exists. Returns an empty string if it cannot be mapped. */
	FString GetAbsoluteShaderPath(const FString& VirtualPath);

	/** True if the virtual path maps to a shader directory but the file is not there (e.g. deleted in the file explorer). */
	bool IsShaderFileMissing(const FString& VirtualPath);

	/** Opens the files using the method chosen in the plugin settings. Returns false if nothing could be opened. */
	bool OpenFilesInExternalEditor(const TArray<FString>& AbsoluteFilePaths);

	/** Returns <Project>/Shaders as an absolute path. */
	FString GetProjectShaderDirectory();

	/**
	 * Creates <Project>/Shaders if needed and makes sure it is mapped as /Project.
	 * Broadcasts OnShaderDirectoryCreated() when the folder was newly created. Returns false on failure.
	 */
	bool EnsureProjectShaderDirectory(FText& OutError);

	/** Broadcast after <Project>/Shaders was created at runtime, so that it can be watched without restarting the editor. */
	FSimpleMulticastDelegate& OnShaderDirectoryCreated();

	/** Shows a Slate toast notification. */
	void ShowNotification(const FText& Message, bool bSuccess);
}
