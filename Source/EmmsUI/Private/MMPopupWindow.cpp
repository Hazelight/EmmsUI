#include "MMPopupWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/VerticalBox.h"
#include "EmmsSlotHelpers.h"
#include "EmmsStatics.h"

UWorld* UMMPopupWindow::GetWorld() const
{
	if (World != nullptr)
		return World;
	else
		return GWorld;
}

void UMMPopupWindow::Spawn()
{
	StrongSelf = TStrongObjectPtr<UMMPopupWindow>(this);
	SlateWindow = SNew(SWindow)
		.Title(FText::FromString(WindowTitle))
		.ClientSize(FVector2f(DefaultWindowSize))
	;

	SlateWindow->GetOnWindowClosedEvent().AddLambda(
		[this](const TSharedRef<SWindow>& ClosedWindow)
		{
			if (bOpen)
			{
				bOpen = false;
				FEditorScriptExecutionGuard ScopeAllowScript;
				OnWindowClosed();
			}

			StrongSelf.Reset();
			SlateWindow.Reset();
		}
	);

	MMWidget = NewObject<UMMWidget>(this, UMMWidget::StaticClass(), NAME_None, RF_Transient);
	MMWidget->ExternalDrawFunction = [this](UMMWidget* Widget, float DeltaTime)
	{
		FEditorScriptExecutionGuard ScopeAllowScript;
		DrawWindow(DeltaTime);
	};

	SlateWindow->SetContent(MMWidget->TakeWidget());

	FSlateApplication::Get().AddWindow(SlateWindow.ToSharedRef());

	bOpen = true;
	
	FEditorScriptExecutionGuard ScopeAllowScript;
	OnWindowOpened();
}

void UMMPopupWindow::CloseWindow()
{
	if (!bOpen)
		return;
	if (SlateWindow.IsValid())
		SlateWindow->RequestDestroyWindow();
}