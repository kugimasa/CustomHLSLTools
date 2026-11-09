#include "CustomHLSLExpressionCustomization.h"

#include "CustomHLSLNodeSetup.h"
#include "CustomHLSLShaderUtils.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Materials/MaterialExpressionCustom.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
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

	FReply CreateShaderFile(const TArray<TWeakObjectPtr<UMaterialExpressionCustom>>& Expressions, const FString& RelativeName, const TWeakPtr<IPropertyUtilities>& PropertyUtilities)
	{
		if (Expressions.Num() != 1 || !Expressions[0].IsValid())
		{
			CustomHLSLTools::ShowNotification(LOCTEXT("SelectOneNode", "Select a single Custom node."), false);
			return FReply::Handled();
		}

		FText Error;
		FScopedTransaction Transaction(LOCTEXT("CreateShaderTransaction", "Create Shader File and Set Up Custom Node"));
		if (CustomHLSLTools::CreateShaderFile(*Expressions[0], RelativeName, Error))
		{
			CustomHLSLTools::ShowNotification(LOCTEXT("ShaderCreated", "Created the shader file and set up the Custom node."), true);
			if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
			{
				Utilities->ForceRefresh();
			}
		}
		else
		{
			Transaction.Cancel();
			CustomHLSLTools::ShowNotification(Error, false);
		}
		return FReply::Handled();
	}

	FReply SetupFromIncludeFile(const TArray<TWeakObjectPtr<UMaterialExpressionCustom>>& Expressions, const TWeakPtr<IPropertyUtilities>& PropertyUtilities)
	{
		UMaterialExpressionCustom* Expression = Expressions.Num() == 1 ? Expressions[0].Get() : nullptr;
		if (!Expression)
		{
			CustomHLSLTools::ShowNotification(LOCTEXT("SelectOneNodeForSetup", "Select a single Custom node."), false);
			return FReply::Handled();
		}

		FString VirtualPath;
		for (const FString& Path : Expression->IncludeFilePaths)
		{
			if (!Path.IsEmpty())
			{
				VirtualPath = Path;
				break;
			}
		}
		if (VirtualPath.IsEmpty())
		{
			CustomHLSLTools::ShowNotification(
				LOCTEXT("NoIncludeToSetup", "Add a path such as /Project/MyFunc.ush to 'Include File Paths' first."), false);
			return FReply::Handled();
		}

		FText Error;
		FScopedTransaction Transaction(LOCTEXT("SetupNodeTransaction", "Set Up Custom Node from Shader File"));
		if (CustomHLSLTools::SetupNodeFromShaderFile(*Expression, VirtualPath, Error))
		{
			CustomHLSLTools::ShowNotification(LOCTEXT("NodeSetUp", "Set up the Custom node from the function signature."), true);
			if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
			{
				Utilities->ForceRefresh();
			}
		}
		else
		{
			Transaction.Cancel();
			CustomHLSLTools::ShowNotification(Error, false);
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
	const TWeakPtr<IPropertyUtilities> PropertyUtilities = DetailBuilder.GetPropertyUtilities();
	const TSharedRef<FString> NewShaderName = MakeShared<FString>();

	Category.AddCustomRow(LOCTEXT("CreateShaderRow", "Create Shader File"))
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("CreateShaderLabel", "New Shader File"))
		.ToolTipText(LOCTEXT("CreateShaderLabelTooltip", "Creates <Project>/Shaders/<Name>.ush with a function built from this node's inputs and output type, then sets up Include File Paths and Code to call it. Sub folders such as Sub/MyFunc are allowed."))
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SEditableTextBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.HintText(LOCTEXT("CreateShaderHint", "MyFunc"))
			.OnTextChanged_Lambda([NewShaderName](const FText& Text)
			{
				*NewShaderName = Text.ToString();
			})
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("CreateShaderButton", "Create"))
			.ToolTipText(LOCTEXT("CreateShaderButtonTooltip", "Creates the shader file and sets up this node. Existing files are never overwritten."))
			.OnClicked_Lambda([Expressions, NewShaderName, PropertyUtilities]()
			{
				return CreateShaderFile(Expressions, *NewShaderName, PropertyUtilities);
			})
		]
	];

	Category.AddCustomRow(LOCTEXT("SetupFromFileRow", "Set Up From Include File"))
	.WholeRowContent()
	[
		SNew(SButton)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("SetupFromFile", "Set Up Node from Include File"))
		.ToolTipText(LOCTEXT("SetupFromFileTooltip", "Reads the function signature from the first file in 'Include File Paths' and sets Inputs, Output Type and Code. Inputs that keep their name stay connected."))
		.OnClicked_Lambda([Expressions, PropertyUtilities]()
		{
			return SetupFromIncludeFile(Expressions, PropertyUtilities);
		})
	];

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
