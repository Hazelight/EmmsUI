#include "MMContextMenuStatics.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "EmmsWidgetElement.h"
#include "SClassViewer.h"
#include "ClassViewerFilter.h"
#include "AngelscriptBinds.h"

void UMMContextMenuMixin::BeginSection(FMMContextMenu& Menu, const FString& SectionLabel)
{
	FMMContextMenu::FContextItem& Item = Menu.ContextItems.Emplace_GetRef();
	Item.Parent = Menu.ActiveContextItem;
	Item.Type = FMMContextMenu::EContextItemType::Section;
	Item.Option.Label = SectionLabel;

	Menu.ActiveContextItem = Menu.ContextItems.Num() - 1;
}

void UMMContextMenuMixin::EndSection(FMMContextMenu& Menu)
{
	if (Menu.ActiveContextItem == -1)
		return;
	FMMContextMenu::FContextItem& Item = Menu.ContextItems[Menu.ActiveContextItem];
	Menu.ActiveContextItem = Item.Parent;
}

void UMMContextMenuMixin::BeginSubMenu(FMMContextMenu& Menu, const FString& Entry, const FString& Tooltip)
{
	FMMContextMenu::FContextItem& Item = Menu.ContextItems.Emplace_GetRef();
	Item.Parent = Menu.ActiveContextItem;
	Item.Type = FMMContextMenu::EContextItemType::SubMenu;
	Item.Option.Label = Entry;
	Item.Option.Tooltip = Tooltip;

	Menu.ActiveContextItem = Menu.ContextItems.Num() - 1;
}

void UMMContextMenuMixin::EndSubMenu(FMMContextMenu& Menu)
{
	if (Menu.ActiveContextItem == -1)
		return;
	FMMContextMenu::FContextItem& Item = Menu.ContextItems[Menu.ActiveContextItem];
	Menu.ActiveContextItem = Item.Parent;
}

void UMMContextMenuMixin::AddSeparator(FMMContextMenu& Menu)
{
	FMMContextMenu::FContextItem& Item = Menu.ContextItems.Emplace_GetRef();
	Item.Parent = Menu.ActiveContextItem;
	Item.Type = FMMContextMenu::EContextItemType::Separator;
}

void UMMContextMenuMixin::AddOption(FMMContextMenu& Menu, const FMMContextMenuOption& Option)
{
	FMMContextMenu::FContextItem& Item = Menu.ContextItems.Emplace_GetRef();
	Item.Parent = Menu.ActiveContextItem;
	Item.Type = FMMContextMenu::EContextItemType::Option;
	Item.Option = Option;
}

void UMMContextMenuMixin::AddClassPicker(FMMContextMenu& Menu, const FMMContextMenuClassPicker& ClassPicker)
{
	FMMContextMenu::FContextItem& Item = Menu.ContextItems.Emplace_GetRef();
	Item.Parent = Menu.ActiveContextItem;
	Item.Type = FMMContextMenu::EContextItemType::ClassPicker;
	Item.ClassPicker = ClassPicker;
}

void UMMContextMenuStatics::ShowContextMenu(const FMMContextMenu& Menu)
{
	TSharedPtr<SWidget> MenuWidget = GenerateContextMenu(Menu);
	if (MenuWidget.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().PushMenu(
				ParentWindow.ToSharedRef(),
				FWidgetPath(),
				MenuWidget.ToSharedRef(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}
		else
		{
			ensure(false);
		}
	}
}

void UMMContextMenuStatics::ShowContextMenuFromWidget(const FMMContextMenu& Menu, const FEmmsWidgetHandle& WidgetHandle)
{
	if (WidgetHandle.Element == nullptr || !IsValid(WidgetHandle.Element->UMGWidget))
		return ShowContextMenu(Menu);

	TSharedPtr<SWidget> TargetWidget = WidgetHandle.Element->UMGWidget->GetCachedWidget();
	if (!TargetWidget.IsValid())
		return ShowContextMenu(Menu);

	TSharedPtr<SWidget> MenuWidget = GenerateContextMenu(Menu);
	if (MenuWidget.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(TargetWidget.ToSharedRef());
		if (!ParentWindow.IsValid())
			ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();

		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().PushMenu(
				ParentWindow.ToSharedRef(),
				FWidgetPath(),
				MenuWidget.ToSharedRef(),
				TargetWidget->GetCachedGeometry().GetLayoutBoundingRect().GetTopLeft(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
				true,
				TargetWidget->GetCachedGeometry().GetLayoutBoundingRect().GetSize()
				);
		}
		else
		{
			ensure(false);
		}
	}
}

void UMMContextMenuStatics::ShowContextMenuFromAnchor(const FMMContextMenu& Menu, const FGeometry& AnchorGeometry)
{
	TSharedPtr<SWidget> MenuWidget = GenerateContextMenu(Menu);
	if (MenuWidget.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
		if (ParentWindow.IsValid())
		{
			FSlateApplication::Get().PushMenu(
				ParentWindow.ToSharedRef(),
				FWidgetPath(),
				MenuWidget.ToSharedRef(),
				AnchorGeometry.GetLayoutBoundingRect().GetTopLeft(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu),
				true,
				AnchorGeometry.GetLayoutBoundingRect().GetSize()
				);
		}
		else
		{
			ensure(false);
		}
	}
}

TSharedPtr<SWidget> UMMContextMenuStatics::GenerateContextMenu(const FMMContextMenu& Menu)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	struct FMenuGenerator
	{
		static void Generate(const FMMContextMenu& Menu, FMenuBuilder& Builder, int ParentIndex)
		{
			for (int32 ItemIndex = 0, Count = Menu.ContextItems.Num(); ItemIndex < Count; ++ItemIndex)
			{
				auto& Item = Menu.ContextItems[ItemIndex];
				if (Item.Parent != ParentIndex)
					continue;

				if (Item.Type == FMMContextMenu::EContextItemType::Section)
				{
					Builder.BeginSection(*Item.Option.Label, FText::FromString(Item.Option.Label));
					Generate(Menu, Builder, ItemIndex);
					Builder.EndSection();
				}
				else if (Item.Type == FMMContextMenu::EContextItemType::SubMenu)
				{
					FSlateIcon Icon;
					if (!Item.Option.Icon.IsNone())
						Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), Item.Option.Icon);

					Builder.AddSubMenu(
						FText::FromString(Item.Option.Label),
						FText::FromString(Item.Option.Tooltip),
						FNewMenuDelegate::CreateLambda([Menu, ItemIndex](FMenuBuilder& SubBuilder)
						{
							Generate(Menu, SubBuilder, ItemIndex);
						}),
						false,
						Icon);
				}
				else if (Item.Type == FMMContextMenu::EContextItemType::Separator)
				{
					Builder.AddMenuSeparator();
				}
				else if (Item.Type == FMMContextMenu::EContextItemType::Option && Item.Option.Type == EMMContextMenuOptionType::Option)
				{
					FSlateIcon Icon;
					if (!Item.Option.Icon.IsNone())
						Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), Item.Option.Icon);

					FUIAction UIAction(
						FExecuteAction::CreateLambda([Option=Item.Option]()
						{
							FEditorScriptExecutionGuard ScopeAllowScript;
							Option.OnClicked.ExecuteIfBound();
						}),
						FCanExecuteAction::CreateLambda([Active=!Item.Option.bDisabled]() -> bool
						{
							return Active;
						})
					);

					Builder.AddMenuEntry(
						FText::FromString(Item.Option.Label),
						FText::FromString(Item.Option.Tooltip),
						Icon,
						UIAction
					);
				}
				else if (Item.Type == FMMContextMenu::EContextItemType::Option && Item.Option.Type == EMMContextMenuOptionType::Checkbox)
				{
					FSlateIcon Icon;
					if (!Item.Option.Icon.IsNone())
						Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), Item.Option.Icon);

					FUIAction UIAction(
						FExecuteAction::CreateLambda([Option=Item.Option]()
						{
							FEditorScriptExecutionGuard ScopeAllowScript;
							Option.OnClicked.ExecuteIfBound();
						}),
						FCanExecuteAction::CreateLambda([Active=!Item.Option.bDisabled]() -> bool
						{
							return Active;
						}),
						FGetActionCheckState::CreateLambda([State=Item.Option.bChecked]()
						{
							return State ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
						})
					);

					Builder.AddMenuEntry(
						FText::FromString(Item.Option.Label),
						FText::FromString(Item.Option.Tooltip),
						Icon,
						UIAction,
						NAME_None,
						EUserInterfaceActionType::Check
					);
				}
				else if (Item.Type == FMMContextMenu::EContextItemType::ClassPicker)
				{
					class FClassFilter : public IClassViewerFilter
					{
					public:
						const UClass* BaseClass = nullptr;

						virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
						{
							return BaseClass == nullptr || InClass->IsChildOf(BaseClass);
						};

						virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
						{
							return BaseClass == nullptr || InUnloadedClassData->IsChildOf(BaseClass);
						};
					};
					
					TSharedPtr<FClassFilter> Filter = MakeShareable(new FClassFilter);
					Filter->BaseClass = Item.ClassPicker.BaseClass.Get();
					
					FClassViewerInitializationOptions InitOptions;
					InitOptions.ClassFilters.Add(Filter.ToSharedRef());
					InitOptions.DisplayMode = Item.ClassPicker.bUseTreeView ? EClassViewerDisplayMode::TreeView : EClassViewerDisplayMode::DefaultView;
					InitOptions.bIsActorsOnly = Item.ClassPicker.bIsActorsOnly;
					InitOptions.bIsPlaceableOnly = Item.ClassPicker.bIsPlaceableOnly;
					InitOptions.bExpandAllNodes = Item.ClassPicker.bExpandAllNodes;

					FClassViewerModule& ClassViewerModule = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");
					TSharedRef<SWidget> ClassViewer = ClassViewerModule.CreateClassViewer(
						InitOptions,
						FOnClassPicked::CreateLambda([Delegate=Item.ClassPicker.OnClassSelected](UClass* Class)
						{
							FEditorScriptExecutionGuard ScopeAllowScript;

							if (Class == nullptr)
								return;

							FSlateApplication::Get().DismissAllMenus();
							Delegate.ExecuteIfBound(Class);
						})
					);
					
					Builder.AddWidget(
						ClassViewer,
						FText::FromString(Item.ClassPicker.Label),
						false,
						false,
						FText::FromString(Item.ClassPicker.Tooltip)
					);
				}
			}
		};
	};

	FMenuGenerator::Generate(Menu, MenuBuilder, -1);

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_MMContextMenu((int32)FAngelscriptBinds::EOrder::Late + 250, []
{
	{
		FAngelscriptBinds::FNamespace ns("mm");
		FAngelscriptBinds::BindGlobalFunction("void ShowContextMenu(const FMMContextMenu& Menu, const mm<UWidget>& Anchor)", &UMMContextMenuStatics::ShowContextMenuFromWidget);
	}
});