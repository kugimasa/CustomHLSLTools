#include "CustomHLSLExpressionCustomization.h"
#include "CustomHLSLShaderWatcher.h"
#include "CustomHLSLToolsLog.h"
#include "CustomHLSLToolsSettings.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

DEFINE_LOG_CATEGORY(LogCustomHLSLTools);

#define LOCTEXT_NAMESPACE "CustomHLSLTools"

class FCustomHLSLToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"MaterialExpressionCustom",
			FOnGetDetailCustomizationInstance::CreateStatic(&FCustomHLSLExpressionCustomization::MakeInstance));

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings(
				"Editor", "Plugins", "CustomHLSLTools",
				LOCTEXT("SettingsName", "Custom HLSL Tools"),
				LOCTEXT("SettingsDescription", "External editor and hot reload settings for Material Custom HLSL nodes."),
				GetMutableDefault<UCustomHLSLToolsSettings>());
		}

		Watcher.Start();
	}

	virtual void ShutdownModule() override
	{
		Watcher.Stop();

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Editor", "Plugins", "CustomHLSLTools");
		}

		if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyModule->UnregisterCustomClassLayout("MaterialExpressionCustom");
		}
	}

private:
	FCustomHLSLShaderWatcher Watcher;
};

IMPLEMENT_MODULE(FCustomHLSLToolsModule, CustomHLSLTools)

#undef LOCTEXT_NAMESPACE
