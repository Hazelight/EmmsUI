#pragma once
#include "CoreMinimal.h"
#include "EmmsWidgetElement.h"
#include "MMWidget.h"
#include "Widgets/SWindow.h"
#include "Tickable.h"
#include "MMPopupWindow.generated.h"

UCLASS(BlueprintType)
class EMMSUI_API UMMPopupWindow : public UObject
{
	GENERATED_BODY()

public:

	// Keeps a strong reference to itself, because generally
	// windows are spawned without a reference being kept to them.
	// This reference will be cleared when the window is closed
	TStrongObjectPtr<UMMPopupWindow> StrongSelf;
	TSharedPtr<SWindow> SlateWindow;
	bool bOpen = false;

	UPROPERTY(EditDefaultsOnly, Category = "Window")
	FString WindowTitle;
	UPROPERTY(EditDefaultsOnly, Category = "Window")
	FVector2D DefaultWindowSize = FVector2D(500, 500);
	UPROPERTY(EditDefaultsOnly, Category = "Window")
	bool bAlwaysOnTop = false;

	UPROPERTY()
	UMMWidget* MMWidget;

	UPROPERTY()
	UWorld* World = nullptr;
	UWorld* GetWorld() const override;

	void Spawn();

	UFUNCTION(ScriptCallable)
	void CloseWindow();

	UFUNCTION(BlueprintImplementableEvent)
	void OnWindowOpened();

	UFUNCTION(BlueprintImplementableEvent)
	void OnWindowClosed();

	UFUNCTION(BlueprintImplementableEvent)
	void DrawWindow(float DeltaTime);

};