#include "AutoPaintEditorModule.h"

#include "AssetToolsModule.h"
#include "AssetTypeActions_AutoPaintSettings.h"
#include "ContentBrowserModule.h"

#define LOCTEXT_NAMESPACE "FAutoPaintEditorModule"

void FAutoPaintEditorModule::StartupModule()
{
	if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.Add(FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&FAssetTypeActions_AutoPaintSettings::OnExtendContentBrowserAssetSelectionMenu));
		ContentBrowserAssetsExtenderDelegateHandle = CBMenuExtenderDelegates.Last().GetHandle();
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTypeAction = MakeShared<FAssetTypeActions_AutoPaintSettings>();
		AssetTools.RegisterAssetTypeActions(AssetTypeAction.ToSharedRef());
	}
}

void FAutoPaintEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("ContentBrowser"))
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.RemoveAll([&](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
		{
			return Delegate.GetHandle() == ContentBrowserAssetsExtenderDelegateHandle;
		});
	}

	if (FModuleManager::Get().IsModuleLoaded("AssetTools") &&
		AssetTypeAction)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(AssetTypeAction.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FAutoPaintEditorModule, AutoPaintEditor)