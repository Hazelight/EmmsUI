#include "MMClassDetailCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateBrush.h"

IDetailCategoryBuilder& UMMClassDetailCustomization::GetCategory(FName CategoryName)
{
	IDetailCategoryBuilder** BuilderPtr = ActiveCategories.Find(CategoryName);
	if (BuilderPtr != nullptr)
		return **BuilderPtr;

	IDetailCategoryBuilder& Category = ActiveDetailBuilder->EditCategory(CategoryName);
	ActiveCategories.Add(CategoryName, &Category);
	return Category;
}

void UMMClassDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	ActiveDetailBuilder = &DetailBuilder;
	UsedUserWidgets.Reset();
	UsedImmediateWidgets.Reset();

	TArray<TWeakObjectPtr<UObject>> ActiveObjects;
	DetailBuilder.GetObjectsBeingCustomized(ActiveObjects);
	for (auto Ptr : ActiveObjects)
	{
		if (Ptr.IsValid())
			ObjectsBeingCustomized.Add(Ptr.Get());
	}

	FEditorScriptExecutionGuard ScopeAllowScript;
	BP_CustomizeDetails();

	ActiveDetailBuilder = nullptr;
	ActiveCategories.Reset();
}

UWorld* UMMClassDetailCustomization::GetWorld() const
{
	if (ObjectsBeingCustomized.Num() != 0 && ObjectsBeingCustomized[0] != nullptr)
		return ObjectsBeingCustomized[0]->GetWorld();
	return nullptr;
}

void UMMClassDetailCustomization::Tick(float DeltaTime)
{
	if (!IsValid(this) || IsUnreachable())
		return;

	if (GetClass()->ScriptTypePtr != nullptr) 
	{
		FEditorScriptExecutionGuard ScopeAllowScript;
		BP_Tick(DeltaTime);
	}
		
}

TStatId UMMClassDetailCustomization::GetStatId() const
{
	return GetStatID();
}

UObject* UMMClassDetailCustomization::GetCustomizedObject()
{
	if (ObjectsBeingCustomized.Num() == 0)
		return nullptr;
	return ObjectsBeingCustomized[0];
}

void UMMClassDetailCustomization::HideProperty(FName PropertyName)
{
	if (ActiveDetailBuilder == nullptr)
		return;

	TSharedPtr<IPropertyHandle> PropertyHandle = ActiveDetailBuilder->GetProperty(PropertyName);
	if (PropertyHandle.IsValid())
		PropertyHandle->MarkHiddenByCustomization();
}

void UMMClassDetailCustomization::HideCategory(FName CategoryName)
{
	if (ActiveDetailBuilder == nullptr)
		return;
	ActiveDetailBuilder->HideCategory(CategoryName);
}

void UMMClassDetailCustomization::EditCategory(FName CategoryName, const FString& CategoryDisplayName, EMMDetailCategoryType CategoryType)
{
	if (ActiveDetailBuilder == nullptr)
		return;
	IDetailCategoryBuilder& Category = ActiveDetailBuilder->EditCategory(CategoryName, FText::FromString(CategoryDisplayName), (ECategoryPriority::Type)CategoryType);
	ActiveCategories.Add(CategoryName, &Category);
}

void UMMClassDetailCustomization::SetCategorySortOrder(FName CategoryName, int32 SortOrder)
{
    if (ActiveDetailBuilder == nullptr)
        return;
    
    IDetailCategoryBuilder& Category = GetCategory(CategoryName);
    Category.SetSortOrder(SortOrder);
}


void UMMClassDetailCustomization::AddAllCategoryDefaultProperties(FName CategoryName, bool bSimpleProperties, bool bAdvancedProperties)
{
	if (ActiveDetailBuilder == nullptr)
		return;

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);

	TArray<TSharedRef<IPropertyHandle>> Properties;
	Category.GetDefaultProperties(Properties, bSimpleProperties, bAdvancedProperties);

	for (TSharedPtr<IPropertyHandle> Property : Properties)
	{
		Category.AddProperty(Property);
	}
}

void UMMClassDetailCustomization::AddDefaultPropertiesFromOtherCategory(FName AddToCategory, FName PropertiesFromCategory, bool bSimpleProperties, bool bAdvancedProperties)
{
	if (ActiveDetailBuilder == nullptr)
		return;

	IDetailCategoryBuilder& SourceCategory = GetCategory(PropertiesFromCategory);
	IDetailCategoryBuilder& DestCategory = GetCategory(AddToCategory);

	TArray<TSharedRef<IPropertyHandle>> Properties;
	SourceCategory.GetDefaultProperties(Properties, bSimpleProperties, bAdvancedProperties);

	for (TSharedPtr<IPropertyHandle> Property : Properties)
	{
		DestCategory.AddProperty(Property);
	}
}

void UMMClassDetailCustomization::AddDefaultProperty(FName CategoryName, FName PropertyName)
{
	if (ActiveDetailBuilder == nullptr)
		return;

	TSharedPtr<IPropertyHandle> Property = ActiveDetailBuilder->GetProperty(PropertyName);
	if (Property.IsValid())
	{
		IDetailCategoryBuilder& Category = GetCategory(CategoryName);
		Category.AddProperty(Property);
	}
}

UUserWidget* UMMClassDetailCustomization::AddRow(FName CategoryName, TSubclassOf<UUserWidget> RowWidget, const FString& FilterString, bool bAdvancedDisplay)
{
	if (ActiveDetailBuilder == nullptr)
		return nullptr;
	if (RowWidget.Get() == nullptr)
		return nullptr;

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);

	UUserWidget* UserWidget = NewObject<UUserWidget>(GetTransientPackage(), RowWidget);
	UserWidget->Initialize();
	UsedUserWidgets.Add(UserWidget);

	FDetailWidgetRow& Row = Category.AddCustomRow(FText::FromString(FilterString), bAdvancedDisplay);
	Row.WholeRowContent()[ UserWidget->TakeWidget() ];
	return UserWidget;
}

UUserWidget* UMMClassDetailCustomization::AddProperty(FName CategoryName, const FString& PropertyName,
	TSubclassOf<UUserWidget> ValueWidget, bool bAdvancedDisplay)
{
	if (ActiveDetailBuilder == nullptr)
		return nullptr;

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	FDetailWidgetRow& Row = Category.AddCustomRow(FText::FromString(PropertyName), bAdvancedDisplay);

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

	UUserWidget* UserWidget = nullptr;
	if (ValueWidget.Get() != nullptr)
	{
		UserWidget = NewObject<UUserWidget>(GetTransientPackage(), ValueWidget);
		UserWidget->Initialize();
		UsedUserWidgets.Add(UserWidget);

		Row.ValueContent()[ UserWidget->TakeWidget() ];
	}

	return UserWidget;
}

FMMDetailProperty UMMClassDetailCustomization::AddCustom(FName CategoryName,
	TSubclassOf<UUserWidget> NameWidget, TSubclassOf<UUserWidget> ValueWidget, const FString& FilterString,
	bool bAdvancedDisplay)
{
	if (ActiveDetailBuilder == nullptr)
		return FMMDetailProperty();

	FMMDetailProperty OutWidgets;

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	FDetailWidgetRow& Row = Category.AddCustomRow(FText::FromString(FilterString), bAdvancedDisplay);

	if (ValueWidget.Get() != nullptr)
	{
		OutWidgets.ValueWidget = NewObject<UUserWidget>(GetTransientPackage(), ValueWidget);
		OutWidgets.ValueWidget->Initialize();
		UsedUserWidgets.Add(OutWidgets.ValueWidget);

		Row.ValueContent()[ OutWidgets.ValueWidget->TakeWidget() ];
	}

	if (NameWidget.Get() != nullptr)
	{
		OutWidgets.NameWidget = NewObject<UUserWidget>(GetTransientPackage(), NameWidget);
		OutWidgets.NameWidget->Initialize();
		UsedUserWidgets.Add(OutWidgets.NameWidget);

		Row.NameContent()[ OutWidgets.NameWidget->TakeWidget() ];
	}

	return OutWidgets;
}

UMMWidget* UMMClassDetailCustomization::AddImmediateRow(FName CategoryName, const FString& FilterString,
	bool bAdvancedDisplay)
{
	if (ActiveDetailBuilder == nullptr)
		return nullptr;

	UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
	UsedImmediateWidgets.Add(ImmWidget);	

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	FDetailWidgetRow& Row = Category.AddCustomRow(FText::FromString(FilterString), bAdvancedDisplay);
	Row.WholeRowContent()[ ImmWidget->TakeWidget() ];
	return ImmWidget;
}

UMMWidget* UMMClassDetailCustomization::AddImmediateProperty(FName CategoryName,
	const FString& PropertyName, bool bAdvancedDisplay)
{
	if (ActiveDetailBuilder == nullptr)
		return nullptr;

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	FDetailWidgetRow& Row = Category.AddCustomRow(FText::FromString(PropertyName), bAdvancedDisplay);

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

FMMDetailImmediateProperty UMMClassDetailCustomization::AddImmediateCustom(FName CategoryName,
	const FString& FilterString, bool bAdvancedDisplay)
{
	if (ActiveDetailBuilder == nullptr)
		return FMMDetailImmediateProperty();

	FMMDetailImmediateProperty OutWidgets;

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	FDetailWidgetRow& Row = Category.AddCustomRow(FText::FromString(FilterString), bAdvancedDisplay);

	{
		UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
		UsedImmediateWidgets.Add(ImmWidget);
		Row.NameContent()[ ImmWidget->TakeWidget() ];
		OutWidgets.NameDrawer = ImmWidget;
	}

	{
		UMMWidget* ImmWidget = NewObject<UMMWidget>(GetTransientPackage());
		UsedImmediateWidgets.Add(ImmWidget);
		Row.ValueContent()[ ImmWidget->TakeWidget() ];
		OutWidgets.ValueDrawer = ImmWidget;
	}

	return OutWidgets;
}

void UMMClassDetailCustomization::NotifyPropertyModified(UObject* Object, FName PropertyName)
{
	if (Object == nullptr)
		return;
	auto* Property = Object->GetClass()->FindPropertyByName(PropertyName);
	if (Property == nullptr)
		return;

	FPropertyChangedEvent PropertyChangedEvent(Property);
	Object->PostEditChangeProperty(PropertyChangedEvent);
}

void UMMClassDetailCustomization::AddExternalObject(FName CategoryName, UObject* Object, bool bHideRootObjectNode)
{
	if (ActiveDetailBuilder == nullptr)
		return;

	FAddPropertyParams Params;
	Params.HideRootObjectNode(bHideRootObjectNode);

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	Category.AddExternalObjects(
		TArray<UObject*>{Object},
		EPropertyLocation::Default,
		Params
	);
}

void UMMClassDetailCustomization::AddExternalObjectProperty(FName CategoryName, UObject* Object, FName PropertyName)
{
	if (ActiveDetailBuilder == nullptr)
		return;

	FAddPropertyParams Params;
	Params.ForceShowProperty();

	IDetailCategoryBuilder& Category = GetCategory(CategoryName);
	Category.AddExternalObjectProperty(
		TArray<UObject*>{Object},
		PropertyName,
		EPropertyLocation::Default,
		Params
	);
}

void UMMClassDetailCustomization::ForceRefresh()
{
	if (!IsValid(this) || IsUnreachable())
		return;
	if (LastDetailBuilder != nullptr)
	{
		IDetailLayoutBuilder* Builder = LastDetailBuilder;
		LastDetailBuilder = nullptr;
		Builder->ForceRefreshDetails();
	}
}

void UMMClassDetailCustomization::DestroyDetails()
{
	bDestroyed = true;

	for (UMMWidget* ImmWidget : UsedImmediateWidgets)
	{
		if (ImmWidget == nullptr)
			continue;
		if (ImmWidget->GetParent() != nullptr)
			ImmWidget->RemoveFromParent();
	}
	UsedImmediateWidgets.Reset();

	for (UUserWidget* UserWidget : UsedUserWidgets)
	{
		if (UserWidget == nullptr)
			continue;
		if (UserWidget->GetParent() != nullptr)
			UserWidget->RemoveFromParent();
	}
	UsedUserWidgets.Reset();

}

FMMDummyDetailCustomization::FMMDummyDetailCustomization(TSubclassOf<UMMClassDetailCustomization> CustomizationClass)
{
	Customization = NewObject<UMMClassDetailCustomization>(GetTransientPackage(), CustomizationClass);
}

void FMMDummyDetailCustomization::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	// Don't kill asset details
	if (Customization != nullptr
		&& (!IsValid(Customization->GetCustomizedObject()) || !Customization->GetCustomizedObject()->IsAsset())
	)
	{
		if (Customization->GetWorld() != World)
		{
			// Do not kill the customization if the world does not match, (e.g., in case of a thumbnail widget world being destroyed)
			return;
		}
		
		Customization->LastDetailBuilder = nullptr;
		Customization->ObjectsBeingCustomized.Reset();
		Customization->MarkAsGarbage();
		Customization = nullptr;
	}
}

FMMDummyDetailCustomization::~FMMDummyDetailCustomization()
{
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);

	if (Customization != nullptr)
	{
		Customization->LastDetailBuilder = nullptr;
		if (!IsEngineExitRequested())
		{
			Customization->DestroyDetails();
			Customization->MarkAsGarbage();
		}
	}
}

void FMMDummyDetailCustomization::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Customization);
}

void FMMDummyDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	FWorldDelegates::OnWorldCleanup.AddSP(this, &FMMDummyDetailCustomization::OnWorldCleanup);

	if (Customization != nullptr)
	{
		Customization->LastDetailBuilder = &DetailBuilder;
		Customization->CustomizeDetails(DetailBuilder);
	}
}

