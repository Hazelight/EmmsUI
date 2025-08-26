#include "AngelscriptManager.h"
#include "AngelscriptBinds.h"
#include "AngelscriptBindDatabase.h"
#include "AngelscriptDocs.h"
#include "EmmsAttribute.h"
#include "Components/PanelWidget.h"
#include "EmmsStatics.h"
#include "UObject/UObjectIterator.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptfunction.h"
#include "as_objecttype.h"
#include "as_generic.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

FEmmsAttributeSpecification* GetOrCreateAttributeSpecification(FProperty* Property, FAngelscriptTypeUsage PropUsage)
{
	FEmmsAttributeSpecification* Spec = FEmmsAttributeSpecification::AttributeSpecsByProperty.FindRef(Property);
	if (Spec == nullptr)
	{
		Spec = new FEmmsAttributeSpecification;
		Spec->Type = EEmmsAttributeType::Property;
		Spec->AttributeProperty = Property;
		Spec->ScriptUsage = PropUsage;

		if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
		{
			if (!BoolProp->IsNativeBool())
				Spec->Type = EEmmsAttributeType::BitField;
		}

		// When a read-only property is changed, the slate widget needs to be rebuilt,
		// since the UMG widget doesn't expect this to change at runtime.
		if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly) || !Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
			Spec->bRequiresWidgetRebuild = true;

		TArray<const FStructProperty*> EncounteredStructProps;
		Spec->bHasObjectReferences = Property->ContainsObjectReference(EncounteredStructProps);

		FEmmsAttributeSpecification::AttributeSpecsByProperty.Add(Property, Spec);
	}

	return Spec;
}

FString GetPropertyCanonicalName(FProperty* Property)
{
	FString PropertyName = FAngelscriptBindDatabase::GetNamingData(Property->GetPathName(), Property->GetName());
	if (CastField<FBoolProperty>(Property) != nullptr && PropertyName[0] == 'b')
		PropertyName = PropertyName.Mid(1);
	return PropertyName;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_EmmsDeclarations((int32)FAngelscriptBinds::EOrder::Late - 5, []
{
	FBindFlags WidgetHandleFlags;
	WidgetHandleFlags.bTemplate = true;
	WidgetHandleFlags.TemplateType = "<T>";
	WidgetHandleFlags.ExtraFlags |= asOBJ_TEMPLATE_SUBTYPE_COVARIANT;
	WidgetHandleFlags.ExtraFlags |= asOBJ_TEMPLATE_INHERIT_SPECIALIZATIONS;
	WidgetHandleFlags.Alignment = 8;

	auto mm_ = FAngelscriptBinds::ValueClass("mm<class T>", sizeof(FEmmsWidgetHandle), WidgetHandleFlags);
	mm_.Constructor("void f()", &UEmmsStatics::ConstructHandle);
	mm_.Constructor("void f(const mm<T>& Other)", &UEmmsStatics::CopyConstructHandle);
	mm_.Method("mm<T>& opAssign(const mm<T>& Other)", &UEmmsStatics::AssignHandle);
});

static const FName NAME_Attribute_Tooltip("ToolTip");
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_EmmsWidgets((int32)FAngelscriptBinds::EOrder::Late + 200, []
{
	auto mm_ = FAngelscriptBinds::ExistingClass("mm<T>");

	mm_.Method("T handle_only GetUnderlyingWidget() const", &UEmmsStatics::GetUnderlyingWidget);
	FAngelscriptBinds::SetPreviousBindIsPropertyAccessor(false);
	SCRIPT_BIND_DOCUMENTATION("Get the underlying UMG widget being used. Changes made to the widget this way need to be managed manually, since they will not be reverted across frames");

	auto mmUWidget_ = FAngelscriptBinds::ExistingClass("mm<UWidget>");
	mmUWidget_.Method("FEmmsSlotHandle GetSlot() const property", &UEmmsStatics::GetSlot);
	SCRIPT_BIND_DOCUMENTATION("Get the slot this widget is currently contained within");

	auto FEmmsSlotHandle_ = FAngelscriptBinds::ExistingClass("FEmmsSlotHandle");

	TMap<TPair<FName, FString>, FEmmsAttributeSpecification*> BoundImplicitSlotAttributes;
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Deprecated))
			continue;
		if (!Class->HasAllClassFlags(CLASS_Native))
			continue;
		if (!FAngelscriptType::GetByClass(Class).IsValid())
			continue;

		if (Class->IsChildOf(UWidget::StaticClass()))
		{
			FString TemplateType = FString::Printf(
				TEXT("mm<%s>"), *FAngelscriptBindDatabase::GetClassBoundName(Class)
			);

			FAngelscriptBinds Widget_ = FAngelscriptBinds::ExistingClass(TemplateType);

			// Any properties that are blueprint writable should be exposed as attributes
			TSet<FName> BoundAttributes;
			for (TFieldIterator<FProperty> It(Class, EFieldIterationFlags::None); It; ++It)
			{
				FProperty* Property = *It;
				if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintAssignable | CPF_Edit))
					continue;
				if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly) && !Property->HasAnyPropertyFlags(CPF_Edit))
					continue;
				if (BoundAttributes.Contains(Property->GetFName()))
					continue;
				if (Property->ArrayDim != 1)
					continue;

				FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
				if (!PropUsage.IsValid())
					continue;

				FMulticastDelegateProperty* EventProperty = CastField<FMulticastDelegateProperty>(Property);
				FDelegateProperty* DelegateProperty = CastField<FDelegateProperty>(Property);
				if (EventProperty != nullptr || DelegateProperty != nullptr)
				{
					FString CleanedEventName = GetPropertyCanonicalName(Property);
					CleanedEventName.RemoveFromStart(TEXT("BP_"));
					CleanedEventName.RemoveFromStart(TEXT("On"));

					// Simple check whether the event was triggered this frame
					Widget_.Method(
						FString::Printf(
							TEXT("bool Was%s() const no_discard"),
							*CleanedEventName
						),
						&UEmmsStatics::WasEventTriggered,
						Property
					);
					FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
					const FString& Tooltip = Property->GetMetaData(NAME_Attribute_Tooltip);
					if (Tooltip.Len() != 0)
						FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);
#endif

					UFunction* SignatureFunction = nullptr;
					if (EventProperty != nullptr)
						SignatureFunction = EventProperty->SignatureFunction;
					else
						SignatureFunction = DelegateProperty->SignatureFunction;

					// Bind another WasEvent() check that outputs the event's parameters
					if (SignatureFunction->ParmsSize != 0)
					{
						FString ParameterList;
						bool bAllParametersValid = true;

						for (TFieldIterator<FProperty> ArgIt(SignatureFunction); ArgIt; ++ArgIt)
						{
							if (!ArgIt->HasAnyPropertyFlags(CPF_Parm))
								continue;
							if (ArgIt->HasAnyPropertyFlags(CPF_ReturnParm))
								continue;

							FAngelscriptTypeUsage ArgUsage = FAngelscriptTypeUsage::FromProperty(*ArgIt);
							if (!ArgUsage.IsValid())
							{
								bAllParametersValid = false;
								break;
							}

							ArgUsage.bIsReference = false;
							ArgUsage.bIsConst = false;

							if (!ParameterList.IsEmpty())
								ParameterList += ", ";
							ParameterList += FString::Printf(
								TEXT("%s&out %s"),
								*ArgUsage.GetAngelscriptDeclaration(
									FAngelscriptType::EAngelscriptDeclarationMode::PreResolvedObject
								),
								*FAngelscriptBindDatabase::GetNamingData(ArgIt->GetPathName(), ArgIt->GetName())
							);
						}

						if (bAllParametersValid && !ParameterList.IsEmpty())
						{
							Widget_.GenericMethod(
								FString::Printf(
									TEXT("bool Was%s(%s) const no_discard"),
									*CleanedEventName,
									*ParameterList
								),
								&UEmmsStatics::WasEventTriggered_Params,
								Property
							);

#if WITH_EDITOR
							if (Tooltip.Len() != 0)
								FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);
#endif
						}
					}

					// Also allow binding events to a function directly
					Widget_.Method(
						FString::Printf(
							TEXT("void On%s(const UObject Object, const FName& FunctionName) const"),
							*CleanedEventName
						),
						&UEmmsStatics::BindToEvent,
						Property
					);
					FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
					if (Tooltip.Len() != 0)
						FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);
#endif

					FAngelscriptDocs::AddScriptFunctionMeta(
						FAngelscriptBinds::GetPreviousFunctionId(),
						TEXT("DelegateBindType"),
						PropUsage.Type->GetAngelscriptTypeName(PropUsage)
					);
					SCRIPT_MANUAL_BIND_META("DelegateObjectParam", "Object");
					SCRIPT_MANUAL_BIND_META("DelegateFunctionParam", "FunctionName");
				}
				else
				{
					FString PropertyName = GetPropertyCanonicalName(Property);
					if (PropertyName == TEXT("Slot"))
						continue;

					FEmmsAttributeSpecification* Spec = GetOrCreateAttributeSpecification(
						Property,
						PropUsage
					);

					Widget_.Method(
						FString::Printf(
							TEXT("void Set%s(const %s&in if_handle_then_const Value) const"),
							*PropertyName,
							*PropUsage.GetAngelscriptDeclaration(
								FAngelscriptType::EAngelscriptDeclarationMode::PreResolvedObject
							)
						),
						&UEmmsStatics::SetAttributeValue,
						Spec
					);
					FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
					const FString& Tooltip = Property->GetMetaData(NAME_Attribute_Tooltip);
					if (Tooltip.Len() != 0)
						FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);
#endif
					
					Widget_.Method(
						FString::Printf(
							TEXT("const %s& Get%s() const"),
							*PropUsage.GetAngelscriptDeclaration(
								FAngelscriptType::EAngelscriptDeclarationMode::PreResolvedObject
							),
							*PropertyName
						),
						&UEmmsStatics::GetAttributeValue,
						Spec
					);
					FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
					if (Tooltip.Len() != 0)
						FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);
#endif
				}
			}
		}
		else if (Class->IsChildOf(UPanelSlot::StaticClass()))
		{
			// Any properties that are blueprint writable should be exposed as attributes
			for (TFieldIterator<FProperty> It(Class, EFieldIterationFlags::None); It; ++It)
			{
				FProperty* Property = *It;
				if (!Property->HasAnyPropertyFlags(CPF_BlueprintVisible))
					continue;
				if (Property->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
					continue;
				if (Property->ArrayDim != 1)
					continue;

				FAngelscriptTypeUsage PropUsage = FAngelscriptTypeUsage::FromProperty(Property);
				if (!PropUsage.IsValid())
					continue;

				FEmmsAttributeSpecification* Spec = GetOrCreateAttributeSpecification(
					Property,
					PropUsage
				);

				FEmmsAttributeSpecification::SlotAttributeSpecs.Add(
					TPair<FName,UClass*>(Property->GetFName(), Class),
					Spec
				);

				FString PropertyName = GetPropertyCanonicalName(Property);
				FString ScriptTypeDecl = PropUsage.GetAngelscriptDeclaration(
					FAngelscriptType::EAngelscriptDeclarationMode::PreResolvedObject
				);

#if WITH_EDITOR
				const FString& Tooltip = Property->GetMetaData(NAME_Attribute_Tooltip);
#endif

				TPair<FName, FString> ImplicitAttributePair = TPair<FName, FString>(
					Property->GetFName(), ScriptTypeDecl
				);
				if (FEmmsAttributeSpecification* ExistingSpec = BoundImplicitSlotAttributes.FindRef(ImplicitAttributePair))
				{
					FEmmsAttributeSpecification::SlotAttributeGenericSpecs.Add(Spec, ExistingSpec);
				}
				else
				{
					// Allow setting it explicit on the slot handle as well
					FEmmsSlotHandle_.Method(
						FString::Printf(
							TEXT("void Set%s(const %s&in if_handle_then_const Value)"),
							*PropertyName,
							*ScriptTypeDecl
						),
						&UEmmsStatics::SetSlotAttributeValue,
						Spec
					);

#if WITH_EDITOR
					if (Tooltip.Len() != 0)
						FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(), Tooltip, TEXT(""), nullptr);
#endif

					// Slot attributes should have implicit hierarchy helper binds that set the correct attribute
					{
						FAngelscriptBinds::FNamespace ns("mm");
						FAngelscriptBinds::BindGlobalFunction(
							FString::Printf(
								TEXT("void %s(const %s&in if_handle_then_const Value)"),
								*PropertyName,
								*ScriptTypeDecl
							),
							&UEmmsStatics::SetImplicitPendingSlotAttribute,
							Spec
						);
						FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();

#if WITH_EDITOR
						if (Tooltip.Len() != 0)
						{
							FAngelscriptDocs::AddUnrealDocumentation(FAngelscriptBinds::GetPreviousFunctionId(),
								FString::Printf(TEXT("The next widget drawn will have the following setting applied to its slot:\n%s"), *Tooltip), TEXT(""), nullptr);
						}
#endif
					}

					BoundImplicitSlotAttributes.Add(ImplicitAttributePair, Spec);
					FEmmsAttributeSpecification::SlotAttributeGenericSpecs.Add(Spec, Spec);
				}
			}
		}
	}

	{
		FAngelscriptBinds::FNamespace ns("mm");
		FAngelscriptBinds::BindGlobalFunction("void BeginDraw(UMMWidget Widget)", &UEmmsStatics::BeginDraw_DefaultRoot);
		SCRIPT_BIND_DOCUMENTATION("Begin drawing to an MMWidget. Immediate mode widgets can only be added while drawing to an MMWidget. Must be matched by an EndDraw() call after you are done");

		FAngelscriptBinds::BindGlobalFunction("mm<UPanelWidget> BeginDraw(UMMWidget Widget, const TSubclassOf<UPanelWidget>& RootPanel)", &UEmmsStatics::BeginDraw);
		SCRIPT_BIND_DOCUMENTATION("Begin drawing to an MMWidget. Override the type of panel that is used as the root of the MMWidget");

		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(1);
		FAngelscriptBinds::BindGlobalFunction("void EndDraw()", &UEmmsStatics::EndDraw);
		SCRIPT_BIND_DOCUMENTATION("Finish drawing immediate widgets to an MMWidget");

		{
			FAngelscriptBinds::BindGlobalFunction("mm<UPanelWidget> BeginDrawViewportOverlay(const UObject WorldContext, const FName& OverlayId, int ZOrder = 0, const TSubclassOf<UPanelWidget>& RootPanel = UVerticalBox)", &UEmmsStatics::BeginDrawViewportOverlay);
			FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(3);
			SCRIPT_BIND_DOCUMENTATION("Begin drawing immediate widgets to the game or editor viewport. Must be matched by an EndDraw() call after you are done");

			auto* ScriptFunction = (asCScriptFunction*)FAngelscriptBinds::GetPreviousBind();
			ScriptFunction->hiddenArgumentIndex = 0;
			ScriptFunction->hiddenArgumentDefault = "__WorldContext";
		}

		FAngelscriptBinds::BindGlobalFunction("mm<UPanelWidget> Begin(const TSubclassOf<UPanelWidget>& PanelType)", &UEmmsStatics::BeginWidget);
		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);
		SCRIPT_BIND_DOCUMENTATION("Begin an immediate panel of the specified type. Must be matched by an End() call when done drawing to that panel");

		FAngelscriptBinds::BindGlobalFunction("mm<UPanelWidget> Begin(const mm<UPanelWidget>& ExistingPanel)", &UEmmsStatics::BeginExistingWidget);
		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);
		SCRIPT_BIND_DOCUMENTATION("Draw widgets to an existing panel. Begin()ing the same panel multiple times draws all contained widgets to it in sequence");

		FAngelscriptBinds::BindGlobalFunction("void End()", &UEmmsStatics::EndWidget);
		SCRIPT_BIND_DOCUMENTATION("End the panel widget that is currently active");

		FAngelscriptBinds::BindGlobalFunction("mm<UWidget> Widget(const TSubclassOf<UWidget>& PanelType)", &UEmmsStatics::AddWidget);
		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);
		SCRIPT_BIND_DOCUMENTATION("Draw an immediate widget of the specified type to the active panel");

		FAngelscriptBinds::BindGlobalFunction("mm<UPanelWidget> Within(const TSubclassOf<UPanelWidget>& PanelType)", &UEmmsStatics::WithinWidget);
		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);
		SCRIPT_BIND_DOCUMENTATION("Create an immediate panel widget of the specified type. Only the one single widget that is drawn right after the Within() call will be placed inside the panel");
	}

		// If this is a Panel widget, allow adding children to it
	{
		auto mmUPanelWidget_ = FAngelscriptBinds::ExistingClass("mm<UPanelWidget>");
		mmUPanelWidget_.Method("mm<UWidget> Add(const TSubclassOf<UWidget>& WidgetType) const allow_discard", &UEmmsStatics::AddChildWidget);
		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

		mmUPanelWidget_.Method("mm<UWidget> Add(const mm<UWidget>& Widget) const allow_discard", &UEmmsStatics::AddExistingChildWidget);
		FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);
	}
});

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_EmmsImplicitHeirarchy((int32)FAngelscriptBinds::EOrder::Late + 300, []
{
	FAngelscriptBinds::FNamespace ns("mm");
	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->HasAnyClassFlags(CLASS_NewerVersionExists | CLASS_Deprecated))
			continue;
		if (!Class->HasAllClassFlags(CLASS_Native))
			continue;
		if (!FAngelscriptType::GetByClass(Class).IsValid())
			continue;

		if (!Class->IsChildOf(UPanelWidget::StaticClass()))
			continue;

		FString BoundName = FAngelscriptBindDatabase::GetClassBoundName(Class);
		FString PlainName = BoundName.Mid(1);
		FString TemplateType = FString::Printf(
			TEXT("mm<%s>"), *BoundName
		);

		FString BeginName = FString::Printf(TEXT("Begin%s"), *PlainName);

		if (!ns.HasFunction(BeginName))
		{
			FAngelscriptBinds::BindGlobalFunction(
				FString::Printf(
					TEXT("%s %s()"),
					*TemplateType,
					*BeginName
				),
				&UEmmsStatics::BeginPanelWidget,
				Class
			);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
			FAngelscriptDocs::AddUnrealDocumentation(
				FAngelscriptBinds::GetPreviousFunctionId(),
				FString::Printf(TEXT("Begin an immediate %s panel. All subsequent widgets will be contained within this panel until End%s() is called"),
					*PlainName, *PlainName)
				, TEXT(""), nullptr
			);
		}

		FAngelscriptBinds::BindGlobalFunction(
			FString::Printf(
				TEXT("void End%s()"),
				*PlainName
			),
			&UEmmsStatics::EndPanelWidget,
			Class
		);
		FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		FAngelscriptDocs::AddUnrealDocumentation(
			FAngelscriptBinds::GetPreviousFunctionId(),
			FString::Printf(TEXT("Finish drawing an immediate %s panel. Subsequent widgets will no longer be contained within it"),
				*PlainName)
			, TEXT(""), nullptr
		);

		FString WithinName = FString::Printf(TEXT("Within%s"), *PlainName);
		if (!ns.HasFunction(WithinName))
		{
			FAngelscriptBinds::BindGlobalFunction(
				FString::Printf(
					TEXT("%s %s()"),
					*TemplateType,
					*WithinName
				),
				&UEmmsStatics::WithinPanelWidget,
				Class
			);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
			FAngelscriptDocs::AddUnrealDocumentation(
				FAngelscriptBinds::GetPreviousFunctionId(),
				FString::Printf(TEXT("Begin an immediate %s panel. Only the one single widget that is drawn right after the Within%s() call will be placed inside the panel"),
					*PlainName, *PlainName)
				, TEXT(""), nullptr
			);
		}
	}
});
