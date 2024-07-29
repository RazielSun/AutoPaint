// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"

class UAutoPaintData;
class SAutoPaintEditorViewport;
class UStaticMeshComponent;
class USceneCaptureComponent2D;
class UAutoPaintCaptureSettings;

class FAutoPaintEditorToolkit final : public FAssetEditorToolkit, public FGCObject, public FNotifyHook, public FEditorUndoClient
{
	struct FTab
	{
		FName TabId;
		FText DisplayName;
		FName Icon;
		TSharedPtr<SWidget> Widget;
	};
	
public:
	const FString ToolkitName = "AutoPaintEditor";
	
	//~ Begin IToolkit Interface
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FText GetBaseToolkitName() const override;
	virtual FName GetToolkitFName() const override { return *ToolkitName; }
	virtual FString GetWorldCentricTabPrefix() const override { return ToolkitName; }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f); }
	//~ End IToolkit Interface
	
	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override { return ToolkitName; }
	//~ End FGCObject Interface

	void InitAutoPaintAssetEditor(EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit);

	UStaticMesh* GetEditAssetStaticMesh() const;
	
	UMeshComponent* GetPreviewMeshComponent() const;
	void UpdatePreviewMeshComponent();

	USceneCaptureComponent2D* GetCaptureComponent() const;
	UStaticMeshComponent* GetCameraComponent() const;
	void UpdateCameraComponent();

	UStaticMeshComponent* GetFloorMeshComponent() const;
	void UpdateFloorMeshComponent();

	UStaticMeshComponent* GetCaptureFloorMeshComponent() const;
	void UpdateCaptureFloorMeshComponent();

	FBoxSphereBounds GetComponentsBounds() const;

	void Capture();
	
	void UpdateRenderTargets();
	void ClearRenderTargets();

	void UpdateMIDs();

	void UpdateVisualizeMID(UTexture* InTexture);

	void SetTextureToData();

private:
	TObjectPtr<UAutoPaintData> EditAsset = nullptr;
	
	/** Component for the preview mesh. */
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent = nullptr;
	TObjectPtr<UStaticMeshComponent> CameraMeshComponent = nullptr;
	TObjectPtr<UStaticMeshComponent> FloorMeshComponent = nullptr;
	TObjectPtr<UStaticMeshComponent> CaptureFloorMeshComponent = nullptr;

	/** Component for capture render targets */
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent2D = nullptr;

	TObjectPtr<UAutoPaintCaptureSettings> Settings = nullptr;

	TSharedPtr<SAutoPaintEditorViewport> Viewport;

	void CreateInternalWidgets();
	void BuildToolbar(FToolBarBuilder& ToolBarBuilder);
	void RegisterTabs(TArray<FTab>& OutTabs) const;

	TArray<FName> RegisteredTabIds;

	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<IDetailsView> SettingsView;

	/**	Graph editor tab */
	static const FName ViewportTabId;
	static const FName DetailsTabId;
	static const FName SettingsTabId;
};