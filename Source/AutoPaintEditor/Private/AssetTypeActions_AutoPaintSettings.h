// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_AutoPaintSettings : public FAssetTypeActions_Base
{
	//~ Begin FAssetTypeActions_Base Interface
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }

	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor) override;

	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return FColor(100, 40, 250); }
	virtual UClass* GetSupportedClass() const override;
	//~ End FAssetTypeActions_Base Interface

	static void CreateAndOpenEditor(FAssetData Asset);

public:
	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& Assets);
};
