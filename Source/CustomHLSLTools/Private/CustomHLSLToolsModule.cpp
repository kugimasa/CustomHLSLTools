#include "CustomHLSLExpressionCustomization.h"
#include "CustomHLSLShaderWatcher.h"
#include "CustomHLSLToolsLog.h"
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

		// UCustomHLSLToolsSettings is a UDeveloperSettings and is registered automatically in Editor Preferences > Plugins.
		Watcher.Start();
	}

	virtual void ShutdownModule() override
	{
		Watcher.Stop();

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
