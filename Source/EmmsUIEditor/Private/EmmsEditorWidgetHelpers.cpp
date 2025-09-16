#include "EmmsEditorWidgetHelpers.h"
#include "EmmsWidgetHelpers.h"
#include "EmmsEventListener.h"
#include "AngelscriptManager.h"
#include "InstancedStructDetails.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"

#include "Components/AssetThumbnailWidget.h"

#include "Modules/ModuleManager.h"
#include "Components/DetailsView.h"
#include "Widgets/Layout/SBox.h"

#include "AngelscriptBinds.h"

void UEmmsEditorWidgetHelpers::SetDetailsViewObject(FEmmsWidgetHandle* Widget, UObject* Object)
{
	if (Widget->Element == nullptr)
		return;
	UDetailsView* DetailsView = Cast<UDetailsView>(Widget->Element->UMGWidget);
	if (DetailsView == nullptr)
		return;
	DetailsView->SetObject(Object);
}

void UEmmsEditorWidgetHelpers::SetDetailsViewStruct_NoTitle(FEmmsWidgetHandle* Widget, void* DataPtr, int TypeId)
{
	SetDetailsViewStruct(Widget, DataPtr, TypeId, TEXT(""));
}

void UEmmsEditorWidgetHelpers::SetDetailsViewStruct(FEmmsWidgetHandle* Widget, void* DataPtr, int TypeId, const FString& HeaderTitle)
{
	if (Widget->Element == nullptr)
		return;
	UScriptStruct* StructType = Cast<UScriptStruct>(FAngelscriptManager::Get().GetUnrealStructFromAngelscriptTypeId(TypeId));
	if (StructType == nullptr)
	{
		FAngelscriptManager::Throw("Must pass a reference to an unreal struct");
		return;
	}

	UDetailsView* DetailsView = Cast<UDetailsView>(Widget->Element->UMGWidget);
	if (DetailsView == nullptr)
		return;

	UEmmsEditableInstancedStruct* EditableObject = Cast<UEmmsEditableInstancedStruct>(DetailsView->GetObject());
	if (EditableObject == nullptr)
	{
		EditableObject = NewObject<UEmmsEditableInstancedStruct>(GetTransientPackage(), NAME_None);
		DetailsView->SetObject(EditableObject);
	}

	// If the input data has changed, update the data in the editable object as well
	if (EditableObject->PrevInputValue.GetScriptStruct() != StructType
		|| !StructType->CompareScriptStruct(DataPtr, EditableObject->PrevInputValue.GetMemory(), 0)
		|| EditableObject->Struct.GetScriptStruct() != StructType)
	{
		EditableObject->PrevInputValue.InitializeAs(StructType, (uint8*)DataPtr);
		EditableObject->Struct.InitializeAs(StructType, (uint8*)DataPtr);

		FPropertyChangedEvent ChangeEvent(EditableObject->GetClass()->FindPropertyByName("Struct"));
		EditableObject->PostEditChangeProperty(ChangeEvent);
	}

	// Copy output data back from the object
	StructType->CopyScriptStruct(DataPtr, EditableObject->Struct.GetMemory());

	if (EditableObject->HeaderTitle != HeaderTitle)
	{
		EditableObject->HeaderTitle = HeaderTitle;

		FPropertyChangedEvent ChangeEvent(EditableObject->GetClass()->FindPropertyByName("HeaderTitle"));
		EditableObject->PostEditChangeProperty(ChangeEvent);

		// HACK: This refreshes the details view to update the title, don't see a better way of triggering a refresh atm
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.ReplaceViewedObjects({{EditableObject, EditableObject}});
	}
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_EmmsEditorWidgetHelpers((int32)FAngelscriptBinds::EOrder::Late + 250, []
{
	if (!FAngelscriptManager::bUseEditorScripts)
		return;

	{
		auto mmUDetailsView_ = FAngelscriptBinds::ExistingClass("mm<UDetailsView>");
		mmUDetailsView_.Method("void SetObject(UObject Object) const", &UEmmsEditorWidgetHelpers::SetDetailsViewObject);
		FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
		mmUDetailsView_.Method("void SetStruct(?& StructRef) const", &UEmmsEditorWidgetHelpers::SetDetailsViewStruct_NoTitle);
		FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
		mmUDetailsView_.Method("void SetStruct(?& StructRef, const FString& HeaderTitle) const", &UEmmsEditorWidgetHelpers::SetDetailsViewStruct);
		FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
	}

	{
		FAngelscriptBinds::FNamespace ns("mm");
		FAngelscriptBinds::BindGlobalFunction("mm<UAssetThumbnailWidget> AssetThumbnail(UObject Object, int32 Resolution = 64)", &UEmmsEditorWidgetHelpers::AssetThumbnailFromObject);
		FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
		FAngelscriptBinds::BindGlobalFunction("mm<UAssetThumbnailWidget> AssetThumbnail(const FAssetData& AssetData, int32 Resolution = 64)", &UEmmsEditorWidgetHelpers::AssetThumbnailFromAssetData);
		FAngelscriptBinds::SetPreviousBindIsEditorOnly(true);
	}
});

TSharedRef<IDetailCustomization> FEmmsEditableInstancedStructDetailCustomization::MakeInstance()
{
	return MakeShareable(new FEmmsEditableInstancedStructDetailCustomization);
}

void FEmmsEditableInstancedStructDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideCategory("Struct");

	UEmmsEditableInstancedStruct* EditableObject = nullptr;

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	for (auto Object : Objects)
	{
		if (Object.IsValid() && Object->IsA<UEmmsEditableInstancedStruct>())
		{
			EditableObject = CastChecked<UEmmsEditableInstancedStruct>(Object);
			break;
		}
	}

	auto StructProperty = DetailBuilder.GetProperty("Struct");

	TSharedRef<FInstancedStructDataDetails> DataDetails = MakeShared<FInstancedStructDataDetails>(StructProperty);
	auto& Category = DetailBuilder.EditCategory("Details");

	if (EditableObject != nullptr)
	{
		if (!EditableObject->HeaderTitle.IsEmpty())
			Category.SetDisplayName(FText::FromString(EditableObject->HeaderTitle));
		else if (auto* ScriptStruct = EditableObject->Struct.GetScriptStruct())
			Category.SetDisplayName(FText::FromString(FName::NameToDisplayString(ScriptStruct->GetName(), false)));
		else
			Category.SetDisplayName(FText::FromString("Details"));
	}
	else
	{
		Category.SetDisplayName(FText::FromString("Details"));
	}

	Category.InitiallyCollapsed(false);
	Category.AddCustomBuilder(DataDetails);
}

FEmmsWidgetHandle UEmmsEditorWidgetHelpers::AssetThumbnailFromObject(UObject* Object, int32 Resolution)
{
    FEmmsWidgetHandle Widget = UEmmsStatics::AddWidget(UAssetThumbnailWidget::StaticClass());
    if (Widget.Element == nullptr)
        return Widget;

    UAssetThumbnailWidget* ThumbnailWidget = Cast<UAssetThumbnailWidget>(Widget.Element->UMGWidget);
    if (ThumbnailWidget)
    {
        if (IsAssetThumbnailWidgetChanged(ThumbnailWidget, FAssetData(Object), Resolution))
        {
            const FIntPoint DesiredRes(Resolution, Resolution);
            ThumbnailWidget->SetResolution(DesiredRes);
            ThumbnailWidget->SetAssetByObject(Object);
        }
    }
    return Widget;
}

FEmmsWidgetHandle UEmmsEditorWidgetHelpers::AssetThumbnailFromAssetData(const FAssetData& AssetData, int32 Resolution)
{
	FEmmsWidgetHandle Widget = UEmmsStatics::AddWidget(UAssetThumbnailWidget::StaticClass());
	if (Widget.Element == nullptr)
		return Widget;

	UAssetThumbnailWidget* ThumbnailWidget = Cast<UAssetThumbnailWidget>(Widget.Element->UMGWidget);
	if (ThumbnailWidget)
	{
	        if (IsAssetThumbnailWidgetChanged(ThumbnailWidget, AssetData, Resolution))
	        {
	            const FIntPoint DesiredRes(Resolution, Resolution);
	            ThumbnailWidget->SetResolution(DesiredRes);
	            ThumbnailWidget->SetAsset(AssetData);
	        }
	}
	return Widget;
}

bool UEmmsEditorWidgetHelpers::IsAssetThumbnailWidgetChanged(UAssetThumbnailWidget* ThumbnailWidget, const FAssetData& NewAssetData, int32 NewResolution)
{
    if (ThumbnailWidget == nullptr)
        return true;
    
    const FIntPoint NewRes(NewResolution, NewResolution);
    const bool bResChanged = ThumbnailWidget->GetResolution() != NewRes;
    
    if (bResChanged)
    {
        return true;
    }
    
    // Access the private AssetToShow property via reflection
    static FStructProperty* AssetProp = FindFProperty<FStructProperty>(UAssetThumbnailWidget::StaticClass(), FName("AssetToShow"));

    ensureMsgf(AssetProp != nullptr, TEXT("UAssetThumbnailWidget::AssetToShow not found. Class may have changed."));

    if (AssetProp)
    {
        const FAssetData* CurrentAsset = AssetProp->ContainerPtrToValuePtr<FAssetData>(ThumbnailWidget);

        if (CurrentAsset)
        {
            bool bAssetChanged = *CurrentAsset != NewAssetData;
            return bAssetChanged;
        }
    }
    
    return true;
}