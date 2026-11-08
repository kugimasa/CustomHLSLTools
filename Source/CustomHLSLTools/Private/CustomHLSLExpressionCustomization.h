#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/** Adds an "Open in External Editor" button to the Custom HLSL material expression details. */
class FCustomHLSLExpressionCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	//~ IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
