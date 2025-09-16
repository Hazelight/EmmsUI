#include "MMWidget.h"
#include "Components/VerticalBox.h"
#include "Slate/SObjectWidget.h"
#include "MMRootVerticalBox.h"
#include "EmmsEventListener.h"
#include "EmmsStatics.h"
#include "EmmsSlotHelpers.h"
#include "EmmsDefaultWidgetStyles.h"
#include "Blueprint/WidgetTree.h"

UMMWidget::UMMWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultRootPanel = UMMRootVerticalBox::StaticClass();
}

void UMMWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Potentially draw right away when initialized
	if (bDrawOnConstruct)
		CallDraw(0.f);

	// Add the root immediate widget to our panel if we have one
	DisplayedRootWidget = GetRootUMGWidget();
	if (DisplayedRootWidget != nullptr)
	{
		TSharedRef<SWidget> SlateRootWidget = DisplayedRootWidget->TakeWidget();
		MyWidget = SlateRootWidget;
		((FSlotBase&)MyGCWidget.Pin()->GetChildren()->GetSlotAt(0)).AttachWidget(SlateRootWidget);

		MyGCWidget.Pin()->Invalidate(EInvalidateWidgetReason::Layout);
		OnRootWidgetChanged();
	}

	MyGCWidget.Pin()->SetCanTick(true);
}

void UMMWidget::NativeConstruct()
{
	Super::NativeConstruct();
	MyGCWidget.Pin()->SetCanTick(true);
}

void UMMWidget::OnBeginDraw()
{
	bHasDrawnThisFrame = true;
	LastDrawFrameCounter = GFrameCounter;
	DrawCount += 1;
}

void UMMWidget::OnEndDraw()
{
	UpdateWidgetTree();

	if (!MyGCWidget.IsValid())
		return;

	// If the root immediate widget has changed, replace it in the UMG hierarchy
	if (GetRootUMGWidget() != DisplayedRootWidget)
	{
		DisplayedRootWidget = GetRootUMGWidget();
		bLayoutChanged = true;

		if (DisplayedRootWidget != nullptr)
		{
			TSharedRef<SWidget> SlateRootWidget = DisplayedRootWidget->TakeWidget();
			MyWidget = SlateRootWidget;
			((FSlotBase&)MyGCWidget.Pin()->GetChildren()->GetSlotAt(0)).AttachWidget(SlateRootWidget);
		}
		else
		{
			TSharedRef<SWidget> SlateRootWidget = SNew(SSpacer);
			MyWidget = SlateRootWidget;
			((FSlotBase&)MyGCWidget.Pin()->GetChildren()->GetSlotAt(0)).AttachWidget(SlateRootWidget);
		}

		MyGCWidget.Pin()->Invalidate(EInvalidateWidgetReason::Layout);
		OnRootWidgetChanged();
	}
}

void UMMWidget::CallDraw(float InDeltaTime)
{
	if (!bAllowDraw)
		return;

	if (GetClass()->bIsScriptClass || ExternalDrawFunction)
	{
		UEmmsStatics::BeginDraw(this, DefaultRootPanel);

		FEditorScriptExecutionGuard ScopeAllowScript;
		DrawWidget(InDeltaTime);
		if (ExternalDrawFunction)
			ExternalDrawFunction(this, InDeltaTime);

		UEmmsStatics::EndDraw();
	}
}

void UMMWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickDraw(InDeltaTime);
}

void UMMWidget::TickDraw(float DeltaTime)
{
	bLayoutChanged = false;
	if (!bHasDrawnThisFrame)
		CallDraw(DeltaTime);
	bHasDrawnThisFrame = false;

	// If we've attached or detached any widgets this frame, recalculate the layout
	if (bLayoutChanged)
	{
		InvalidateLayoutAndVolatility();
		ForceLayoutPrepass();
		bLayoutChanged = false;
	}
}

int32 UMMWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
#if WITH_EDITOR
	// Immediate widgets in the designer should draw during Paint, because they are prevented from running Tick
	if (IsDesignTime())
		((UMMWidget*)this)->TickDraw(Args.GetDeltaTime());
#endif

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

UWidget* UMMWidget::CreateNewWidget(UClass* WidgetType)
{
	UWidget* UMGWidget = NewObject<UWidget>(WidgetTree, WidgetType, NAME_None, RF_Transient);
	UEmmsDefaultWidgetStyles::ApplyDefaultStyleToNewWidget(UMGWidget);
	return UMGWidget;
}

FEmmsWidgetHandle UMMWidget::GetOrCreateRootWidget(
	TSubclassOf<UWidget> WidgetType)
{
	if (WidgetType == nullptr)
		return FEmmsWidgetHandle{nullptr, nullptr};

	if (ActiveRootWidget != nullptr && ActiveRootWidget->UMGWidget->GetClass() == WidgetType)
	{
		PendingRootWidget = ActiveRootWidget;
		return FEmmsWidgetHandle{this, PendingRootWidget};
	}
	else
	{
		PendingRootWidget = new FEmmsWidgetElement();
		PendingRootWidget->UMGWidget = CreateNewWidget(WidgetType);

		return FEmmsWidgetHandle{this, PendingRootWidget};
	}
}

FEmmsWidgetHandle UMMWidget::GetOrCreateChildWidget(
	TSubclassOf<UWidget> WidgetType,
	FEmmsWidgetHandle ParentWidget,
	uint32 HashIdent)
{
	FEmmsWidgetIdentifier Identifier = {WidgetType.Get(), ParentWidget.Element, HashIdent};

	FEmmsWidgetElement* WidgetElement = nullptr;
	auto* ExistingWidgets = AvailableWidgets.Find(Identifier);
	if (ExistingWidgets != nullptr && ExistingWidgets->Num() != 0)
	{
		WidgetElement = (*ExistingWidgets)[0];
		if (ExistingWidgets->Num() <= 1)
			AvailableWidgets.Remove(Identifier);
		else
			ExistingWidgets->RemoveAt(0);
	}
	else
	{
		WidgetElement = new FEmmsWidgetElement();
		WidgetElement->UMGWidget = CreateNewWidget(WidgetType);
	}

	WidgetElement->Parent = ParentWidget.Element;
	if (WidgetElement->Parent != nullptr)
	{
		WidgetElement->Parent->PendingChildren.Add(WidgetElement);

		// Apply pending slot attributes
		UPanelWidget* ParentPanel = Cast<UPanelWidget>(ParentWidget.Element->UMGWidget);
		if (ParentPanel != nullptr)
		{
			UClass* SlotType = ParentPanel->GetSlotClass();

			for (auto& ParentDefaultSlotAttrElem : ParentWidget.Element->DefaultChildSlotAttributes)
			{
				if (ParentDefaultSlotAttrElem.Value.IsEmpty())
					continue;

				auto* GenericSpec = (FEmmsAttributeSpecification*)ParentDefaultSlotAttrElem.Key;
				auto* ResolvedSpec = FEmmsAttributeSpecification::SlotAttributeSpecs.FindRef(
					TPair<FName, UClass*>(GenericSpec->GetAttributeName(), SlotType)
				);

				if (ResolvedSpec != nullptr)
				{
					FEmmsAttributeState& SlotAttrState = WidgetElement->SlotAttributes.FindOrAdd(ResolvedSpec);
					SlotAttrState.SetPendingValue(ResolvedSpec, ParentDefaultSlotAttrElem.Value.GetDataPtr());
				}
			}

			for (auto& SlotAttributeElem : PendingSlotAttributes)
			{
				if (SlotAttributeElem.Value.IsEmpty())
					continue;

				auto* GenericSpec = (FEmmsAttributeSpecification*)SlotAttributeElem.Key;
				auto* ResolvedSpec = FEmmsAttributeSpecification::SlotAttributeSpecs.FindRef(
					TPair<FName, UClass*>(GenericSpec->GetAttributeName(), SlotType)
				);

				if (ResolvedSpec != nullptr)
				{
					FEmmsAttributeState& SlotAttrState = WidgetElement->SlotAttributes.FindOrAdd(ResolvedSpec);
					if (SlotAttrState.PendingValue.IsEmpty())
					{
						// Swap memory with the pending attribute
						FEmmsAttributeValue EmptyValue = MoveTemp(SlotAttrState.PendingValue);
						SlotAttrState.PendingValue = MoveTemp(SlotAttributeElem.Value);
						SlotAttributeElem.Value = MoveTemp(EmptyValue);
					}
					else
					{
						// Override the pending attribute
						SlotAttrState.SetPendingValue(ResolvedSpec, SlotAttributeElem.Value.GetDataPtr());
						GenericSpec->ResetValue(SlotAttributeElem.Value);
					}
				}
				else
				{
					GenericSpec->ResetValue(SlotAttributeElem.Value);
				}
			}
		}
		else
		{
			ResetPendingSlotAttributes();
		}
	}

	PendingWidgets.FindOrAdd(Identifier).Add(WidgetElement);
	return FEmmsWidgetHandle{this, WidgetElement};
}

void UMMWidget::ResetPendingSlotAttributes()
{
	for (auto& SlotAttributeElem : PendingSlotAttributes)
	{
		auto* GenericSpec = (FEmmsAttributeSpecification*)SlotAttributeElem.Key;
		GenericSpec->ResetValue(SlotAttributeElem.Value);
	}
}

void UMMWidget::UpdateWidgetTree()
{
	// Delete widgets that weren't used this frame
	for (auto& UnusedList : AvailableWidgets)
	{
		for (auto* Element : UnusedList.Value)
		{
			if (Element->UMGWidget != nullptr)
				Element->UMGWidget->RemoveFromParent();
			delete Element;
		}
	}

	// Swap the previous and current lists, but maintain memory allocations
	auto PrevWidgets = MoveTemp(AvailableWidgets);
	AvailableWidgets = MoveTemp(PendingWidgets);
	PrevWidgets.Reset();
	PendingWidgets = MoveTemp(PrevWidgets);

	// Update properties and children of all active widgets
	if (ActiveRootWidget != PendingRootWidget)
	{
		delete ActiveRootWidget;
		ActiveRootWidget = PendingRootWidget;
	}

	PendingRootWidget = nullptr;

	if (ActiveRootWidget != nullptr)
	{
		UpdateWidgetAttributes(ActiveRootWidget);
		UpdateWidgetHierarchy(ActiveRootWidget);
	}
}

void UMMWidget::UpdateWidgetAttributes(FEmmsWidgetElement* Widget)
{
	// Update widget attributes
	bool bRebuildWidget = false;
	for (auto& AttributeElem : Widget->Attributes)
	{
		FEmmsAttributeSpecification* Spec = AttributeElem.Key;
		if (AttributeElem.Value.Update(Spec, Widget->UMGWidget))
		{
			if (Spec->bRequiresWidgetRebuild)
				bRebuildWidget = true;
		}
	}

	// Some attribute changes require the widget to be rebuilt,
	// for example EditDefaultsOnly attributes that the widget doesn't expect to change at runtime
	if (bRebuildWidget && Widget->UMGWidget->GetCachedWidget().IsValid())
	{
		Widget->UMGWidget->RemoveFromParent();
		Widget->UMGWidget->MarkAsGarbage();
		Widget->UMGWidget = CreateNewWidget(Widget->UMGWidget->GetClass());
		Widget->UMGSlot = nullptr;

		// Set all active attribute values on the new widget
		for (auto& AttributeElem : Widget->Attributes)
			AttributeElem.Value.ApplyCurrentToNewContainer(AttributeElem.Key, Widget->UMGWidget);

		// Move event listeners to the new widget
		for (auto& ListenerElem : Widget->EventListeners)
			ListenerElem.Value->MoveToNewContainer(Widget->UMGWidget);

		// We must re-apply the parent's list of children so the new widget gets added to the panel
		if (Widget->Parent != nullptr)
			Widget->Parent->bNeedUpdateChildren = true;

		// Clear the widget's own children, so they get readded
		Widget->ActiveChildren.Reset();
	}

	// Update children
	// This still uses the PendingChildren list, because UpdateWidgetHierarchy has not yet made them active
	for (auto* Child : Widget->PendingChildren)
		UpdateWidgetAttributes(Child);
}

void UMMWidget::UpdateWidgetHierarchy(FEmmsWidgetElement* Widget)
{
	// If the list of children has changed, update the slots of the parent
	if (Widget->PendingChildren != Widget->ActiveChildren
		|| Widget->bNeedUpdateChildren)
	{
		UPanelWidget* Panel = Cast<UPanelWidget>(Widget->UMGWidget);
		if (Panel != nullptr)
		{
			if (Widget->PendingChildren.Num() == 0)
			{
				// No more children, clear previous widgets
				Panel->ClearChildren();
			}
			else
			{
				for (int i = 0, Count = Widget->PendingChildren.Num(); i < Count; ++i)
				{
					if (i >= Panel->GetChildrenCount())
					{
						// Add a new widget
						Panel->AddChild(Widget->PendingChildren[i]->UMGWidget);
					}
					else if (Panel->GetChildAt(i) != Widget->PendingChildren[i]->UMGWidget)
					{
						// Remove all subsequent widgets after one widget is different
						while (i < Panel->GetChildrenCount())
							Panel->RemoveChildAt(i);
						Panel->AddChild(Widget->PendingChildren[i]->UMGWidget);
					}
				}
				
				// Remove any remaining widgets we no longer want
				while (Widget->PendingChildren.Num() < Panel->GetChildrenCount())
					Panel->RemoveChildAt(Widget->PendingChildren.Num());
			}
		}

		bLayoutChanged = true;
		Widget->bNeedUpdateChildren = false;
	}

	// Swap pending and active children lists, but maintain memory allocations
	auto PrevChildren = MoveTemp(Widget->ActiveChildren);
	Widget->ActiveChildren = MoveTemp(Widget->PendingChildren);
	PrevChildren.Reset();
	Widget->PendingChildren = MoveTemp(PrevChildren);

	// When the slot changes, nuke all attributes that are no longer relevant
	if (Widget->UMGWidget->Slot != Widget->UMGSlot)
	{
		Widget->UMGSlot = Widget->UMGWidget->Slot;
		if (Widget->UMGSlot != nullptr)
		{
			for (auto AttrIt = Widget->SlotAttributes.CreateIterator(); AttrIt; ++AttrIt)
			{
				if (AttrIt->Key->IsCompatibleWithContainer(Widget->UMGWidget->Slot))
				{
					AttrIt->Value.MovePendingToNewContainer(AttrIt->Key);
				}
				else
				{
					AttrIt->Value.Reset(AttrIt->Key);
					AttrIt.RemoveCurrent();
				}
			}
		}
	}

	// Update event listeners
	for (auto& ListenerElem : Widget->EventListeners)
		ListenerElem.Value->UpdateEventListener(Widget->UMGWidget);

	// Update slot attributes
	if (Widget->UMGWidget->Slot != nullptr)
	{
		for (auto& AttributeElem : Widget->SlotAttributes)
			AttributeElem.Value.Update(AttributeElem.Key, Widget->UMGWidget->Slot);
	}

	for (auto& AttributeElem : Widget->DefaultChildSlotAttributes)
		AttributeElem.Key->ResetValue(AttributeElem.Value);

	// Update children
	for (auto* Child : Widget->ActiveChildren)
		UpdateWidgetHierarchy(Child);
}

UWidget* UMMWidget::GetRootUMGWidget()
{
	if (ActiveRootWidget != nullptr)
		return ActiveRootWidget->UMGWidget;
	return nullptr;
}

void UMMWidget::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UMMWidget* MMWidget = CastChecked<UMMWidget>(InThis);

	for (auto& WidgetListElem : MMWidget->AvailableWidgets)
	{
		Collector.AddReferencedObject(WidgetListElem.Key.WidgetType);
		for (FEmmsWidgetElement* Widget : WidgetListElem.Value)
			Widget->AddReferencedObjects(Collector);
	}

	for (auto& WidgetListElem : MMWidget->PendingWidgets)
	{
		Collector.AddReferencedObject(WidgetListElem.Key.WidgetType);
		for (FEmmsWidgetElement* Widget : WidgetListElem.Value)
			Widget->AddReferencedObjects(Collector);
	}

	for (auto& AttribElem : MMWidget->PendingSlotAttributes)
	{
		if (AttribElem.Key->bHasObjectReferences)
			AttribElem.Key->AddReferencedObjects(Collector, AttribElem.Value);
	}
}

void UMMWidget::BeginDestroy()
{
	for (auto& WidgetListElem : AvailableWidgets)
	{
		for (FEmmsWidgetElement* Widget : WidgetListElem.Value)
			delete Widget;
	}
	AvailableWidgets.Empty();

	for (auto& WidgetListElem : PendingWidgets)
	{
		for (FEmmsWidgetElement* Widget : WidgetListElem.Value)
			delete Widget;
	}
	PendingWidgets.Empty();

	for (auto& AttribElem : PendingSlotAttributes)
	{
		AttribElem.Key->ResetValue(AttribElem.Value);
	}
	PendingSlotAttributes.Empty();

	Super::BeginDestroy();
}