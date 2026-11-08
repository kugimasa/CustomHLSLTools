#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CustomHLSLToolsSettings.generated.h"

UENUM()
enum class ECustomHLSLOpenMode : uint8
{
	/** Use the editor selected in Editor Preferences > General > Source Code. */
	SourceCodeAccessor,
	/** Use the OS file association for .ush / .usf. */
	SystemDefault,
	/** Launch CustomEditorPath with CustomEditorArguments. */
	CustomCommand,
};

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Custom HLSL Tools"))
class UCustomHLSLToolsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UCustomHLSLToolsSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** How the "Open in External Editor" button opens the include files. */
	UPROPERTY(EditAnywhere, config, Category="External Editor")
	ECustomHLSLOpenMode OpenMode = ECustomHLSLOpenMode::SourceCodeAccessor;

	/** Executable used when OpenMode is CustomCommand (e.g. Code.exe). */
	UPROPERTY(EditAnywhere, config, Category="External Editor", meta=(EditCondition="OpenMode==ECustomHLSLOpenMode::CustomCommand", EditConditionHides))
	FFilePath CustomEditorPath;

	/** Arguments used when OpenMode is CustomCommand. {File} is replaced by the absolute file path. */
	UPROPERTY(EditAnywhere, config, Category="External Editor", meta=(EditCondition="OpenMode==ECustomHLSLOpenMode::CustomCommand", EditConditionHides))
	FString CustomEditorArguments;

	/** Recompile materials that include a shader file when that file is saved. */
	UPROPERTY(EditAnywhere, config, Category="Hot Reload")
	bool bAutoRecompileOnSave = true;

	/** Seconds to wait after the last file change before recompiling, so that editors that save in several steps trigger one recompile. */
	UPROPERTY(EditAnywhere, config, Category="Hot Reload", meta=(ClampMin="0.0", EditCondition="bAutoRecompileOnSave"))
	float RecompileDelaySeconds = 0.5f;
};
