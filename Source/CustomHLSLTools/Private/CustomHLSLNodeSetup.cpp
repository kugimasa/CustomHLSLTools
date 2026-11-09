#include "CustomHLSLNodeSetup.h"

#include "CustomHLSLShaderUtils.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"
#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "CustomHLSLTools"

namespace
{
	const TCHAR* const DefaultArgumentType = TEXT("float3");

	/** Runs Mutate between PreEditChange / PostEditChangeProperty so that the graph node and material pick up the change. */
	template <typename FuncType>
	void ModifyProperty(UMaterialExpressionCustom& Expression, FName PropertyName, FuncType&& Mutate)
	{
		FProperty* Property = FindFProperty<FProperty>(UMaterialExpressionCustom::StaticClass(), PropertyName);
		Expression.PreEditChange(Property);
		Mutate();
		FPropertyChangedEvent Event(Property);
		Expression.PostEditChangeProperty(Event);
	}

	bool IsValidIdentifier(const FString& Name)
	{
		if (Name.IsEmpty() || FChar::IsDigit(Name[0]))
		{
			return false;
		}
		for (const TCHAR Char : Name)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
			{
				return false;
			}
		}
		return true;
	}

	FString ToIdentifier(const FString& Name)
	{
		FString Result;
		for (const TCHAR Char : Name)
		{
			Result.AppendChar((FChar::IsAlnum(Char) || Char == TEXT('_')) ? Char : TEXT('_'));
		}
		if (Result.IsEmpty() || FChar::IsDigit(Result[0]))
		{
			Result.InsertAt(0, TEXT('_'));
		}
		return Result;
	}

	/** Returns the HLSL return type for an output type. */
	FString GetReturnType(ECustomMaterialOutputType OutputType)
	{
		switch (OutputType)
		{
		case CMOT_Float1: return TEXT("float");
		case CMOT_Float2: return TEXT("float2");
		case CMOT_Float3: return TEXT("float3");
		case CMOT_Float4: return TEXT("float4");
		case CMOT_MaterialAttributes: return TEXT("FMaterialAttributes");
		default: return TEXT("float4");
		}
	}

	bool TryParseReturnType(const FString& TypeName, ECustomMaterialOutputType& OutType)
	{
		if (TypeName == TEXT("float") || TypeName == TEXT("half")) { OutType = CMOT_Float1; return true; }
		if (TypeName == TEXT("float2") || TypeName == TEXT("half2")) { OutType = CMOT_Float2; return true; }
		if (TypeName == TEXT("float3") || TypeName == TEXT("half3")) { OutType = CMOT_Float3; return true; }
		if (TypeName == TEXT("float4") || TypeName == TEXT("half4")) { OutType = CMOT_Float4; return true; }
		if (TypeName == TEXT("FMaterialAttributes")) { OutType = CMOT_MaterialAttributes; return true; }
		return false;
	}

	/** Validates a user supplied path such as "Sub/MyFunc" and returns it with forward slashes and without extension. */
	bool NormalizeRelativeName(const FString& RelativeName, FString& OutName, FText& OutError)
	{
		FString Name = RelativeName.TrimStartAndEnd().Replace(TEXT("\\"), TEXT("/"));
		if (Name.EndsWith(TEXT(".ush"), ESearchCase::IgnoreCase))
		{
			Name.LeftChopInline(4);
		}

		TArray<FString> Parts;
		Name.ParseIntoArray(Parts, TEXT("/"), true);
		if (Parts.IsEmpty())
		{
			OutError = LOCTEXT("EmptyName", "Enter a file name such as MyFunc or Sub/MyFunc.");
			return false;
		}
		for (const FString& Part : Parts)
		{
			if (Part == TEXT("..") || Part == TEXT(".") || Part.Contains(TEXT(":")))
			{
				OutError = LOCTEXT("InvalidName", "The file name must be a relative path inside <Project>/Shaders.");
				return false;
			}
		}

		OutName = FString::Join(Parts, TEXT("/"));
		return true;
	}

	FString BuildCall(const FString& FunctionName, const TArray<FCustomInput>& Inputs)
	{
		TArray<FString> Names;
		for (const FCustomInput& Input : Inputs)
		{
			Names.Add(Input.InputName.ToString());
		}
		return FString::Printf(TEXT("return %s(%s);"), *FunctionName, *FString::Join(Names, TEXT(", ")));
	}

	FString BuildShaderSource(const FString& FunctionName, ECustomMaterialOutputType OutputType, const TArray<FCustomInput>& Inputs)
	{
		TArray<FString> Arguments;
		for (const FCustomInput& Input : Inputs)
		{
			Arguments.Add(FString::Printf(TEXT("%s %s"), DefaultArgumentType, *Input.InputName.ToString()));
		}

		const FString ReturnType = GetReturnType(OutputType);
		FString Body;
		if (OutputType == CMOT_MaterialAttributes)
		{
			Body = TEXT("\tFMaterialAttributes Result = (FMaterialAttributes)0;\n\treturn Result;");
		}
		else
		{
			Body = FString::Printf(TEXT("\treturn (%s)0;"), *ReturnType);
		}

		return FString::Printf(TEXT("#pragma once\n\n%s %s(%s)\n{\n%s\n}\n"),
			*ReturnType, *FunctionName, *FString::Join(Arguments, TEXT(", ")), *Body);
	}

	/** Removes // and block comments so that the signature regex is not confused by them. */
	FString StripComments(const FString& Source)
	{
		FString Result;
		Result.Reserve(Source.Len());
		const int32 Length = Source.Len();
		for (int32 Index = 0; Index < Length; ++Index)
		{
			if (Source[Index] == TEXT('/') && Index + 1 < Length && Source[Index + 1] == TEXT('/'))
			{
				while (Index < Length && Source[Index] != TEXT('
'))
				{
					++Index;
				}
				Result.AppendChar(TEXT('
'));
			}
			else if (Source[Index] == TEXT('/') && Index + 1 < Length && Source[Index + 1] == TEXT('*'))
			{
				Index += 2;
				while (Index + 1 < Length && !(Source[Index] == TEXT('*') && Source[Index + 1] == TEXT('/')))
				{
					++Index;
				}
				++Index;
				Result.AppendChar(TEXT(' '));
			}
			else
			{
				Result.AppendChar(Source[Index]);
			}
		}
		return Result;
	}

	struct FParsedFunction
	{
		FString ReturnType;
		FString Name;
		TArray<FString> ArgumentNames;
	};

	bool ParseFunction(const FString& Source, const FString& PreferredName, FParsedFunction& OutFunction, FText& OutError)
	{
		static const TSet<FString> Keywords = { TEXT("if"), TEXT("for"), TEXT("while"), TEXT("switch"), TEXT("else"), TEXT("return") };

		const FString Stripped = StripComments(Source);
		const FRegexPattern Pattern(TEXT("([A-Za-z_]\\w*)\\s+([A-Za-z_]\\w*)\\s*\\(([^)]*)\\)\\s*\\{"));
		FRegexMatcher Matcher(Pattern, Stripped);

		bool bFound = false;
		FString ArgumentList;
		while (Matcher.FindNext())
		{
			const FString ReturnType = Matcher.GetCaptureGroup(1);
			const FString Name = Matcher.GetCaptureGroup(2);
			if (Keywords.Contains(ReturnType))
			{
				continue;
			}

			if (!bFound || Name == PreferredName)
			{
				OutFunction.ReturnType = ReturnType;
				OutFunction.Name = Name;
				ArgumentList = Matcher.GetCaptureGroup(3);
				bFound = true;
			}
			if (Name == PreferredName)
			{
				break;
			}
		}

		if (!bFound)
		{
			OutError = LOCTEXT("NoFunction", "No function definition found in the shader file.");
			return false;
		}

		OutFunction.ArgumentNames.Reset();
		TArray<FString> Arguments;
		ArgumentList.ParseIntoArray(Arguments, TEXT(","), true);
		for (FString Argument : Arguments)
		{
			// Drop default values and semantics, then take the last token as the argument name.
			int32 Cut = INDEX_NONE;
			if (Argument.FindChar(TEXT('='), Cut) || Argument.FindChar(TEXT(':'), Cut))
			{
				Argument.LeftInline(Cut);
			}

			TArray<FString> Tokens;
			Argument.ParseIntoArrayWS(Tokens);
			if (Tokens.IsEmpty())
			{
				continue;
			}
			if (Tokens.Contains(TEXT("out")) || Tokens.Contains(TEXT("inout")))
			{
				OutError = FText::Format(LOCTEXT("OutParameter", "'{0}' has an out/inout parameter, which the Custom node inputs cannot represent."), FText::FromString(OutFunction.Name));
				return false;
			}

			FString ArgumentName = Tokens.Last();
			ArgumentName.RemoveFromEnd(TEXT("]"));
			if (ArgumentName.FindChar(TEXT('['), Cut))
			{
				ArgumentName.LeftInline(Cut);
			}
			if (!IsValidIdentifier(ArgumentName) || Tokens.Num() < 2)
			{
				OutError = FText::Format(LOCTEXT("BadArgument", "Cannot read the argument '{0}'."), FText::FromString(Argument.TrimStartAndEnd()));
				return false;
			}
			OutFunction.ArgumentNames.Add(ArgumentName);
		}

		return true;
	}
}

namespace CustomHLSLTools
{

bool CreateShaderFile(UMaterialExpressionCustom& Expression, const FString& RelativeName, FText& OutError)
{
	FString Name;
	if (!NormalizeRelativeName(RelativeName, Name, OutError))
	{
		return false;
	}

	const FString FunctionName = ToIdentifier(FPaths::GetBaseFilename(Name));
	const FString VirtualPath = FString::Printf(TEXT("/Project/%s.ush"), *Name);
	const FString FilePath = FPaths::Combine(GetProjectShaderDirectory(), Name + TEXT(".ush"));

	if (FPaths::FileExists(FilePath))
	{
		OutError = FText::Format(LOCTEXT("AlreadyExists", "File already exists: {0}"), FText::FromString(FilePath));
		return false;
	}
	if (!EnsureProjectShaderDirectory(OutError))
	{
		return false;
	}

	// A node without inputs gets a single placeholder so that the generated function has something to work with.
	TArray<FCustomInput> Inputs = Expression.Inputs;
	Inputs.RemoveAll([](const FCustomInput& Input) { return Input.InputName.IsNone(); });
	if (Inputs.IsEmpty())
	{
		FCustomInput& Input = Inputs.AddDefaulted_GetRef();
		Input.InputName = TEXT("A");
	}

	const ECustomMaterialOutputType OutputType = Expression.OutputType;
	if (!FFileHelper::SaveStringToFile(BuildShaderSource(FunctionName, OutputType, Inputs), *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = FText::Format(LOCTEXT("WriteFailed", "Failed to write file: {0}"), FText::FromString(FilePath));
		return false;
	}
	FlushShaderFileCache();

	Expression.Modify();
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Inputs), [&]()
	{
		Expression.Inputs = Inputs;
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, IncludeFilePaths), [&]()
	{
		Expression.IncludeFilePaths.AddUnique(VirtualPath);
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Code), [&]()
	{
		Expression.Code = BuildCall(FunctionName, Inputs);
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Description), [&]()
	{
		Expression.Description = FunctionName;
	});

	return true;
}

bool SetupNodeFromShaderFile(UMaterialExpressionCustom& Expression, const FString& VirtualPath, FText& OutError)
{
	const FString File = ResolveVirtualShaderPath(VirtualPath, OutError);
	if (File.IsEmpty())
	{
		return false;
	}

	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *File))
	{
		OutError = FText::Format(LOCTEXT("ReadFailed", "Failed to read file: {0}"), FText::FromString(File));
		return false;
	}

	FParsedFunction Function;
	if (!ParseFunction(Source, FPaths::GetBaseFilename(File), Function, OutError))
	{
		return false;
	}

	ECustomMaterialOutputType OutputType;
	if (!TryParseReturnType(Function.ReturnType, OutputType))
	{
		OutError = FText::Format(LOCTEXT("UnsupportedReturnType", "Unsupported return type '{0}' (supported: float, float2, float3, float4, FMaterialAttributes)."), FText::FromString(Function.ReturnType));
		return false;
	}

	TArray<FCustomInput> Inputs;
	for (const FString& ArgumentName : Function.ArgumentNames)
	{
		const FName InputName(*ArgumentName);
		const FCustomInput* Existing = Expression.Inputs.FindByPredicate([InputName](const FCustomInput& Input) { return Input.InputName == InputName; });
		if (Existing)
		{
			Inputs.Add(*Existing);
		}
		else
		{
			Inputs.AddDefaulted_GetRef().InputName = InputName;
		}
	}

	Expression.Modify();
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Inputs), [&]()
	{
		Expression.Inputs = Inputs;
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, OutputType), [&]()
	{
		Expression.OutputType = OutputType;
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, IncludeFilePaths), [&]()
	{
		Expression.IncludeFilePaths.AddUnique(VirtualPath);
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Code), [&]()
	{
		Expression.Code = BuildCall(Function.Name, Inputs);
	});
	ModifyProperty(Expression, GET_MEMBER_NAME_CHECKED(UMaterialExpressionCustom, Description), [&]()
	{
		Expression.Description = Function.Name;
	});

	return true;
}

}

#undef LOCTEXT_NAMESPACE
