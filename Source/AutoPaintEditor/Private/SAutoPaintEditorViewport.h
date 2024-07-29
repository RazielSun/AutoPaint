// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/GCObject.h"
#include "Layout/Visibility.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "PreviewScene.h"
#include "EditorViewportClient.h"
#include "SEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class FAutoPaintEditorToolkit;
class FAdvancedPreviewScene;

class SAutoPaintEditorViewport : public SEditorViewport, public FGCObject, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS( SAutoPaintEditorViewport ){}
		SLATE_ARGUMENT(TWeakPtr<FAutoPaintEditorToolkit>, AutoPaintEditorPtr)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SAutoPaintEditorViewport();

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("SAutoPaintEditorViewport");
	}
	//~ End FGCObject Interface

	//~ Begin ICommonEditorViewportToolbarInfoProvider Interface
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override
	{
		return SharedThis(this);
	}
	virtual TSharedPtr<FExtender> GetExtenders() const override
	{
		return MakeShared<FExtender>();
	}
	virtual void OnFloatingButtonClicked() override {}
	//~ End ICommonEditorViewportToolbarInfoProvider Interface
	
	TSharedRef<FAdvancedPreviewScene> GetPreviewScene() { return AdvancedPreviewScene.ToSharedRef(); }
	
protected:
	//~ Begin SEditorViewport Interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual bool IsVisible() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
	//~ End SEditorViewport Interface
	
	void AddComponent(USceneComponent* SceneComponent) const;
	void DestroyComponent(USceneComponent* SceneComponent) const;

private:
	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<FAutoPaintEditorToolkit> AutoPaintEditorPtr;

	/** Level viewport client */
	TSharedPtr<class FAutoPaintEditorViewportClient> EditorViewportClient;
	
	/** Preview Scene - uses advanced preview settings */
	TSharedPtr<FAdvancedPreviewScene> AdvancedPreviewScene;
};
