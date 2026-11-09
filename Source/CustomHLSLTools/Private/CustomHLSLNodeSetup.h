#pragma once

#include "CoreMinimal.h"

class UMaterialExpressionCustom;

namespace CustomHLSLTools
{
	/**
	 * Creates <Project>/Shaders/<RelativeName>.ush with a function generated from the node's inputs and output type,
	 * then sets up the node (Description, Include File Paths, Code) to call it.
	 * RelativeName may contain sub folders (e.g. "Sub/MyFunc"); the function is named after the last element.
	 * Existing files are never overwritten. Returns false and fills OutError on failure.
	 */
	bool CreateShaderFile(UMaterialExpressionCustom& Expression, const FString& RelativeName, FText& OutError);

	/** Converts "/Project/Sub/MyFunc.ush" to "Sub/MyFunc". Returns false for any other kind of path. */
	bool GetProjectShaderRelativeName(const FString& VirtualPath, FString& OutName);

	/**
	 * Writes the file for VirtualPath again from the node's inputs and output type, e.g. after it was deleted in the file explorer.
	 * Fails if the file exists. Returns false and fills OutError on failure.
	 */
	bool RecreateShaderFile(UMaterialExpressionCustom& Expression, const FString& VirtualPath, FText& OutError);

	/**
	 * Renames the file for VirtualPath (a /Project path) to <Project>/Shaders/<NewRelativeName>.ush, renames the function
	 * inside it and updates Include File Paths, Code and Description of the node. Existing files are never overwritten.
	 */
	bool RenameShaderFile(UMaterialExpressionCustom& Expression, const FString& VirtualPath, const FString& NewRelativeName, FText& OutError);

	/**
	 * Reads the function signature from the shader file and sets up the node (Inputs, Output Type, Code, Include File Paths).
	 * Input pins whose names are kept stay connected. The function named like the file is preferred, otherwise the first one is used.
	 * If ReplacedVirtualPath is set, that Include File Paths entry is replaced by VirtualPath instead of appending.
	 * Returns false and fills OutError on failure.
	 */
	bool SetupNodeFromShaderFile(UMaterialExpressionCustom& Expression, const FString& VirtualPath, FText& OutError, const FString& ReplacedVirtualPath = FString());
}
