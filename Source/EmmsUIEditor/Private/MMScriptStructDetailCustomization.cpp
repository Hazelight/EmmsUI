#include "MMScriptStructDetailCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"

TSharedRef<IPropertyTypeCustomization> FMMScriptStructDetailCustomizationWrapper::MakeInstance(TSubclassOf<UMMScriptStructDetailCustomization> CustomizationClass)
{
	auto Wrapper = MakeShared<FMMScriptStructDetailCustomizationWrapper>();
	Wrapper->Customization = TStrongObjectPtr<UMMScriptStructDetailCustomization>(
		NewObject<UMMScriptStructDetailCustomization>(GetTransientPackage(), CustomizationClass->GetMostUpToDateClass())
	);
	return Wrapper;
}

FMMScriptStructDetailCustomizationWrapper::~FMMScriptStructDetailCustomizationWrapper()
{
	if (Customization.IsValid())
	{
		Customization->Wrapper = nullptr;
		FEditorScriptExecutionGuard ScopeAllowScript;
		Customization->OnRemoved();
		Customization->DeleteCustomization();
		Customization.Reset();
	}
}

void FMMScriptStructDetailCustomizationWrapper::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	FEditorScriptExecutionGuard ScopeAllowScript;
	TGuardValue ScopeActiveRow(ActiveRow, &HeaderRow);
	TGuardValue ScopeActiveUtils(ActiveUtils, &CustomizationUtils);

	StructPropertyHandle = PropertyHandle;

	TArray<UObject*> Outers;
	PropertyHandle->GetOuterObjects(Outers);

	Customization->OuterObjects.Reset();
	for (UObject* OuterObject : Outers)
		Customization->OuterObjects.Add(OuterObject);

	Customization->Wrapper = this;
	Customization->CustomizeHeader();
	Customization->FinishCustomizingHeader();
}

void FMMScriptStructDetailCustomizationWrapper::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FEditorScriptExecutionGuard ScopeAllowScript;
	TGuardValue ScopeActiveRow(ActiveChildBuilder, &ChildBuilder);
	TGuardValue ScopeActiveUtils(ActiveUtils, &StructCustomizationUtils);

	StructPropertyHandle = PropertyHandle;

	TArray<UObject*> Outers;
	PropertyHandle->GetOuterObjects(Outers);

	Customization->OuterObjects.Reset();
	for (UObject* OuterObject : Outers)
		Customization->OuterObjects.Add(OuterObject);

	Customization->Wrapper = this;
	Customization->CustomizeChildren();
	Customization->FinishCustomizingChildren();
}

void UMMScriptStructDetailCustomization::DefaultNameContent()
{
	if (Wrapper == nullptr || Wrapper->ActiveRow == nullptr)
		return;

	if (Wrapper->ActiveRow->HasNameContent())
	{
		Wrapper->ActiveRow->NameContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				Wrapper->ActiveRow->NameContent().Widget
			]
			+SVerticalBox::Slot()
			[
				Wrapper->StructPropertyHandle->CreatePropertyNameWidget()
			]
		];
	}
	else
	{
		Wrapper->ActiveRow->NameContent()
		[
			Wrapper->StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
}

UMMWidget* UMMScriptStructDetailCustomization::ImmediateNameContent()
{
	if (Wrapper == nullptr || Wrapper->ActiveRow == nullptr)
		return nullptr;

	UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
	UsedImmediateWidgets.Add(ImmWidget);

	if (Wrapper->ActiveRow->HasNameContent())
	{
		Wrapper->ActiveRow->NameContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				Wrapper->ActiveRow->NameContent().Widget
			]
			+SVerticalBox::Slot()
			[
				ImmWidget->TakeWidget()
			]
		];
	}
	else
	{
		Wrapper->ActiveRow->NameContent()
		[
			ImmWidget->TakeWidget()
		];
	}

	return ImmWidget;
}

void UMMScriptStructDetailCustomization::DefaultValueContent()
{
	if (Wrapper == nullptr || Wrapper->ActiveRow == nullptr)
		return;

	if (Wrapper->ActiveRow->HasValueContent())
	{
		Wrapper->ActiveRow->ValueContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				Wrapper->ActiveRow->ValueContent().Widget
			]
			+SVerticalBox::Slot()
			[
				Wrapper->StructPropertyHandle->CreatePropertyValueWidget()
			]
		];
	}
	else
	{
		Wrapper->ActiveRow->ValueContent()
		[
			Wrapper->StructPropertyHandle->CreatePropertyValueWidget()
		];
	}
}

UMMWidget* UMMScriptStructDetailCustomization::ImmediateValueContent()
{
	if (Wrapper == nullptr || Wrapper->ActiveRow == nullptr)
		return nullptr;

	UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
	UsedImmediateWidgets.Add(ImmWidget);

	if (Wrapper->ActiveRow->HasValueContent())
	{
		Wrapper->ActiveRow->ValueContent()
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			[
				Wrapper->ActiveRow->ValueContent().Widget
			]
			+SVerticalBox::Slot()
			[
				ImmWidget->TakeWidget()
			]
		];
	}
	else
	{
		Wrapper->ActiveRow->ValueContent()
		[
			ImmWidget->TakeWidget()
		];
	}

	return ImmWidget;
}

void UMMScriptStructDetailCustomization::DefaultWholeRowContent()
{
	DefaultNameContent();
	DefaultValueContent();
}

UMMWidget* UMMScriptStructDetailCustomization::ImmediateWholeRowContent()
{
	if (Wrapper == nullptr || Wrapper->ActiveRow == nullptr)
		return nullptr;

	UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
	UsedImmediateWidgets.Add(ImmWidget);

	Wrapper->ActiveRow->WholeRowContent()
	.VAlign(VAlign_Center)
	[
		ImmWidget->TakeWidget()
	];

	return ImmWidget;
}

void UMMScriptStructDetailCustomization::AddAllPropertyChildRows()
{
	if (Wrapper == nullptr || Wrapper->ActiveChildBuilder == nullptr)
		return;

	uint32 NumChildren;
	if (Wrapper->StructPropertyHandle->GetNumChildren(NumChildren) == FPropertyAccess::Success)
	{
		for (uint32 Index = 0; Index < NumChildren; Index++)
		{
			TSharedPtr<IPropertyHandle> ChildProperty = Wrapper->StructPropertyHandle->GetChildHandle(Index);
			Wrapper->ActiveChildBuilder->AddProperty(ChildProperty.ToSharedRef());
		}
	}
}

void UMMScriptStructDetailCustomization::AddPropertyChildRow(FName PropertyName)
{
	if (Wrapper == nullptr || Wrapper->ActiveChildBuilder == nullptr)
		return;

	auto Child = Wrapper->StructPropertyHandle->GetChildHandle(PropertyName, false);
	if (Child.IsValid())
		Wrapper->ActiveChildBuilder->AddProperty(Child.ToSharedRef());
}

UMMWidget* UMMScriptStructDetailCustomization::AddImmediateChildProperty(FString PropertyName)
{
	if (Wrapper == nullptr || Wrapper->ActiveChildBuilder == nullptr)
		return nullptr;

	FDetailWidgetRow& Row = Wrapper->ActiveChildBuilder->AddCustomRow(FText::FromString(PropertyName));
	Row.NameContent()[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.Padding( FMargin( 0, 1, 0, 1 ) )
		.FillWidth(1)
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::GetBrush( TEXT("PropertyWindow.NoOverlayColor") ) )
			.Padding( FMargin( 0.0f, 2.0f ) )
			.VAlign(VAlign_Center)
			[
				SNew( STextBlock )
				.Text( FText::FromString(PropertyName) )
				.Font( FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont") ) )
			]
		]
	];

	UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
	UsedImmediateWidgets.Add(ImmWidget);

	Row.ValueContent()[ ImmWidget->TakeWidget() ];

	return ImmWidget;
}

UMMWidget* UMMScriptStructDetailCustomization::AddImmediateChildRow()
{
	if (Wrapper == nullptr || Wrapper->ActiveChildBuilder == nullptr)
		return nullptr;

	UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
	UsedImmediateWidgets.Add(ImmWidget);

	Wrapper->ActiveChildBuilder->AddCustomRow(FText())
		.WholeRowContent()
		.VAlign(VAlign_Center)
		[
			ImmWidget->TakeWidget()
		];

	return ImmWidget;
}

bool UMMScriptStructDetailCustomization::HasStructValue()
{
	if (Wrapper == nullptr || Wrapper->StructPropertyHandle == nullptr)
		return false;

	void* StructPtr = nullptr;
	Wrapper->StructPropertyHandle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
	{
		StructPtr = RawData;
		return false;
	});

	if (StructPtr == nullptr)
		return false;

	return true;
}

const FScriptStructWildcard& UMMScriptStructDetailCustomization::GetStructValue(UScriptStruct* StructType)
{
	if (StructType != DetailStruct)
	{
		static FScriptStructWildcard ErrorStruct;
		FAngelscriptManager::Throw("Invalid struct type for detail customization");
		return ErrorStruct;
	}

	if (Wrapper == nullptr || Wrapper->StructPropertyHandle == nullptr)
	{
		static FScriptStructWildcard ErrorStruct;
		FAngelscriptManager::Throw("Attempted to access struct value after customization was destroyed");
		return ErrorStruct;
	}

	void* StructPtr = nullptr;
	Wrapper->StructPropertyHandle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
	{
		StructPtr = RawData;
		return false;
	});

	if (StructPtr == nullptr)
	{
		static FScriptStructWildcard ErrorStruct;
		FAngelscriptManager::Throw("No struct data available");
		return ErrorStruct;
	}

	return *(FScriptStructWildcard*)StructPtr;
}

void UMMScriptStructDetailCustomization::SetStructValue(FAngelscriptAnyStructParameter NewValue)
{
	if (Wrapper == nullptr || Wrapper->StructPropertyHandle == nullptr)
	{
		FAngelscriptManager::Throw("Attempted to set struct value after customization was destroyed");
		return;
	}

	if (NewValue.InstancedStruct.GetScriptStruct() != DetailStruct)
	{
		FAngelscriptManager::Throw("Invalid struct type for detail customization");
		return;
	}

	Wrapper->StructPropertyHandle->NotifyPreChange();

	Wrapper->StructPropertyHandle->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 NumDatas)
	{
		DetailStruct->CopyScriptStruct(RawData, NewValue.InstancedStruct.GetMemory());
		return true;
	});

	Wrapper->StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	Wrapper->StructPropertyHandle->NotifyFinishedChangingProperties();
}

UWorld* UMMScriptStructDetailCustomization::GetWorld() const
{
	for (auto Object : OuterObjects)
	{
		if (Object.IsValid())
		{
			UWorld* OuterWorld = Object->GetWorld();
			if (OuterWorld != nullptr)
				return OuterWorld;
		}
	}

	return nullptr;
}

void UMMScriptStructDetailCustomization::DeleteCustomization()
{
	for (UMMWidget* ImmWidget : UsedImmediateWidgets)
	{
		if (ImmWidget->GetParent() != nullptr)
			ImmWidget->RemoveFromParent();
	}
	UsedImmediateWidgets.Reset();
}

void UMMScriptStructDetailCustomization::FinishCustomizingHeader()
{
}

void UMMScriptStructDetailCustomization::FinishCustomizingChildren()
{
}

void UMMScriptStructDetailCustomization::Tick(float DeltaTime)
{
	if (!IsValid(this) || IsUnreachable() || Wrapper == nullptr)
		return;

	if (GetClass()->ScriptTypePtr != nullptr)
	{
		if (HasStructValue())
		{
			FEditorScriptExecutionGuard ScopeAllowScript;
			BP_Tick(DeltaTime);
		}
	}
}

TStatId UMMScriptStructDetailCustomization::GetStatId() const
{
	return GetStatID();
}
