// Fill out your copyright notice in the Description page of Project Settings.


#include "AssetTypeActions_AutoPaintSettings.h"

#include "AutoPaintData.h"
#include "AutoPaintEditorToolkit.h"
#include "AssetRegistry/AssetRegistryModule.h"

void FAssetTypeActions_AutoPaintSettings::OpenAssetEditor(const TArray<UObject*>& InObjects,
                                                          TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Object : InObjects)
	{
		const TSharedPtr<FAutoPaintEditorToolkit> NewEditor = MakeShared<FAutoPaintEditorToolkit>();
		if (!NewEditor)
		{
			FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
			return;
		}

		if (ensure(Object) &&
			ensure(NewEditor))
		{
			const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
			NewEditor->InitAutoPaintAssetEditor(Mode, EditWithinLevelEditor, Object);
		}
		else
		{
			FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
		}
	}
}

FText FAssetTypeActions_AutoPaintSettings::GetName() const
{
	return UAutoPaintData::StaticClass()->GetDisplayNameText();
}

UClass* FAssetTypeActions_AutoPaintSettings::GetSupportedClass() const
{
	return UAutoPaintData::StaticClass();
}

void FAssetTypeActions_AutoPaintSettings::CreateAndOpenEditor(FAssetData Asset)
{
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	FString PackageName, AssetName, ExtensionName;
	FPaths::Split(Asset.GetPackage()->GetName(), PackageName, AssetName, ExtensionName);
	
	AssetName.RemoveFromStart("SM_");
	AssetName.RemoveFromStart("S_");
	AssetName = "APD_" + AssetName;

	PackageName = FPaths::Combine(PackageName, AssetName);
	AssetTools.CreateUniqueAssetName(PackageName, "", PackageName, AssetName);

	UAutoPaintData* NewData = NewObject<UAutoPaintData>(CreatePackage(*PackageName), *AssetName, RF_Public | RF_Standalone);
	NewData->AssignStaticMesh(Asset);
	NewData->PostEditChange();

	FAssetRegistryModule::AssetCreated(NewData);
	TArray<UObject*> ObjectsToSync{NewData};
	GEditor->SyncBrowserToObjects(ObjectsToSync);

	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewData);
}

TSharedRef<FExtender> FAssetTypeActions_AutoPaintSettings::OnExtendContentBrowserAssetSelectionMenu(
	const TArray<FAssetData>& Assets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	
	if (Assets.Num() != 1)
	{
		return Extender;
	}

	for (const FAssetData& Asset : Assets)
	{
		if (!Asset.GetClass()->IsChildOf<UStaticMesh>())
		{
			return Extender;
		}
	}

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([Assets](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				INVTEXT("Create AutoPaint Data"),
				INVTEXT("Creates autopaint data asset for terrain weight paint"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.ComposureCompositing"),
				FUIAction(FExecuteAction::CreateStatic(&FAssetTypeActions_AutoPaintSettings::CreateAndOpenEditor, Assets[0])));
		})
	);

	return Extender;
}
