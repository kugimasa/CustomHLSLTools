#pragma once

#include "CoreMinimal.h"

class ISlateStyle;

namespace CustomHLSLToolsStyle
{
	/** Brush name of the plugin icon (Resources/Icon128.png). */
	extern const FName PluginIconName;

	void Register();
	void Unregister();
	const ISlateStyle& Get();
}
