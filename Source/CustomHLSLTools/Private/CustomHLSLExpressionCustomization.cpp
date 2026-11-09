#include "CustomHLSLExpressionCustomization.h"

#include "CustomHLSLNodeSetup.h"
#include "CustomHLSLShaderUtils.h"
#include "CustomHLSLToolsStyle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IPropertyUtilities.h"
#include "Materials/MaterialExpressionCustom.h"
#include "ScopedTransaction.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
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

	void RenameShaderFile(const TArray<TWeakObjectPtr<UMaterialExpressionCustom>>& Expressions, const FString& VirtualPath, const FString& NewName, const TWeakPtr<IPropertyUtilities>& PropertyUtilities)
	{
		UMaterialExpressionCustom* Expression = Expressions.Num() == 1 ? Expressions[0].Get() : nullptr;
		if (!Expression)
		{
			return;
		}

		FText Error;
		FScopedTransaction Transaction(LOCTEXT("RenameShaderTransaction", "Rename Shader File"));
		if (!CustomHLSLTools::RenameShaderFile(*Expression, VirtualPath, NewName, Error))
		{
			Transaction.Cancel();
			CustomHLSLTools::ShowNotification(Error, false);
		}

		// Refresh in both cases so that a rejected name goes back to the current one.
		if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
		{
			Utilities->ForceRefresh();
		}
	}

	FReply RecreateShaderFile(const TArray<TWeakObjectPtr<UMaterialExpressionCustom>>& Expressions, const FString& VirtualPath, const TWeakPtr<IPropertyUtilities>& PropertyUtilities)
	{
		UMaterialExpressionCustom* Expression = Expressions.Num() == 1 ? Expressions[0].Get() : nullptr;
		if (!Expression)
		{
			return FReply::Handled();
		}

		FText Error;
		if (CustomHLSLTools::RecreateShaderFile(*Expression, VirtualPath, Error))
		{
			CustomHLSLTools::ShowNotification(FText::Format(LOCTEXT("ShaderRecreated", "Recreated {0}."), FText::FromString(VirtualPath)), true);
			if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
			{
				Utilities->ForceRefresh();
			}
		}
		else
		{
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

	// Header so that it is clear these rows come from this plugin.
	Category.AddCustomRow(LOCTEXT("PluginHeaderRow", "Custom HLSL Tools"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 2.0f, 6.0f, 2.0f)
		[
			SNew(SImage)
			.Image(CustomHLSLToolsStyle::Get().GetBrush(CustomHLSLToolsStyle::PluginIconName))
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.Text(LOCTEXT("PluginHeader", "Custom HLSL Tools"))
		]
	];

	// The first non-empty include path is the shader file this node was set up with.
	FString VirtualPath;
	if (Expressions.Num() == 1 && Expressions[0].IsValid())
	{
		for (const FString& Path : Expressions[0]->IncludeFilePaths)
		{
			if (!Path.IsEmpty())
			{
				VirtualPath = Path;
				break;
			}
		}
	}

	// Until a shader file is attached, only offer to create one.
	if (Expressions.Num() == 1 && VirtualPath.IsEmpty())
	{
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
		return;
	}

	if (Expressions.Num() == 1)
	{
		FString RelativeName;
		const bool bIsProjectFile = CustomHLSLTools::GetProjectShaderRelativeName(VirtualPath, RelativeName);
		const bool bMissing = CustomHLSLTools::IsShaderFileMissing(VirtualPath);

		Category.AddCustomRow(LOCTEXT("ShaderFileRow", "Shader File"))
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("ShaderFileLabel", "Shader File"))
			.ToolTipText(LOCTEXT("ShaderFileLabelTooltip", "Name of the file in <Project>/Shaders. Editing it renames the file and the function inside it, and updates Include File Paths, Code and Description."))
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		[
			SNew(SEditableTextBox)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(FText::FromString(bIsProjectFile ? RelativeName : VirtualPath))
			.IsReadOnly(!bIsProjectFile || bMissing)
			.OnTextCommitted_Lambda([Expressions, VirtualPath, RelativeName, PropertyUtilities](const FText& Text, ETextCommit::Type)
			{
				if (Text.ToString() != RelativeName)
				{
					RenameShaderFile(Expressions, VirtualPath, Text.ToString(), PropertyUtilities);
				}
			})
		];

		if (bMissing)
		{
			Category.AddCustomRow(LOCTEXT("MissingFileRow", "Shader File Missing"))
			.WholeRowContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.2f))
					.AutoWrapText(true)
					.Text(FText::Format(LOCTEXT("MissingFile", "{0} was not found. The material will fail to compile until it is recreated."), FText::FromString(VirtualPath)))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RecreateButton", "Recreate"))
					.ToolTipText(LOCTEXT("RecreateButtonTooltip", "Writes the file again from this node's inputs and output type."))
					.OnClicked_Lambda([Expressions, VirtualPath, PropertyUtilities]()
					{
						return RecreateShaderFile(Expressions, VirtualPath, PropertyUtilities);
					})
				]
			];
		}
	}

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
