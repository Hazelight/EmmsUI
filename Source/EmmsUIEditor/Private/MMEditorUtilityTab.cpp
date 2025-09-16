#include "MMEditorUtilityTab.h"
#include "Widgets/Docking/SDockTab.h"
#include "Components/VerticalBox.h"
#include "Framework/Docking/TabManager.h"
#include "MMWidget.h"
#include "EmmsStatics.h"
#include "EmmsSlotHelpers.h"
#include "Editor.h"

UWorld* UMMEditorUtilityTab::GetWorld() const
{
	if (World != nullptr)
		return World;
	else
		return GEditor->GetEditorWorldContext().World();
}

bool UMMEditorUtilityTab::IsTabFocused() const
{
	if (!SlateTab.IsValid())
		return false;
	return SlateTab->IsActive();
}

bool UMMEditorUtilityTab::IsTabVisible() const
{
	if (!SlateTab.IsValid())
		return false;
	return SlateTab->IsForeground();
}

void UMMEditorUtilityTab::Spawn()
{
	StrongSelf = TStrongObjectPtr<UMMEditorUtilityTab>(this);

	SlateTab->SetOnTabClosed(
		SDockTab::FOnTabClosedCallback::CreateLambda(
		[this](TSharedRef<SDockTab> ClosedTab)
		{
			if (bOpen)
			{
				bOpen = false;
				FEditorScriptExecutionGuard ScopeAllowScript;
				OnTabClosed();
			}

			StrongSelf.Reset();
			SlateTab.Reset();
		})
	);

	MMWidget = NewObject<UMMWidget>(this, UMMWidget::StaticClass(), NAME_None, RF_Transient);
	MMWidget->ExternalDrawFunction = [this](UMMWidget* Widget, float DeltaTime)
	{
		FEditorScriptExecutionGuard ScopeAllowScript;
		DrawTab(DeltaTime);
	};

	SlateTab->SetContent(MMWidget->TakeWidget());

	bOpen = true;
	FEditorScriptExecutionGuard ScopeAllowScript;
	OnTabOpened();
}

void UMMEditorUtilityTab::CloseTab()
{
	if (SlateTab.IsValid())
		SlateTab->RequestCloseTab();
}

void UMMEditorUtilityTab::SpawnOrFocusTab(TSubclassOf<UMMEditorUtilityTab> TabType)
{
	if (TabType != nullptr)
		FGlobalTabmanager::Get()->TryInvokeTab(TabType.Get()->GetFName());
}