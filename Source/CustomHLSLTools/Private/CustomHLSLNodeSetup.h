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

	/**
	 * Reads the function signature from the shader file and sets up the node (Inputs, Output Type, Code, Include File Paths).
	 * Input pins whose names are kept stay connected. The function named like the file is preferred, otherwise the first one is used.
	 * Returns false and fills OutError on failure.
	 */
	bool SetupNodeFromShaderFile(UMaterialExpressionCustom& Expression, const FString& VirtualPath, FText& OutError);
}
