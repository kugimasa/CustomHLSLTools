#include "CustomHLSLToolsStyle.h"

#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace CustomHLSLToolsStyle
{
	const FName PluginIconName(TEXT("CustomHLSLTools.PluginIcon"));

	static TSharedPtr<FSlateStyleSet> StyleSet;

	void Register()
	{
		if (StyleSet.IsValid())
		{
			return;
		}

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CustomHLSLTools"));
		if (!Plugin.IsValid())
		{
			return;
		}

		StyleSet = MakeShared<FSlateStyleSet>(TEXT("CustomHLSLToolsStyle"));
		StyleSet->SetContentRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources")));
		StyleSet->Set(PluginIconName, new FSlateImageBrush(StyleSet->RootToContentDir(TEXT("Icon128"), TEXT(".png")), FVector2f(16.0f, 16.0f)));
		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet);
	}

	void Unregister()
	{
		if (StyleSet.IsValid())
		{
			FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet);
			StyleSet.Reset();
		}
	}

	const ISlateStyle& Get()
	{
		return *StyleSet;
	}
}
