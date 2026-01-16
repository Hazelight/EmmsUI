#pragma once
#include "Components/Button.h"
#include "EmmsDefaultWidgetStyles.generated.h"

UCLASS()
class UEmmsDefaultWidgetStyles : public UObject
{
	GENERATED_BODY()

public:

	static void ApplyDefaultStyleToNewWidget(UWidget* Widget);
	static FSlateFontInfo& GetDefaultFont();
	static const FButtonStyle& GetDefaultButtonStyle();
	static const FEditableTextStyle& GetDefaultEditableTextStyle();

};