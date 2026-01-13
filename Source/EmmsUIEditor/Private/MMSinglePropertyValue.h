#pragma once
#include "Components/SinglePropertyView.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "MMSinglePropertyValue.generated.h"

UCLASS(MinimalAPI)
class UMMSinglePropertyValue : public UPropertyViewBase
{
	GENERATED_BODY()

public:

	/** The name of the property to display. */
	UPROPERTY(EditAnywhere, Category = "View")
	FName PropertyName;

	UPROPERTY(EditAnywhere, Category = "View")
	bool bHideAssetThumbnail = false;
	UPROPERTY(EditAnywhere, Category = "View")
	bool bShowResetToDefault = false;

	TSharedPtr<ISinglePropertyView> SinglePropertyViewWidget;

	virtual void BuildContentWidget() override
	{
		SinglePropertyViewWidget.Reset();

		if (!GetDisplayWidget().IsValid())
		{
			return;
		}

		bool bCreateMissingWidget = true;
		FText MissingWidgetText = FText::FromString("Editor Only");

		if (GIsEditor)
		{
			UObject* ViewedObject = GetObject();
			if (ViewedObject == nullptr)
			{
				bool bIsObjectNull = Object.IsNull();
				if (bIsObjectNull)
				{
					MissingWidgetText = FText::FromString("Undefined Object");
				}
				else
				{
					MissingWidgetText = FText::FromString("UnloadedObject");
				}
			}
			else if (PropertyName == NAME_None)
			{
				MissingWidgetText = FText::FromString("Undefined Property");
			}
			else
			{
				FProperty* Property = ViewedObject->GetClass()->FindPropertyByName(PropertyName);
				if (Property == nullptr)
				{
					MissingWidgetText = FText::FromString("Unknown Property");
				}
				else if (!Property->HasAllPropertyFlags(CPF_Edit))
				{
					MissingWidgetText = FText::FromString("Invalid Property");
				}
				else if (CastField<FArrayProperty>(Property) || CastField<FMapProperty>(Property) || CastField<FSetProperty>(Property))
				{
					MissingWidgetText = FText::FromString("Unsupported Property");
				}
				else
				{
					FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
					FSinglePropertyParams SinglePropertyArgs;
					SinglePropertyArgs.NamePlacement = EPropertyNamePlacement::Hidden;
					SinglePropertyArgs.bHideAssetThumbnail = bHideAssetThumbnail;
					SinglePropertyArgs.bHideResetToDefault = !bShowResetToDefault;
					SinglePropertyViewWidget = PropertyEditorModule.CreateSingleProperty(ViewedObject, PropertyName, SinglePropertyArgs);

					if (SinglePropertyViewWidget.IsValid())
					{
						if (const TSharedPtr<IPropertyHandle> Handle = SinglePropertyViewWidget->GetPropertyHandle())
						{
							FSimpleDelegate PropertyChanged = FSimpleDelegate::CreateUObject(this, &UMMSinglePropertyValue::InternalSinglePropertyChanged);
							Handle->SetOnPropertyValueChanged(PropertyChanged);
							Handle->SetOnChildPropertyValueChanged(PropertyChanged);
						}
						GetDisplayWidget()->SetContent(SinglePropertyViewWidget.ToSharedRef());
						bCreateMissingWidget = false;
					}
					else
					{
						// Some built-in structs like FColor are supported, others aren't
						if (CastField<FStructProperty>(Property))
						{
							MissingWidgetText = FText::FromString("Unsupported Property");
						}
						else
						{
							MissingWidgetText = FText::FromString("Unknown Error");
						}
					}
				}
			}
		}

		if (bCreateMissingWidget)
		{
			GetDisplayWidget()->SetContent(
				SNew(STextBlock)
				.Text(MissingWidgetText)
			);
		}
	}

	void InternalSinglePropertyChanged()
	{
		OnPropertyChangedBroadcast(PropertyName);
	}

	virtual void OnObjectChanged() override
	{
		BuildContentWidget();
	}

	virtual void ReleaseSlateResources(bool bReleaseChildren) override
	{
		Super::ReleaseSlateResources(bReleaseChildren);
		SinglePropertyViewWidget.Reset();
	}
};
