#include "CustomHLSLExpressionCustomization.h"
#include "CustomHLSLShaderUtils.h"
#include "CustomHLSLShaderWatcher.h"
#include "CustomHLSLToolsLog.h"
#include "CustomHLSLToolsStyle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

DEFINE_LOG_CATEGORY(LogCustomHLSLTools);

#define LOCTEXT_NAMESPACE "CustomHLSLTools"

class FCustomHLSLToolsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		CustomHLSLToolsStyle::Register();

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(
			"MaterialExpressionCustom",
			FOnGetDetailCustomizationInstance::CreateStatic(&FCustomHLSLExpressionCustomization::MakeInstance));

		// UCustomHLSLToolsSettings is a UDeveloperSettings and is registered automatically in Editor Preferences > Plugins.
		Watcher.Start();

		// Start watching <Project>/Shaders when the plugin creates it at runtime.
		ShaderDirectoryCreatedHandle = CustomHLSLTools::OnShaderDirectoryCreated().AddLambda([this]()
		{
			Watcher.Stop();
			Watcher.Start();
		});
	}

	virtual void ShutdownModule() override
	{
		CustomHLSLTools::OnShaderDirectoryCreated().Remove(ShaderDirectoryCreatedHandle);
		Watcher.Stop();

		if (FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyModule->UnregisterCustomClassLayout("MaterialExpressionCustom");
		}

		CustomHLSLToolsStyle::Unregister();
	}

private:
	FCustomHLSLShaderWatcher Watcher;
	FDelegateHandle ShaderDirectoryCreatedHandle;
};

IMPLEMENT_MODULE(FCustomHLSLToolsModule, CustomHLSLTools)

#undef LOCTEXT_NAMESPACE
