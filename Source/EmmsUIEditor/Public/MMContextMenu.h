#pragma once
#include "CoreMinimal.h"
#include "AngelscriptDelegateWithPayload.h"
#include "MMContextMenu.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FMMContextMenuClassPickerDelegate, UClass*, PickedClass);

UENUM(BlueprintType)
enum class EMMContextMenuOptionType : uint8
{
	Option,
	Checkbox,
};

USTRUCT(BlueprintType)
struct EMMSUIEDITOR_API FMMContextMenuOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	EMMContextMenuOptionType Type = EMMContextMenuOptionType::Option;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	FString Tooltip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	FName Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	bool bDisabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	bool bChecked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu")
	FAngelscriptDelegateWithPayload OnClicked;
};

USTRUCT(BlueprintType)
struct EMMSUIEDITOR_API FMMContextMenuClassPicker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	FString Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	FString Tooltip;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	TSubclassOf<UObject> BaseClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	bool bUseTreeView = false;

	/** Filters so only actors will be displayed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	bool bIsActorsOnly = false;

	/** Filters so only placeable actors will be displayed. Forces bIsActorsOnly to true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	bool bIsPlaceableOnly = false;

	/** If true, all nodes will be expanded by default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	bool bExpandAllNodes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Context Menu Class Picker")
	FMMContextMenuClassPickerDelegate OnClassSelected;
};

USTRUCT(BlueprintType)
struct EMMSUIEDITOR_API FMMContextMenu
{
	GENERATED_BODY()

	enum class EContextItemType
	{
		Section,
		SubMenu,
		Option,
		Separator,
		ClassPicker,
	};

	struct FContextItem
	{
		int Parent = -1;
		EContextItemType Type;

		FMMContextMenuOption Option;
		FMMContextMenuClassPicker ClassPicker;
	};

	int ActiveContextItem = -1;
	TArray<FContextItem> ContextItems;
};