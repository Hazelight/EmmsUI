#pragma once
#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "EmmsWidgetHandle.h"
#include "MMContextMenu.h"
#include "MMContextMenuStatics.generated.h"

UCLASS(Meta = (ScriptMixin = "FMMContextMenu"))
class EMMSUIEDITOR_API UMMContextMenuMixin : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(ScriptCallable)
	static void BeginSection(FMMContextMenu& Menu, const FString& SectionLabel = TEXT(""));

	UFUNCTION(ScriptCallable)
	static void EndSection(FMMContextMenu& Menu);

	UFUNCTION(ScriptCallable)
	static void BeginSubMenu(FMMContextMenu& Menu, const FString& Label, const FString& Tooltip = TEXT(""));

	UFUNCTION(ScriptCallable)
	static void EndSubMenu(FMMContextMenu& Menu);

	UFUNCTION(ScriptCallable)
	static void AddSeparator(FMMContextMenu& Menu);

	UFUNCTION(ScriptCallable)
	static void AddOption(FMMContextMenu& Menu, const FMMContextMenuOption& Option);

	UFUNCTION(ScriptCallable)
	static void AddClassPicker(FMMContextMenu& Menu, const FMMContextMenuClassPicker& ClassPicker);
};

UCLASS(Meta = (ScriptName = "mm"))
class EMMSUIEDITOR_API UMMContextMenuStatics : public UObject
{
	GENERATED_BODY()

public:

	UFUNCTION(ScriptCallable)
	static void ShowContextMenu(const FMMContextMenu& Menu);

	UFUNCTION(ScriptCallable, Meta = (ScriptName = "ShowContextMenu"))
	static void ShowContextMenuFromAnchor(const FMMContextMenu& Menu, const FGeometry& AnchorGeometry);

	static void ShowContextMenuFromWidget(const FMMContextMenu& Menu, const FEmmsWidgetHandle& WidgetHandle);
	static TSharedPtr<SWidget> GenerateContextMenu(const FMMContextMenu& Menu);
};