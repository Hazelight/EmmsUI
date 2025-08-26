#include "EmmsStatics.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "EmmsEventListener.h"
#include "EmmsUISubsystem.h"
#include "MMPopupWindow.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

#include "StartAngelscriptHeaders.h"
#include "as_context.h"
#include "as_scriptfunction.h"
#include "as_objecttype.h"
#include "as_generic.h"
#include "as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

#if WITH_EDITOR
#include "SLevelViewport.h"
#include "Editor.h"
#include "IAssetViewport.h"
#endif

TArray<UEmmsStatics::FImplicitHierarchy> UEmmsStatics::ImplicitHierarchy;

UWorld* UEmmsStatics::GetWorldForUI(const UObject* WorldContext)
{
	UWorld* World = nullptr;
	if (WorldContext != nullptr)
	{
		World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	}

	if (World == nullptr)
	{
#if WITH_EDITOR
		// If there is a play world, use that. Otherwise use the editor world
		World = GEditor->GetCurrentPlayWorld(nullptr);
		if (World == nullptr)
			World = GEditor->GetEditorWorldContext().World();
#else
		// Cooked games always use GWorld
		World = GWorld;
#endif
	}
	return World;
}

void UEmmsStatics::ConstructHandle(FEmmsWidgetHandle* Handle)
{
	new(Handle) FEmmsWidgetHandle{};
}

void UEmmsStatics::CopyConstructHandle(FEmmsWidgetHandle* Handle, FEmmsWidgetHandle* OtherHandle)
{
	new(Handle) FEmmsWidgetHandle{OtherHandle->WidgetTree, OtherHandle->Element};
}

FEmmsWidgetHandle* UEmmsStatics::AssignHandle(FEmmsWidgetHandle* Handle, FEmmsWidgetHandle* OtherHandle)
{
	*Handle = *OtherHandle;
	return Handle;
}

UWidget* UEmmsStatics::GetUnderlyingWidget(FEmmsWidgetHandle* Handle)
{
	if (Handle->Element == nullptr)
		return nullptr;
	return Handle->Element->UMGWidget;
}

UPanelSlot* UEmmsStatics::GetUnderlyingSlot(FEmmsWidgetHandle* Handle)
{
	if (Handle->Element == nullptr)
		return nullptr;
	return Handle->Element->UMGWidget->Slot;
}

FEmmsSlotHandle UEmmsStatics::GetSlot(FEmmsWidgetHandle* Handle)
{
	return FEmmsSlotHandle{Handle->WidgetTree, Handle->Element};
}

FEmmsWidgetHandle UEmmsStatics::AddChildWidget(FEmmsWidgetHandle* Parent, const TSubclassOf<UWidget>& WidgetType)
{
	if (Parent->WidgetTree == nullptr)
		return FEmmsWidgetHandle{};
	return Parent->WidgetTree->GetOrCreateChildWidget(
		WidgetType,
		*Parent
	);
}

FEmmsWidgetHandle UEmmsStatics::AddExistingChildWidget(FEmmsWidgetHandle* Parent, FEmmsWidgetHandle* Child)
{
	if (Child->Element == nullptr)
		return FEmmsWidgetHandle{};
	if (Parent->Element == nullptr)
		return FEmmsWidgetHandle{};

	if (Child->WidgetTree != Parent->WidgetTree)
	{
		FAngelscriptManager::Throw("Widgets are from different root widget trees");
		return FEmmsWidgetHandle{};
	}

	if (Child->Element->Parent != nullptr)
		Child->Element->Parent->PendingChildren.Remove(Child->Element);
	Child->Element->Parent = Parent->Element;
	Parent->Element->PendingChildren.Add(Child->Element);
	return *Child;
}

void UEmmsStatics::SetAttributeValue(FEmmsWidgetHandle* Widget, class asCScriptFunction* ScriptFunction, void* Value)
{
	if (Widget->Element == nullptr)
		return;
	auto* Spec = (FEmmsAttributeSpecification*)ScriptFunction->userData;
	Widget->Element->Attributes.FindOrAdd(Spec).SetPendingValue(Spec, Value);
}

void* UEmmsStatics::GetAttributeValue(FEmmsWidgetHandle* Widget, class asCScriptFunction* ScriptFunction)
{
	if (Widget->Element == nullptr)
		return nullptr;

	auto* Spec = (FEmmsAttributeSpecification*)ScriptFunction->userData;
	auto& AttributeState = Widget->Element->Attributes.FindOrAdd(Spec);
	AttributeState.UpdateMirroredValue(Spec, Widget->Element->UMGWidget);
	return AttributeState.MirroredValue.GetDataPtr();
}

FEmmsAttributeValue* UEmmsStatics::GetPartialPendingAttribute(FEmmsWidgetHandle Widget, FEmmsAttributeSpecification* Spec)
{
	if (Widget.Element == nullptr)
		return nullptr;

	auto& AttributeState = Widget.Element->Attributes.FindOrAdd(Spec);
	Spec->InitializeValue(AttributeState.PendingValue);
	return &AttributeState.PendingValue;
}

FEmmsAttributeValue* UEmmsStatics::GetPartialPendingSlotAttribute(FEmmsWidgetHandle Widget, FEmmsAttributeSpecification* GenericSpec)
{
	if (Widget.Element == nullptr)
		return nullptr;
	if (Widget.Element->UMGWidget->Slot == nullptr)
		return nullptr;

	UPanelSlot* Slot = Widget.Element->UMGWidget->Slot;
	auto* ResolvedSpec = FEmmsAttributeSpecification::SlotAttributeSpecs.FindRef(
		TPair<FName, UClass*>(GenericSpec->GetAttributeName(), Slot->GetClass())
	);

	auto& AttributeState = Widget.Element->SlotAttributes.FindOrAdd(ResolvedSpec);
	ResolvedSpec->InitializeValue(AttributeState.PendingValue);
	return &AttributeState.PendingValue;
}

void UEmmsStatics::SetSlotAttributeValue(FEmmsSlotHandle* Widget, class asCScriptFunction* ScriptFunction, void* Value)
{
	if (Widget->Element == nullptr)
		return;
	if (Widget->Element->UMGWidget->Slot == nullptr)
		return;

	auto* GenericSpec = (FEmmsAttributeSpecification*)ScriptFunction->userData;

	UPanelSlot* Slot = Widget->Element->UMGWidget->Slot;
	auto* ResolvedSpec = FEmmsAttributeSpecification::SlotAttributeSpecs.FindRef(
		TPair<FName, UClass*>(GenericSpec->GetAttributeName(), Slot->GetClass())
	);

	Widget->Element->SlotAttributes.FindOrAdd(ResolvedSpec).SetPendingValue(ResolvedSpec, Value);
}

void UEmmsStatics::SetDefaultChildSlotAttributeValue(FEmmsWidgetHandle Widget, FEmmsAttributeSpecification* Spec, void* Value)
{
	if (Widget.Element == nullptr)
		return;
	Spec->AssignValue(Widget.Element->DefaultChildSlotAttributes.FindOrAdd(Spec), Value);
}

void UEmmsStatics::SetImplicitPendingSlotAttribute(class asCScriptFunction* ScriptFunction, void* ValuePtr)
{
	auto* GenericSpec = (FEmmsAttributeSpecification*)ScriptFunction->userData;
	FEmmsAttributeValue* AttrValue = GetPartialImplicitPendingSlotAttribute(GenericSpec);
	if (AttrValue != nullptr)
		GenericSpec->AssignValue(*AttrValue, ValuePtr);
}

UPanelWidget* UEmmsStatics::FImplicitHierarchy::GetCurrentPanel()
{
	if (ActiveWidgets.Num() == 0)
		return nullptr;
	UPanelWidget* ActivePanel = Cast<UPanelWidget>(ActiveWidgets.Last().Element->UMGWidget);
	return ActivePanel;
}

FEmmsWidgetElement* UEmmsStatics::FImplicitHierarchy::GetCurrentElement()
{
	if (ActiveWidgets.Num() == 0)
		return nullptr;
	return ActiveWidgets.Last().Element;
}

FEmmsAttributeValue* UEmmsStatics::GetPartialImplicitPendingSlotAttribute(FEmmsAttributeSpecification* GenericSpec)
{
	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root is active to set slot properties on");
		return nullptr;
	}

	// We have a spec for _some_ slot type, but we need the spec for the slot that our current
	// widget is actually going to create later!
	UPanelWidget* ActivePanel = ImplicitHierarchy.Last().GetCurrentPanel();
	if (ActivePanel == nullptr)
	{
		FAngelscriptManager::Throw("No panel widget is currently active");
		return nullptr;
	}

	UClass* SlotType = ActivePanel->GetSlotClass();
	auto* ResolvedSpec = FEmmsAttributeSpecification::SlotAttributeSpecs.FindRef(
		TPair<FName, UClass*>(GenericSpec->GetAttributeName(), SlotType)
	);

	if (ResolvedSpec == nullptr)
	{
		FString Error = FString::Printf(
			TEXT("Slot attribute %s does not apply to slots of type %s"),
			*GenericSpec->GetAttributeName().ToString(),
			*GetNameSafe(SlotType)
		);
		FAngelscriptManager::Throw(TCHAR_TO_ANSI(*Error));
		return nullptr;
	}

	// We still add it by the generic spec, not the resolved one, to share memory usages
	FEmmsAttributeValue& AttrValue = ImplicitHierarchy.Last().Root->PendingSlotAttributes.FindOrAdd(GenericSpec);
	if (AttrValue.IsEmpty())
		GenericSpec->InitializeValue(AttrValue);
	return &AttrValue;
}

void UEmmsStatics::BeginDraw_DefaultRoot(UMMWidget* Widget)
{
	BeginDraw(Widget, nullptr);
}

FEmmsWidgetHandle UEmmsStatics::BeginDraw(UMMWidget* Widget, const TSubclassOf<UPanelWidget>& RootPanel)
{
	if (Widget == nullptr)
		return FEmmsWidgetHandle{};
	if (Widget->WidgetTree == nullptr)
		return FEmmsWidgetHandle{};

	// If we have a bunch of draws active 
	if (ImplicitHierarchy.Num() > 1000)
	{
		FAngelscriptManager::Throw("Large amount of MMWidget Draws active, possible missing EndDraw()?");
		ImplicitHierarchy.Reset();
		return FEmmsWidgetHandle{};
	}

	for (int i = 0, Count = ImplicitHierarchy.Num(); i < Count; ++i)
	{
		if (ImplicitHierarchy[i].Root == Widget)
		{
			FAngelscriptManager::Throw("MMWidget has already begun drawing. Possible missing EndDraw()?");
			ImplicitHierarchy.RemoveAt(i);
			return FEmmsWidgetHandle{};
		}
	}

	FImplicitHierarchy& Hier = ImplicitHierarchy.Emplace_GetRef();
	Hier.Root = Widget;
	Hier.Root->OnBeginDraw();

	// Add the root panel
	TSubclassOf<UPanelWidget> RootType = RootPanel;
	if (RootType == nullptr)
		RootType = Widget->DefaultRootPanel;
	if (RootType == nullptr)
		RootType = USizeBox::StaticClass();

	FEmmsWidgetHandle RootWidget = ImplicitHierarchy.Last().Root->GetOrCreateRootWidget(RootType);
	ImplicitHierarchy.Last().ActiveWidgets.Add(FImplicitHierarchyPanel{RootWidget.Element});
	return RootWidget;
}

FEmmsWidgetHandle UEmmsStatics::BeginDrawViewportOverlay(const UObject* WorldContext, const FName& OverlayId, int OverlayZOrder, const TSubclassOf<UPanelWidget>& RootPanel)
{
	UWorld* World = GetWorldForUI(WorldContext);
	if (World == nullptr)
		return FEmmsWidgetHandle{};

	UEmmsUISubsystem* EmmsSubsys = World->GetSubsystem<UEmmsUISubsystem>();
	if (EmmsSubsys == nullptr)
		return FEmmsWidgetHandle{};

	FEmmsViewportOverlay* Overlay = EmmsSubsys->ViewportOverlays.Find(OverlayId);
	if (Overlay != nullptr)
	{
		Overlay->LastDrawUITickCounter = EmmsSubsys->UITickCounter;
		return BeginDraw(Overlay->Widget, RootPanel);
	}
	else
	{
		FEmmsViewportOverlay NewOverlay;
		NewOverlay.Widget = NewObject<UMMWidget>(GetTransientPackage(), NAME_None);
		NewOverlay.Widget->DefaultRootPanel = UVerticalBox::StaticClass();
		NewOverlay.LastDrawUITickCounter = EmmsSubsys->UITickCounter;

		if (UGameViewportClient* GameViewport = World->GetGameViewport())
		{
			GameViewport->AddViewportWidgetContent(NewOverlay.Widget->TakeWidget(), OverlayZOrder);
			NewOverlay.ViewportClient = GameViewport;
		}
#if WITH_EDITOR
		else if (GCurrentLevelEditingViewportClient)
		{
			TSharedPtr<SLevelViewport> LevelViewport = StaticCastSharedPtr<SLevelViewport>(GCurrentLevelEditingViewportClient->GetEditorViewportWidget());
			NewOverlay.AssetViewport = LevelViewport;
			LevelViewport->AddOverlayWidget(NewOverlay.Widget->TakeWidget(), OverlayZOrder);
		}
#endif

		EmmsSubsys->ViewportOverlays.Add(OverlayId, NewOverlay);
		return BeginDraw(NewOverlay.Widget, RootPanel);
	}
}

void UEmmsStatics::EndDraw()
{
	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root was active to end drawing");
		return;
	}

	if (ImplicitHierarchy.Last().ActiveWidgets.Num() > 1)
	{
		FAngelscriptManager::Throw("Panel widgets were still active when the root was ended");
	}

	UMMWidget* Widget = ImplicitHierarchy.Last().Root;
	ImplicitHierarchy.RemoveAt(ImplicitHierarchy.Num() - 1, EAllowShrinking::No);
	Widget->OnEndDraw();
}

FEmmsWidgetHandle UEmmsStatics::BeginWidget(const TSubclassOf<UPanelWidget>& PanelType)
{
	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root is active to add widgets to");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	FEmmsWidgetHandle NewPanel = AddWidget(PanelType);
	ImplicitHierarchy.Last().ActiveWidgets.Add(
		FImplicitHierarchyPanel{NewPanel.Element}
	);

	return NewPanel;
}
FEmmsWidgetHandle UEmmsStatics::BeginExistingWidget(const FEmmsWidgetHandle& PanelWidget)
{
	if (PanelWidget.Element == nullptr)
	{
		FAngelscriptManager::Throw("Invalid panel widget to Begin");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root is active to add widgets to");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	if (ImplicitHierarchy.Last().Root != PanelWidget.WidgetTree)
	{
		FAngelscriptManager::Throw("Panel widget belongs to a different root widget tree");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	ImplicitHierarchy.Last().ActiveWidgets.Add(
		FImplicitHierarchyPanel{PanelWidget.Element}
	);

	return PanelWidget;
}

void UEmmsStatics::EndWidget()
{
	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root was active to end");
		return;
	}

	if (ImplicitHierarchy.Last().ActiveWidgets.Num() == 0)
	{
		FAngelscriptManager::Throw("No panel is active to end");
		return;
	}

	ImplicitHierarchy.Last().ActiveWidgets.RemoveAt(
		ImplicitHierarchy.Last().ActiveWidgets.Num() - 1,
		EAllowShrinking::No
	);

	ImplicitHierarchy.Last().Root->ResetPendingSlotAttributes();
}

FEmmsWidgetHandle UEmmsStatics::AddWidget(const TSubclassOf<UWidget>& WidgetType)
{
	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root is active to add widgets to");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	if (ImplicitHierarchy.Last().ActiveWidgets.Num() == 0)
	{
		FAngelscriptManager::Throw("No panel is active to add widgets to");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	FImplicitHierarchyPanel CurrentPanel = ImplicitHierarchy.Last().ActiveWidgets.Last();

	// If this is a "Within" panel, remove it from the active hierarchy
	if (!CurrentPanel.bIsScope)
	{
		ImplicitHierarchy.Last().ActiveWidgets.RemoveAt(
			ImplicitHierarchy.Last().ActiveWidgets.Num() - 1,
			EAllowShrinking::No
		);
	}

	// Add to the active implicit parent
	return ImplicitHierarchy.Last().Root->GetOrCreateChildWidget(
		WidgetType,
		FEmmsWidgetHandle{
			ImplicitHierarchy.Last().Root,
			CurrentPanel.Element
		}
	);
}

FEmmsWidgetHandle UEmmsStatics::BeginPanelWidget(class asCScriptFunction* ScriptFunction)
{
	return BeginWidget(
		(UClass*)ScriptFunction->userData
	);
}

void UEmmsStatics::EndPanelWidget(class asCScriptFunction* ScriptFunction)
{
	UClass* PanelClass = (UClass*)ScriptFunction->userData;

	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root was active to end");
		return;
	}

	UPanelWidget* Panel = ImplicitHierarchy.Last().GetCurrentPanel();
	if (Panel == nullptr)
	{
		FAngelscriptManager::Throw("No widget was active to end");
		return;
	}

	if (!Panel->IsA(PanelClass))
	{
		FString Message = FString::Printf(TEXT("Mismatched Begin/End: trying to end a %s, but the active panel was a %s"),
			*PanelClass->GetName(), *Panel->GetClass()->GetName());
		FAngelscriptManager::Throw(TCHAR_TO_ANSI(*Message));
		return;
	}

	EndWidget();
}

FEmmsWidgetHandle UEmmsStatics::WithinWidget(const TSubclassOf<UPanelWidget>& PanelType)
{
	if (ImplicitHierarchy.Num() == 0)
	{
		FAngelscriptManager::Throw("No root is active to add widgets to");
		return FEmmsWidgetHandle{nullptr, nullptr};
	}

	FEmmsWidgetHandle NewPanel = AddWidget(PanelType);
	ImplicitHierarchy.Last().ActiveWidgets.Add(
		FImplicitHierarchyPanel{NewPanel.Element, false}
	);

	return NewPanel;
}

FEmmsWidgetHandle UEmmsStatics::WithinPanelWidget(class asCScriptFunction* ScriptFunction)
{
	return WithinWidget(
		(UClass*)ScriptFunction->userData
	);
}

UEmmsEventListener* UEmmsStatics::GetOrCreateEventListener(FEmmsWidgetHandle* Handle, FProperty* Property)
{
	UEmmsEventListener* Listener = Handle->Element->EventListeners.FindRef(Property);
	if (Listener == nullptr)
	{
		Listener = NewObject<UEmmsEventListener>(GetTransientPackage());
		Listener->MMWidget = Handle->WidgetTree;

		FScriptDelegate BindDelegate;
		BindDelegate.BindUFunction(Listener, UEmmsEventListener::NAME_InterceptEvent);

		if (auto* DelegateProperty = CastField<FDelegateProperty>(Property))
		{
			Listener->DelegateProperty = DelegateProperty;
			Listener->SignatureFunction = DelegateProperty->SignatureFunction;

			DelegateProperty->SetValue_InContainer(
				Handle->Element->UMGWidget,
				BindDelegate
			);
		}
		else if (auto* EventProperty = CastField<FMulticastDelegateProperty>(Property))
		{
			Listener->EventProperty = EventProperty;
			Listener->SignatureFunction = EventProperty->SignatureFunction;

			EventProperty->AddDelegate(
				BindDelegate,
				Handle->Element->UMGWidget
			);
		}

		// Detect if the function wants to return an FEventReply
		if (Listener->SignatureFunction != nullptr)
		{
			Listener->ReturnProperty = Listener->SignatureFunction->GetReturnProperty();
			if (auto* ReturnStructProp = CastField<FStructProperty>(Listener->ReturnProperty))
				Listener->bReturnsEventReply = (ReturnStructProp->Struct == FEventReply::StaticStruct());
		}

		Handle->Element->EventListeners.Add(
			Property, Listener
		);
	}

	return Listener;
}

bool UEmmsStatics::WasEventTriggered(FEmmsWidgetHandle* Handle, class asCScriptFunction* ScriptFunction)
{
	if (Handle->Element == nullptr)
		return false;

	FProperty* Prop = (FProperty*)ScriptFunction->userData;
	UEmmsEventListener* Listener = GetOrCreateEventListener(Handle, Prop);
	return Listener->ConsumeTriggered();
}

void UEmmsStatics::WasEventTriggered_Params(class asIScriptGeneric* InGeneric)
{
	asCGeneric* Generic = static_cast<asCGeneric*>(InGeneric);
	auto* ScriptFunction = (asCScriptFunction*)Generic->GetFunction();
	auto* Handle = (FEmmsWidgetHandle*)Generic->GetObject();

	FProperty* Prop = (FProperty*)ScriptFunction->userData;
	UEmmsEventListener* Listener = GetOrCreateEventListener(Handle, Prop);

	if (Listener->ConsumeTriggered())
	{
		int ArgIndex = 0;
		for (TFieldIterator<FProperty> It(Listener->SignatureFunction); It; ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_Parm))
				continue;
			if (It->HasAnyPropertyFlags(CPF_ReturnParm))
				continue;

			It->CopyCompleteValue(
				Generic->GetArgAddress(ArgIndex),
				It->ContainerPtrToValuePtr<void>(Listener->TriggeredParameters));
			ArgIndex += 1;
		}

		Generic->SetReturnByte(true);
	}
	else
	{
		Generic->SetReturnByte(false);
	}
}

void UEmmsStatics::BindToEvent(FEmmsWidgetHandle* Handle, class asCScriptFunction* ScriptFunction, UObject* Object, const FName& FunctionName)
{
	if (Handle->Element == nullptr)
		return;

	FProperty* Prop = (FProperty*)ScriptFunction->userData;
	UEmmsEventListener* Listener = GetOrCreateEventListener(Handle, Prop);

	FScriptDelegate Delegate;
	Delegate.BindUFunction(Object, FunctionName);
	Listener->PendingImmediateDelegates.AddUnique(Delegate);
}

UMMPopupWindow* UEmmsStatics::SpawnPopupWindow(const UObject* WorldContext, const TSubclassOf<UMMPopupWindow>& WindowClass)
{
	if (WindowClass == nullptr)
		return nullptr;

	UWorld* World = GetWorldForUI(WorldContext);
	if (World == nullptr)
		return nullptr;

	UMMPopupWindow* Window = NewObject<UMMPopupWindow>(GetTransientPackage(), WindowClass);
	Window->World = World;
	Window->Spawn();
	return Window;
}

UObject* UEmmsStatics::AsyncLoadAsset(const UObject* WorldContext, const TSubclassOf<UObject>& AssetType, const FString& AssetPath)
{
	UWorld* World = GetWorldForUI(WorldContext);
	if (World == nullptr)
		return nullptr;

	UEmmsUISubsystem* EmmsSubsys = World->GetSubsystem<UEmmsUISubsystem>();
	if (EmmsSubsys == nullptr)
		return nullptr;

	UObject* Asset = EmmsSubsys->AsyncLoadAsset(AssetPath);
	if (Asset != nullptr && !Asset->IsA(AssetType))
		return nullptr;

	return Asset;
}
