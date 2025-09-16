#pragma once
#include "CoreMinimal.h"
#include "MMWidget.h"
#include "IDetailCustomization.h"
#include "UObject/GCObject.h"
#include "MMClassDetailCustomization.generated.h"

UENUM(BlueprintType)
enum class EMMDetailCategoryType : uint8
{
	Variable = 0,
	Transform,
	Important,
	TypeSpecific,
	Default,
	Uncommon
};

USTRUCT(BlueprintType)
struct FMMDetailProperty
{
	GENERATED_BODY()

	UPROPERTY(ScriptReadOnly)
	UUserWidget* NameWidget = nullptr;

	UPROPERTY(ScriptReadOnly)
	UUserWidget* ValueWidget = nullptr;
};

USTRUCT(BlueprintType)
struct FMMDetailImmediateProperty
{
	GENERATED_BODY()

	UPROPERTY(ScriptReadOnly)
	UMMWidget* NameDrawer = nullptr;

	UPROPERTY(ScriptReadOnly)
	UMMWidget* ValueDrawer = nullptr;
};

class FMMDummyDetailCustomization : public IDetailCustomization, public FGCObject
{
public:
	TObjectPtr<UMMClassDetailCustomization> Customization = nullptr;

	FMMDummyDetailCustomization(TSubclassOf<UMMClassDetailCustomization> CustomizationClass);
	virtual ~FMMDummyDetailCustomization();

	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	virtual FString GetReferencerName() const
	{
		return TEXT("FMMDummyDetailCustomization");
	}
};

UCLASS(Blueprintable, Abstract)
class UMMClassDetailCustomization : public UObject, public FTickableGameObject
{
	GENERATED_BODY()
public:

	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_Tick(float DeltaTime);

	virtual bool IsTickableInEditor() const override { return true; }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual bool IsTickable() const override { return !HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject) && !bDestroyed; }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Detail Customization")
	TSubclassOf<UObject> DetailClass;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Detail Customization")
	TArray<UObject*> ObjectsBeingCustomized;

	UPROPERTY()
	TArray<UUserWidget*> UsedUserWidgets;

	UPROPERTY()
	TArray<UMMWidget*> UsedImmediateWidgets;

	UFUNCTION(BlueprintImplementableEvent)
	void BP_CustomizeDetails();

	UFUNCTION(ScriptCallable)
	UObject* GetCustomizedObject();

	UFUNCTION(ScriptCallable)
	void HideProperty(FName PropertyName);

	UFUNCTION(ScriptCallable)
	void HideCategory(FName CategoryName);

	UFUNCTION(ScriptCallable)
	void EditCategory(FName CategoryName, const FString& CategoryDisplayName = TEXT(""), EMMDetailCategoryType CategoryType = EMMDetailCategoryType::Default);

	// Sets the category's order in the details view where lower numbers appear first
	UFUNCTION(ScriptCallable)
	void SetCategorySortOrder(FName CategoryName, int SortOrder);

	// Add a category's default properties to be shown at this position in the detail customization
	UFUNCTION(ScriptCallable)
	void AddAllCategoryDefaultProperties(FName CategoryName, bool bSimpleProperties = true, bool bAdvancedProperties = true);

	// Add properties from a different category into the specified category
	UFUNCTION(ScriptCallable)
	void AddDefaultPropertiesFromOtherCategory(FName AddToCategory, FName PropertiesFromCategory, bool bSimpleProperties = true, bool bAdvancedProperties = true);

	// Add a default property's edit widget to this category
	UFUNCTION(ScriptCallable)
	void AddDefaultProperty(FName CategoryName, FName PropertyName);

	UFUNCTION(ScriptCallable)
	UUserWidget* AddRow(FName CategoryName, TSubclassOf<UUserWidget> RowWidget, const FString& FilterString = TEXT(""), bool bAdvancedDisplay = false);

	UFUNCTION(ScriptCallable)
	UUserWidget* AddProperty(FName CategoryName, const FString& PropertyName, TSubclassOf<UUserWidget> ValueWidget, bool bAdvancedDisplay = false);

	UFUNCTION(ScriptCallable)
	FMMDetailProperty AddCustom(FName CategoryName, TSubclassOf<UUserWidget> NameWidget, TSubclassOf<UUserWidget> ValueWidget, const FString& FilterString = TEXT(""), bool bAdvancedDisplay = false);

	UFUNCTION(ScriptCallable)
	UMMWidget* AddImmediateRow(FName CategoryName, const FString& FilterString = TEXT(""), bool bAdvancedDisplay = false);

	UFUNCTION(ScriptCallable)
	UMMWidget* AddImmediateProperty(FName CategoryName, const FString& PropertyName, bool bAdvancedDisplay = false);

	UFUNCTION(ScriptCallable)
	FMMDetailImmediateProperty AddImmediateCustom(FName CategoryName, const FString& FilterString = TEXT(""), bool bAdvancedDisplay = false);

	UFUNCTION(ScriptCallable)
	void NotifyPropertyModified(UObject* Object, FName PropertyName);

	UFUNCTION(ScriptCallable)
	void AddExternalObject(FName CategoryName, UObject* Object, bool bHideRootObjectNode = true);

	UFUNCTION(ScriptCallable)
	void AddExternalObjectProperty(FName CategoryName, UObject* Object, FName PropertyName);

	UFUNCTION(ScriptCallable)
	void ForceRefresh();

	void DestroyDetails();
	bool bDestroyed = false;

	class IDetailLayoutBuilder* LastDetailBuilder = nullptr;
	class IDetailLayoutBuilder* ActiveDetailBuilder = nullptr;
	TMap<FName, class IDetailCategoryBuilder*> ActiveCategories;
	class IDetailCategoryBuilder& GetCategory(FName CategoryName);

	void CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder);
};