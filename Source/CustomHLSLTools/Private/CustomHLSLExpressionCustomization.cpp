#include "CustomHLSLExpressionCustomization.h"

#include "CustomHLSLShaderUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CustomHLSLTools"

namespace
{
	FReply OpenIncludeFiles(const TArray<TWeakObjectPtr<UMaterialExpressionCustom>>& Expressions)
	{
		TArray<FString> Files;
		TArray<FText> Problems;
		bool bHasAnyInclude = false;

		for (const TWeakObjectPtr<UMaterialExpressionCustom>& WeakExpression : Expressions)
		{
			const UMaterialExpressionCustom* Expression = WeakExpression.Get();
			if (!Expression)
			{
				continue;
			}

			for (const FString& VirtualPath : Expression->IncludeFilePaths)
			{
				if (VirtualPath.IsEmpty())
				{
					continue;
				}
				bHasAnyInclude = true;

				FText Error;
				const FString File = CustomHLSLTools::ResolveVirtualShaderPath(VirtualPath, Error);
				if (File.IsEmpty())
				{
					Problems.Add(Error);
				}
				else
				{
					Files.AddUnique(File);
				}
			}
		}

		if (!bHasAnyInclude)
		{
			CustomHLSLTools::ShowNotification(
				LOCTEXT("NoIncludes", "Add a path such as /Project/MyFunc.ush to 'Include File Paths' first."), false);
			return FReply::Handled();
		}

		for (const FText& Problem : Problems)
		{
			CustomHLSLTools::ShowNotification(Problem, false);
		}

		if (Files.Num() > 0 && !CustomHLSLTools::OpenFilesInExternalEditor(Files))
		{
			CustomHLSLTools::ShowNotification(
				LOCTEXT("OpenFailed", "Failed to open the file. Check the open mode in Editor Preferences > Plugins > Custom HLSL Tools."), false);
		}

		return FReply::Handled();
	}
}

TSharedRef<IDetailCustomization> FCustomHLSLExpressionCustomization::MakeInstance()
{
	return MakeShared<FCustomHLSLExpressionCustomization>();
}

void FCustomHLSLExpressionCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	TArray<TWeakObjectPtr<UMaterialExpressionCustom>> Expressions;
	for (const TWeakObjectPtr<UObject>& Object : Objects)
	{
		if (UMaterialExpressionCustom* Expression = Cast<UMaterialExpressionCustom>(Object.Get()))
		{
			Expressions.Add(Expression);
		}
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("MaterialExpressionCustom");

	Category.AddCustomRow(LOCTEXT("ExternalEditorRow", "External Editor"))
	.WholeRowContent()
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("OpenInExternalEditor", "Open Include Files in External Editor"))
		.ToolTipText(LOCTEXT("OpenInExternalEditorTooltip", "Opens every file listed in 'Include File Paths' with the editor configured in Editor Preferences > Plugins > Custom HLSL Tools."))
		.OnClicked_Lambda([Expressions]()
		{
			return OpenIncludeFiles(Expressions);
		})
	];
}

#undef LOCTEXT_NAMESPACE
