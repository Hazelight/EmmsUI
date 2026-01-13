/**
 * An example actor with a button that opens a context menu
 */
class AExample_ActorWithContextMenu : AActor
{
#if EDITOR
	bool bCheckboxChecked = true;

	UFUNCTION(CallInEditor)
	void OpenContextMenu()
	{
		FMMContextMenu ContextMenu;

		// Context menu options can be added to the list bound to a delegate when clicked
		{
			FMMContextMenuOption Option;
			Option.Label = "Context Menu Option";
			Option.Icon = n"GenericCommands.Paste";
			Option.OnClicked.BindUFunction(this, n"OnContextMenuOptionClicked");
			ContextMenu.AddOption(Option);
		}

		// Options can also be checkboxes
		{
			FMMContextMenuOption Option;
			Option.Type = EMMContextMenuOptionType::Checkbox;
			Option.bChecked = bCheckboxChecked;
			Option.Label = "Checkbox";
			Option.Tooltip = "Clicking this option will toggle the checkbox in front of the option";
			Option.OnClicked.BindUFunction(this, n"OnToggleCheckbox");;
			ContextMenu.AddOption(Option);
		}

		// Options can be placed inside sections
		ContextMenu.BeginSection("Section Label");
			{
				FMMContextMenuOption Option;
				Option.Label = "Option Inside Section";
				Option.OnClicked.BindUFunction(this, n"OnSectionOptionClicked");
				ContextMenu.AddOption(Option);
			}
			{
				FMMContextMenuOption Option;
				Option.Label = "Second Option Inside Section";
				Option.OnClicked.BindUFunction(this, n"OnSectionOptionClicked");
				ContextMenu.AddOption(Option);
			}
		ContextMenu.EndSection();

		// To add a separator line:
		ContextMenu.AddSeparator();

		// Submenus can also be added:
		ContextMenu.BeginSubMenu("Submenu Option");
			{
				FMMContextMenuOption Option;
				Option.Label = "Option Inside Submenu";
				Option.Icon = n"GenericCommands.Copy";
				Option.OnClicked.BindUFunction(this, n"OnSubmenuOptionClicked");
				ContextMenu.AddOption(Option);
			}
			{
				FMMContextMenuOption Option;
				Option.Label = "Second Option Inside Submenu";
				Option.Icon = n"Icons.Plus";
				Option.OnClicked.BindUFunction(this, n"OnSubmenuOptionClicked");
				ContextMenu.AddOption(Option);
			}
		ContextMenu.EndSubMenu();

		// Show the context menu we've built:
		mm::ShowContextMenu(ContextMenu);
	}

	UFUNCTION()
	private void OnContextMenuOptionClicked()
	{
		Log("The top context menu option was clicked");
	}

	UFUNCTION()
	private void OnSubmenuOptionClicked()
	{
		Log("An option in a submenu was clicked");
	}

	UFUNCTION()
	private void OnSectionOptionClicked()
	{
		Log("An option in a section was clicked");
	}

	UFUNCTION()
	private void OnToggleCheckbox()
	{
		bCheckboxChecked = !bCheckboxChecked;
	}

#endif
}