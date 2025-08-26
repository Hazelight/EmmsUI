#include "EmmsUISubsystem.h"
#include "EmmsUISubsystem.h"
#include "Engine/GameViewportClient.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "IAssetViewport.h"
#endif

void UEmmsUISubsystem::RemoveOverlayWidget(FEmmsViewportOverlay Overlay)
{
	if (Overlay.Widget == nullptr)
		return;
	if (!Overlay.Widget->GetCachedWidget().IsValid())
		return;

	if (Overlay.ViewportClient.IsValid())
	{
		Overlay.ViewportClient->RemoveViewportWidgetContent(Overlay.Widget->GetCachedWidget().ToSharedRef());
	}

#if WITH_EDITOR
	if (TSharedPtr<IAssetViewport> AssetViewportPinned = Overlay.AssetViewport.Pin())
	{
		AssetViewportPinned->RemoveOverlayWidget(Overlay.Widget->GetCachedWidget().ToSharedRef());
	}
#endif
}

UObject* UEmmsUISubsystem::AsyncLoadAsset(const FString& AssetPath)
{
	FEmmsPendingAssetLoad& PendingAsset = PendingLoads.FindOrAdd(AssetPath);
	PendingAsset.LastRequestedFrameCounter = GFrameCounter;
	PendingAsset.LastRequestedUITickCounter = UITickCounter;

	if (!PendingAsset.bLoading)
	{
		PendingAsset.bLoading = true;

		FString PackagePath;
		FString ObjectPath;

		if (FPackageName::IsValidLongPackageName(AssetPath))
		{
			PackagePath = AssetPath;
			ObjectPath = PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
		}
		else if (FPackageName::IsValidObjectPath(AssetPath))
		{
			ObjectPath = AssetPath;
			PackagePath = FPackageName::ObjectPathToPackageName(ObjectPath);
		}

		if (!ObjectPath.IsEmpty())
		{
			PendingAsset.Asset = FindObject<UObject>(nullptr, *ObjectPath);
			if (PendingAsset.Asset == nullptr)
			{
				LoadPackageAsync(PackagePath, FLoadPackageAsyncDelegate::CreateLambda(
					[
						WeakThis = TWeakObjectPtr<UEmmsUISubsystem>(this),
						PackagePath, ObjectPath, AssetPath
					](const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)
					{
						if (!WeakThis.IsValid())
							return;
						FEmmsPendingAssetLoad* CompletedAsset = WeakThis->PendingLoads.Find(AssetPath);
						if (CompletedAsset != nullptr)
						{
							CompletedAsset->Asset = FindObject<UObject>(nullptr, *ObjectPath);
						}
					}
				));
			}
		}
	}

	return PendingAsset.Asset;
}

void UEmmsUISubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (auto It = ViewportOverlays.CreateIterator(); It; ++It)
	{
		if (It->Value.Widget == nullptr || It->Value.LastDrawUITickCounter < UITickCounter-2)
		{
			RemoveOverlayWidget(It->Value);
			It.RemoveCurrent();
		}
	}

	for (auto It = PendingLoads.CreateIterator(); It; ++It)
	{
		if (It->Value.LastRequestedUITickCounter < UITickCounter - 10)
		{
			It.RemoveCurrent();
		}
	}

	UITickCounter += 1;
}

void UEmmsUISubsystem::Deinitialize()
{
	Super::Deinitialize();

	for (auto& OverlayElem : ViewportOverlays)
		RemoveOverlayWidget(OverlayElem.Value);
	ViewportOverlays.Reset();
}
