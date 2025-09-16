#pragma once
#include "EmmsWidgetElement.h"
#include "Styling/SlateWidgetStyleContainerBase.h"
#include "Styling/SlateTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "IDetailCustomization.h"
#include "EmmsAttribute.h"
#include "EmmsStatics.h"
#include "EmmsEditorWidgetHelpers.generated.h"

class UAssetThumbnailWidget;
struct FAssetData;

UCLASS()
class EMMSUIEDITOR_API UEmmsEditorWidgetHelpers : public UObject
{
	GENERATED_BODY()

public:

	static void SetDetailsViewObject(FEmmsWidgetHandle* Widget, UObject* Object);
	static void SetDetailsViewStruct_NoTitle(FEmmsWidgetHandle* Widget, void* DataPtr, int TypeId);
	static void SetDetailsViewStruct(FEmmsWidgetHandle* Widget, void* DataPtr, int TypeId, const FString& HeaderTitle);

	static FEmmsWidgetHandle AssetThumbnailFromObject(UObject* Object, int32 Resolution);
	static FEmmsWidgetHandle AssetThumbnailFromAssetData(const FAssetData& AssetData, int32 Resolution);

private:
    static bool IsAssetThumbnailWidgetChanged(UAssetThumbnailWidget* ThumbnailWidget, const FAssetData& NewAssetData, int32 NewResolution);
};

UCLASS(AutoExpandCategories="Struct")
class UEmmsEditableInstancedStruct : public UObject
{
	GENERATED_BODY()

public:

	FInstancedStruct PrevInputValue;

	UPROPERTY()
	FString HeaderTitle;

	UPROPERTY(EditAnywhere, Category = "Struct", Meta = (StructTypeConst))
	FInstancedStruct Struct;
};

class FEmmsEditableInstancedStructDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};