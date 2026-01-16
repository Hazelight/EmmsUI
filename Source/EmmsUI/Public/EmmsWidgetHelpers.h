#pragma once
#include "EmmsWidgetElement.h"
#include "Styling/SlateTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "EmmsAttribute.h"
#include "EmmsStatics.h"
#include "EmmsWidgetHelpers.generated.h"

UCLASS()
class EMMSUI_API UEmmsWidgetHelpers : public UObject
{
	GENERATED_BODY()

public:
	static FEmmsAttributeSpecification* Attr_UWidget_RenderTransform;
	static FEmmsAttributeSpecification* Attr_UWidget_ToolTipText;
	static FEmmsAttributeSpecification* Attr_UTextBlock_Text;
	static FEmmsAttributeSpecification* Attr_UTextBlock_Font;
	static FEmmsAttributeSpecification* Attr_UTextBlock_ColorAndOpacity;
	static FEmmsAttributeSpecification* Attr_UTextBlock_AutoWrapText;
	static FMulticastDelegateProperty* Event_UButton_OnClicked;
	static FEmmsAttributeSpecification* Attr_UButton_WidgetStyle;
	static FEmmsAttributeSpecification* Attr_USpacer_Size;
	static FEmmsAttributeSpecification* Attr_USizeBox_WidthOverride;
	static FEmmsAttributeSpecification* Attr_USizeBox_HeightOverride;
	static FEmmsAttributeSpecification* Attr_USizeBox_MinDesiredWidth;
	static FEmmsAttributeSpecification* Attr_USizeBox_MaxDesiredWidth;
	static FEmmsAttributeSpecification* Attr_USizeBox_MinDesiredHeight;
	static FEmmsAttributeSpecification* Attr_USizeBox_MaxDesiredHeight;
	static FEmmsAttributeSpecification* Attr_USizeBox_MinAspectRatio;
	static FEmmsAttributeSpecification* Attr_USizeBox_MaxAspectRatio;
	static FEmmsAttributeSpecification* Attr_UEditableTextBox_Text;
	static FEmmsAttributeSpecification* Attr_UEditableText_Text;
	static FEmmsAttributeSpecification* Attr_UEditableText_HintText;
	static FEmmsAttributeSpecification* Attr_UEditableText_WidgetStyle;
	static FEmmsAttributeSpecification* Attr_UBorder_Background;
	static FDelegateProperty* Event_UBorder_OnMouseButtonDownEvent;
	static FDelegateProperty* Event_UBorder_OnMouseDoubleClickEvent;
	static FEmmsAttributeSpecification* Attr_UImage_Brush;
	static FEmmsAttributeSpecification* Attr_UImage_ColorAndOpacity;
	static FEmmsAttributeSpecification* Attr_UListView_EntryWidgetClass;
	static FEmmsAttributeSpecification* Attr_USpinBox_Value;
	static FEmmsAttributeSpecification* Attr_USpinBox_MinValue;
	static FEmmsAttributeSpecification* Attr_USpinBox_MaxValue;
	static FEmmsAttributeSpecification* Attr_USpinBox_MinSliderValue;
	static FEmmsAttributeSpecification* Attr_USpinBox_MaxSliderValue;
	static FEmmsAttributeSpecification* Attr_USpinBox_Delta;
	static FEmmsAttributeSpecification* Attr_USlider_Value;
	static FEmmsAttributeSpecification* Attr_USlider_MinValue;
	static FEmmsAttributeSpecification* Attr_USlider_MaxValue;
	static FEmmsAttributeSpecification* Attr_UCheckBox_CheckedState;

	template<typename T>
	static T* GetPartialPendingAttribute(FEmmsWidgetHandle Widget, FEmmsAttributeSpecification* Spec)
	{
		if (FEmmsAttributeValue* AttrValue = UEmmsStatics::GetPartialPendingAttribute(Widget, Spec))
			return (T*)AttrValue->GetDataPtr();
		else
			return nullptr;
	}

	template<typename T>
	static T* GetPartialPendingSlotAttribute(FEmmsWidgetHandle Widget, FEmmsAttributeSpecification* Spec)
	{
		if (FEmmsAttributeValue* AttrValue = UEmmsStatics::GetPartialPendingSlotAttribute(Widget, Spec))
			return (T*)AttrValue->GetDataPtr();
		else
			return nullptr;
	}

	static FEmmsWidgetHandle Button(const FString& LabelText);
	static FEmmsWidgetHandle Button_IconBrush(const FString& LabelText, const FSlateBrush& IconBrush, const FVector2D& IconSize, const FLinearColor& IconColor);
	static FEmmsWidgetHandle Button_IconStyleBrush(const FString& LabelText, const FName& IconStyleBrush, const FVector2D& IconSize, const FLinearColor& IconColor);
	static void SetButtonStyleColor(FEmmsWidgetHandle* Handle, const FLinearColor& StyleColor);
	static bool Button_ImplBoolConv(FEmmsWidgetHandle* Widget);
	static void SetButtonInnerPadding(FEmmsWidgetHandle* Widget, float Horizontal, float Vertical);

	static FEmmsWidgetHandle Text(const FString& Text, float FontSize = 0, const FLinearColor& Color = FLinearColor::White, bool bWrap = false, bool bBold = false);
	static void SetTextFontSize(FEmmsWidgetHandle* Widget, float FontSize);
	static void SetTextFontFace(FEmmsWidgetHandle* Widget, FName FontFace);
	static void SetTextBold(FEmmsWidgetHandle* Widget, bool bBold);

	static FEmmsWidgetHandle Spacer_Uniform(float Size);
	static FEmmsWidgetHandle Spacer(float Width, float Height);

	static FEmmsWidgetHandle BeginSizeBox(float Width = 0, float Height = 0, float MinWidth = 0, float MaxWidth = 0, float MinHeight = 0, float MaxHeight = 0);
	static FEmmsWidgetHandle WithinSizeBox(float Width = 0, float Height = 0, float MinWidth = 0, float MaxWidth = 0, float MinHeight = 0, float MaxHeight = 0);

	static void SetRenderScale_Uniform(FEmmsWidgetHandle* Widget, float UniformScale);
	static void SetRenderScale_Vector(FEmmsWidgetHandle* Widget, const FVector2D& Scale);
	static void SetRenderScale(FEmmsWidgetHandle* Widget, float HorizontalScale, float VerticalScale);
	static void SetRenderTranslation_Vector(FEmmsWidgetHandle* Widget, const FVector2D& Translation);
	static void SetRenderTranslation(FEmmsWidgetHandle* Widget, float XTranslation, float YTranslation);
	static void SetRenderTransformAngle(FEmmsWidgetHandle* Widget, float Angle);
	static bool IsWidgetValid(FEmmsWidgetHandle* Widget);
	static bool IsHovered(FEmmsWidgetHandle* Widget);
	static bool HasAnyUserFocus(FEmmsWidgetHandle* Widget);
	static const FGeometry& GetCachedGeometry(FEmmsWidgetHandle* Widget);

	static void SetToolTipText(FEmmsWidgetHandle* Widget, const FString& Text);

	static FEmmsWidgetHandle EditableTextBox(FString& Value);
	static FEmmsWidgetHandle EditableText(FString& Value, const FString& HintText, float FontSize = 0, const FLinearColor& Color = FLinearColor::White, bool bBold = false);
	static FEmmsWidgetHandle SpinBox(double& Value);
	static FEmmsWidgetHandle SpinBox_Constrained(double& Value, float MinValue, float MaxValue, float Delta = 0);
	static FEmmsWidgetHandle Slider(double& Value, float MinValue = 0, float MaxValue = 1);

	static FEmmsWidgetHandle CheckBox(bool& Value);
	static FEmmsWidgetHandle CheckBox_Label(bool& Value, const FString& Label);

	static FEmmsWidgetHandle ListView(const TArray<UObject*>& ListItems);
	static FEmmsWidgetHandle ListView_Indexed(int ItemCount);
	static FEmmsWidgetHandle ListView_Widgets(const TArray<UObject*>& ListItems, const TSubclassOf<UUserWidget>& ItemWidgetClass);
	static void SetListItems(FEmmsWidgetHandle* ListView, const TArray<UObject*>& ListItems);
	static UObject* GetSelectedItem(FEmmsWidgetHandle* ListView, const TSubclassOf<UObject>& ItemType);
	static int GetSelectedIndex(FEmmsWidgetHandle* ListView);
	static void SetListViewDefaultSelectedIndex(FEmmsWidgetHandle* ListView, int Index);
	
	static FEmmsWidgetHandle BeginBorder();
	static FEmmsWidgetHandle BeginBorder_Color(const FLinearColor& Color);
	static FEmmsWidgetHandle BeginBorder_RoundedColor(const FLinearColor& Color, float RoundedCornerRadius, const FLinearColor& OutlineColor, float OutlineWidth);
	static FEmmsWidgetHandle BeginBorder_Texture(UTexture2D* Texture);
	static FEmmsWidgetHandle BeginBorder_Material(UMaterialInterface* Texture);
	static FEmmsWidgetHandle BeginBorder_Brush(const FSlateBrush& Brush);
	static FEmmsWidgetHandle BeginBorder_StyleBrush(const FName& BrushName);
	static UMaterialInstanceDynamic* GetBorderDynamicMaterial(FEmmsWidgetHandle* Widget);

	static bool WasBorderClicked(FEmmsWidgetHandle* Widget);
	static bool WasBorderMiddleClicked(FEmmsWidgetHandle* Widget);
	static bool WasBorderRightClicked(FEmmsWidgetHandle* Widget);
	static bool WasBorderDoubleClicked(FEmmsWidgetHandle* Widget);

	static FEmmsWidgetHandle WithinBorder();
	static FEmmsWidgetHandle WithinBorder_Color(const FLinearColor& Color);
	static FEmmsWidgetHandle WithinBorder_RoundedColor(const FLinearColor& Color, float RoundedCornerRadius, const FLinearColor& OutlineColor, float OutlineWidth);
	static FEmmsWidgetHandle WithinBorder_Texture(UTexture2D* Texture);
	static FEmmsWidgetHandle WithinBorder_Material(UMaterialInterface* Texture);
	static FEmmsWidgetHandle WithinBorder_Brush(const FSlateBrush& Brush);
	static FEmmsWidgetHandle WithinBorder_StyleBrush(const FName& BrushName);

	static FEmmsWidgetHandle Image_Texture(UTexture2D* Texture, float Width = 0, float Height = 0, const FLinearColor& Color = FLinearColor::White);
	static FEmmsWidgetHandle Image_Material(UMaterialInterface* Texture, float Width, float Height, const FLinearColor& Color = FLinearColor::White);
	static FEmmsWidgetHandle Image_Brush(const FSlateBrush& Brush, float Width = 0, float Height = 0, const FLinearColor& Color = FLinearColor::White);
	static FEmmsWidgetHandle Image_StyleBrush(const FName& BrushName, float Width = 0, float Height = 0, const FLinearColor& Color = FLinearColor::White);
	static UMaterialInstanceDynamic* GetImageDynamicMaterial(FEmmsWidgetHandle* Widget);

	static FEmmsWidgetHandle ComboBox_Enum(void* DataPtr, int TypeId);
	static FEmmsWidgetHandle ComboBox_Names(const TArray<FName>& Items, FName& OutSelectedItem);
	static FEmmsWidgetHandle ComboBox_Names_NoOut(const TArray<FName>& Items);
	static FEmmsWidgetHandle ComboBox_Strings(const TArray<FString>& Items, FString& OutSelectedItem);
	static FEmmsWidgetHandle ComboBox_Strings_NoOut(const TArray<FString>& Items);
	static int GetComboBoxSelectedIndex(FEmmsWidgetHandle* Widget);
};