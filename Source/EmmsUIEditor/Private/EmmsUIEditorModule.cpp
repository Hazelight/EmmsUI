#include "EmmsUIEditorModule.h"
#include "AngelscriptCodeModule.h"
#include "ClassGenerator/AngelscriptClassGenerator.h"
#include "MMEditorUtilityTab.h"
#include "MMScriptStructDetailCustomization.h"
#include "MMClassDetailCustomization.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "EmmsEditorWidgetHelpers.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectIterator.h"

IMPLEMENT_MODULE(FEmmsUIEditorModule, EmmsUIEditor);

void FEmmsUIEditorModule::StartupModule()
{
	FAngelscriptClassGenerator::OnPostReload.AddLambda([this](bool bIsFullReload)
	{
		if (bIsFullReload)
			UpdatePostCompile();
	});

	FPropertyEditorModule* PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyModule)
		PropertyModule->RegisterCustomClassLayout("EmmsEditableInstancedStruct", FOnGetDetailCustomizationInstance::CreateStatic(&FEmmsEditableInstancedStructDetailCustomization::MakeInstance));
}

void FEmmsUIEditorModule::ShutdownModule()
{
}


void FEmmsUIEditorModule::UpdatePostCompile()
{
	TSet<FName> PresentNomadTabSpawners;

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
	auto ToolsRootGroup = MenuStructure.GetToolsStructureRoot();

	// Remove old detail customizations
	for (auto& Elem : RegisteredClassDetailCustomizations)
		PropertyModule.UnregisterCustomClassLayout(Elem.Key);
	RegisteredClassDetailCustomizations.Reset();
	for (auto& Elem : RegisteredScriptStructDetailCustomizations)
		PropertyModule.UnregisterCustomPropertyTypeLayout(Elem.Key);
	RegisteredScriptStructDetailCustomizations.Reset();

	for (UClass* Class : TObjectRange<UClass>())
	{
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_NewerVersionExists | CLASS_Deprecated))
			continue;
		if (!Class->bIsScriptClass)
			continue;

		// Register new tab spawners, or re-register changed tab spawners
		if (Class->IsChildOf(UMMEditorUtilityTab::StaticClass()))
		{
			auto* CDO = Class->GetDefaultObject<UMMEditorUtilityTab>();
			FName TabId = Class->GetFName();

			PresentNomadTabSpawners.Add(TabId);
			UClass* PrevClass = RegisteredNomadTabSpawners.FindRef(TabId);

			if (PrevClass == nullptr || PrevClass != Class)
			{
				if (PrevClass != nullptr)
					FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);

				TSharedPtr<FWorkspaceItem> Group = ToolsRootGroup;
				if (!CDO->Category.IsEmpty())
				{
					auto* FoundGroup = ToolsMenuCategories.Find(CDO->Category);
					if (FoundGroup != nullptr)
					{
						Group = *FoundGroup;
					}
					else
					{
						Group = ToolsRootGroup->AddGroup(
							FName(CDO->Category),
							FText::FromString(CDO->Category),
							FText()
						);
						ToolsMenuCategories.Add(CDO->Category, Group);
					}
				}

				FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
					TabId,
					FOnSpawnTab::CreateLambda([Class](const FSpawnTabArgs& SpawnTabArgs) -> TSharedRef<SDockTab>
					{
						TSharedRef<SDockTab> Tab = SNew(SDockTab)
							.TabRole(ETabRole::NomadTab);

						auto* TabObject = NewObject<UMMEditorUtilityTab>(GetTransientPackage(), Class);
						TabObject->SlateTab = Tab;
						TabObject->Spawn();

						return Tab;
					})
				)
					.SetDisplayName(FText::FromString(CDO->TabTitle))
					.SetTooltipText(FText::FromString(CDO->ToolTip))
					.SetGroup(Group.ToSharedRef())
					.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), CDO->Icon))
					.SetMenuType(
						CDO->bShowInToolsMenu ? ETabSpawnerMenuType::Enabled : ETabSpawnerMenuType::Hidden
					)
				;

				RegisteredNomadTabSpawners.Add(TabId, Class);
			}
		}
		// Register new struct detail customizations
		else if (Class->IsChildOf(UMMScriptStructDetailCustomization::StaticClass()))
		{
			auto* CDO = Class->GetDefaultObject<UMMScriptStructDetailCustomization>();
			if (CDO == nullptr)
				continue;
			if (CDO->DetailStruct == nullptr)
				continue;

			TSubclassOf<UMMScriptStructDetailCustomization> CustomizationClass = Class;
			PropertyModule.RegisterCustomPropertyTypeLayout( CDO->DetailStruct->GetFName(),
				FOnGetPropertyTypeCustomizationInstance::CreateLambda(
				[CustomizationClass]() -> TSharedRef<IPropertyTypeCustomization>
				{
					return FMMScriptStructDetailCustomizationWrapper::MakeInstance(CustomizationClass);
				}
			));

			RegisteredScriptStructDetailCustomizations.Add(CDO->DetailStruct->GetFName(), Class);
		}
		// Register new class detail customizations
		else if (Class->IsChildOf(UMMClassDetailCustomization::StaticClass()))
		{
			auto* CDO = Class->GetDefaultObject<UMMClassDetailCustomization>();
			if (CDO == nullptr)
				continue;
			if (CDO->DetailClass.Get() == nullptr)
				continue;

			TSubclassOf<UMMClassDetailCustomization> CustomizationClass = Class;
			PropertyModule.RegisterCustomClassLayout( CDO->DetailClass->GetFName(), FOnGetDetailCustomizationInstance::CreateLambda(
				[CustomizationClass]() -> TSharedRef<IDetailCustomization>
				{
					return MakeShared<FMMDummyDetailCustomization>(CustomizationClass);
				}
			));
		}
	}

	// Remove old tab spawners that have been deleted
	for (auto It = RegisteredNomadTabSpawners.CreateIterator(); It; ++It)
	{
		if (!PresentNomadTabSpawners.Contains(It->Key))
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(It->Key);
			It.RemoveCurrent();
		}
	}
}
