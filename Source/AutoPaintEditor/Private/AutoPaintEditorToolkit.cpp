// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoPaintEditorToolkit.h"

#include "AdvancedPreviewScene.h"
#include "AutoPaintCaptureSettings.h"
#include "AutoPaintData.h"
#include "SAutoPaintEditorViewport.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

const FName FAutoPaintEditorToolkit::ViewportTabId(TEXT("AutoPaintEditor_Viewport"));
const FName FAutoPaintEditorToolkit::DetailsTabId(TEXT("AutoPaintEditor_Details"));
const FName FAutoPaintEditorToolkit::SettingsTabId(TEXT("AutoPaintEditor_Settings"));

void FAutoPaintEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(GetBaseToolkitName());

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	TArray<FTab> RegisteredTabs;
	RegisterTabs(RegisteredTabs);

	for (const FTab& Tab : RegisteredTabs)
	{
		InTabManager->RegisterTabSpawner(Tab.TabId, FOnSpawnTab::CreateLambda([DisplayName = Tab.DisplayName, Widget = TWeakPtr<SWidget>(Tab.Widget)](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			return SNew(SDockTab)
			.Label(DisplayName)
			[
				Widget.IsValid() ? Widget.Pin().ToSharedRef() : SNullWidget::NullWidget
			];
		}))
		.SetDisplayName(Tab.DisplayName)
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), Tab.Icon));

		RegisteredTabIds.Add(Tab.TabId);
	}
}

void FAutoPaintEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	for (const FName& TabId : RegisteredTabIds)
	{
		InTabManager->UnregisterTabSpawner(TabId);
	}
}

FText FAutoPaintEditorToolkit::GetBaseToolkitName() const
{
	return FText::FromString(FName::NameToDisplayString(ToolkitName, false));
}

void FAutoPaintEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditAsset);
	Collector.AddReferencedObject(PreviewMeshComponent);
	Collector.AddReferencedObject(CameraMeshComponent);
	Collector.AddReferencedObject(SceneCaptureComponent2D);
	Collector.AddReferencedObject(FloorMeshComponent);
	Collector.AddReferencedObject(CaptureFloorMeshComponent);
	Collector.AddReferencedObject(Settings);
}

void FAutoPaintEditorToolkit::InitAutoPaintAssetEditor(EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost, UObject* ObjectToEdit)
{
	EditAsset = Cast<UAutoPaintData>(ObjectToEdit);
	EditAsset->SetFlags(RF_Transactional);

	Settings = NewObject<UAutoPaintCaptureSettings>(GetTransientPackage());
	UpdateMIDs();
	
	PreviewMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	UpdatePreviewMeshComponent();

	SceneCaptureComponent2D = NewObject<USceneCaptureComponent2D>(GetTransientPackage(), NAME_None, RF_Transient);
	CameraMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	UpdateCameraComponent();

	FloorMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	UpdateFloorMeshComponent();
	
	CaptureFloorMeshComponent = NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	UpdateCaptureFloorMeshComponent();
	
	UpdateVisualizeMID((EditAsset) ? EditAsset->TextureAsset : nullptr);

	GEditor->RegisterForUndo(this);

	CreateInternalWidgets();

	// clang-format off
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_AutoPaintEditor_Layout_v1.1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->AddTab( ViewportTabId, ETabState::OpenedTab )
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab( DetailsTabId, ETabState::OpenedTab )
				->AddTab( SettingsTabId, ETabState::OpenedTab)
				->SetForegroundTab(DetailsTabId)
			)
		)
	);
	// clang-format on

	InitAssetEditor(Mode, InitToolkitHost, *ToolkitName, StandaloneDefaultLayout, true, true, ObjectToEdit, false);

	{
		const TSharedRef<FExtender> ToolbarExtender = MakeShared<FExtender>();

		ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			GetToolkitCommands(),
			FToolBarExtensionDelegate::CreateSP(this, &FAutoPaintEditorToolkit::BuildToolbar)
		);

		AddToolbarExtender(ToolbarExtender);
	}

	RegenerateMenusAndToolbars();
}

void FAutoPaintEditorToolkit::CreateInternalWidgets()
{
	FDetailsViewArgs Args;
	Args.bHideSelectionTip = true;
	Args.NotifyHook = this;

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyModule.CreateDetailView(Args);
	DetailsView->SetObject(EditAsset.Get());

	SettingsView = PropertyModule.CreateDetailView(Args);
	SettingsView->SetObject(Settings.Get());
	
	Viewport = SNew(SAutoPaintEditorViewport)
	.AutoPaintEditorPtr(SharedThis(this));
}

void FAutoPaintEditorToolkit::BuildToolbar(FToolBarBuilder& ToolBarBuilder)
{
	ToolBarBuilder.BeginSection("AutoPaint");
	{
		ToolBarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]
			{
				ClearRenderTargets();
			})),
			NAME_None,
			INVTEXT("Clear RTs"),
			INVTEXT("Clears Render Targets"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ShowFlagsMenu.Decals"));

		ToolBarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]
			{
				Capture();
			})),
			NAME_None,
			INVTEXT("Capture"),
			INVTEXT("Captures mesh to render targets"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SceneCaptureComponent2D"));

		ToolBarBuilder.AddToolBarButton(
			FUIAction(FExecuteAction::CreateLambda([this]
			{
				SetTextureToData();
			})),
			NAME_None,
			INVTEXT("Save Tex"),
			INVTEXT("Save texture from Final RT to Auto Paint Data"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Texture2D"));
	}
	ToolBarBuilder.EndSection();
}

void FAutoPaintEditorToolkit::RegisterTabs(TArray<FTab>& OutTabs) const
{
	OutTabs.Add({ ViewportTabId, INVTEXT("Viewport"), "LevelEditor.Tabs.Viewports", Viewport });
	OutTabs.Add({ DetailsTabId, INVTEXT("Details"), "LevelEditor.Tabs.Details", DetailsView });
	OutTabs.Add({ SettingsTabId, INVTEXT("Settings"), "LevelEditor.Tabs.Details", SettingsView });
}

UStaticMesh* FAutoPaintEditorToolkit::GetEditAssetStaticMesh() const
{
	return EditAsset ? EditAsset->ReferencedStaticMesh.Get() : nullptr;
}

UMeshComponent* FAutoPaintEditorToolkit::GetPreviewMeshComponent() const
{
	return PreviewMeshComponent;
}

void FAutoPaintEditorToolkit::UpdatePreviewMeshComponent()
{
	if (PreviewMeshComponent)
	{
		if (GEditor->PreviewPlatform.GetEffectivePreviewFeatureLevel() <= ERHIFeatureLevel::ES3_1)
		{
			PreviewMeshComponent->SetMobility(EComponentMobility::Static);
		}
		
		if (EditAsset)
		{
			PreviewMeshComponent->SetStaticMesh(EditAsset->ReferencedStaticMesh.Get());
		}
	}
}

USceneCaptureComponent2D* FAutoPaintEditorToolkit::GetCaptureComponent() const
{
	return SceneCaptureComponent2D;
}

UStaticMeshComponent* FAutoPaintEditorToolkit::GetCameraComponent() const
{
	return CameraMeshComponent;
}

void FAutoPaintEditorToolkit::UpdateCameraComponent()
{
	if (SceneCaptureComponent2D)
	{
		TArray<FEngineShowFlagsSetting> ShowFlagsSettings;
		ShowFlagsSettings.Add({"AmbientOcclusion", false});
		ShowFlagsSettings.Add({"Fog", false});
		ShowFlagsSettings.Add({"AtmosphericFog", false});
		ShowFlagsSettings.Add({ TEXT("NaniteMeshes"), false });
		ShowFlagsSettings.Add({ TEXT("Atmosphere"), false });
		ShowFlagsSettings.Add({ TEXT("Bloom"), false });
		ShowFlagsSettings.Add({ TEXT("Lighting"), false });

		SceneCaptureComponent2D->ShowFlagSettings = ShowFlagsSettings;
		SceneCaptureComponent2D->bCaptureEveryFrame = false;
		SceneCaptureComponent2D->bCaptureOnMovement = false;
		SceneCaptureComponent2D->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
		SceneCaptureComponent2D->PostProcessBlendWeight = 1.f;

		if (Settings)
		{
			SceneCaptureComponent2D->CaptureSource = Settings->CaptureSource;
		}

		if (EditAsset)
		{
			SceneCaptureComponent2D->ProjectionType = EditAsset->ProjectionType;
			SceneCaptureComponent2D->OrthoWidth = EditAsset->CameraOrthoWidth;
			SceneCaptureComponent2D->FOVAngle = EditAsset->CameraFOV;
			
			const FVector Location = FVector::UpVector * EditAsset->CameraDistance;
			SceneCaptureComponent2D->SetRelativeLocation(Location);
			SceneCaptureComponent2D->SetRelativeRotation(EditAsset->CameraRotation);
			SceneCaptureComponent2D->UpdateBounds();
		}
	}
	if (CameraMeshComponent)
	{
		if (SceneCaptureComponent2D)
		{
			CameraMeshComponent->SetupAttachment(SceneCaptureComponent2D);
		}
		if (!CameraMeshComponent->GetStaticMesh())
		{
			CameraMeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/EditorMeshes/MatineeCam_SM.MatineeCam_SM")));
		}
	}
}

UStaticMeshComponent* FAutoPaintEditorToolkit::GetFloorMeshComponent() const
{
	return FloorMeshComponent;
}

void FAutoPaintEditorToolkit::UpdateFloorMeshComponent()
{
	if (FloorMeshComponent)
	{
		FloorMeshComponent->SetRelativeLocation(FVector::ZeroVector);

		if (EditAsset)
		{
			const float PlaneSize = 100.f;
			FloorMeshComponent->SetRelativeScale3D(FVector(EditAsset->TextureWorldSize.X / PlaneSize, EditAsset->TextureWorldSize.Y / PlaneSize, 1.f));
		}
		
		if (!FloorMeshComponent->GetStaticMesh())
		{
			FloorMeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/AutoPaint/Resources/Meshes/S_VisualizePlane.S_VisualizePlane")));
		}

		if (Settings && Settings->VisualizeMID)
		{
			FloorMeshComponent->SetMaterial(0, Settings->VisualizeMID);
		}
	}
}

UStaticMeshComponent* FAutoPaintEditorToolkit::GetCaptureFloorMeshComponent() const
{
	return CaptureFloorMeshComponent;
}

void FAutoPaintEditorToolkit::UpdateCaptureFloorMeshComponent()
{
	if (CaptureFloorMeshComponent)
	{
		CaptureFloorMeshComponent->SetRelativeLocation(FVector::ZeroVector);
		CaptureFloorMeshComponent->SetRelativeScale3D(FVector::OneVector * 10);
		
		if (!CaptureFloorMeshComponent->GetStaticMesh())
		{
			CaptureFloorMeshComponent->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/AutoPaint/Resources/Meshes/S_RenderPlane.S_RenderPlane")));
		}
	}
}

FBoxSphereBounds FAutoPaintEditorToolkit::GetComponentsBounds() const
{
	FBox Box(ForceInit);
	if (GetPreviewMeshComponent())
		Box += GetPreviewMeshComponent()->Bounds.GetBox();
	if (GetCaptureComponent())
		Box += GetCaptureComponent()->Bounds.GetBox();
	const FVector Extents = Box.GetExtent() * 1.5;
	return FBoxSphereBounds(Box.GetCenter(), Extents, Extents.Size());
}

void FAutoPaintEditorToolkit::Capture()
{
	UpdateRenderTargets();
	UpdateCameraComponent();
	UpdateFloorMeshComponent();
	
	// Capture
	if (SceneCaptureComponent2D)
	{
		if (Settings && Settings->SceneCaptureRT)
			SceneCaptureComponent2D->TextureTarget = Settings->SceneCaptureRT;
		
		SceneCaptureComponent2D->ClearShowOnlyComponents();
		SceneCaptureComponent2D->ShowOnlyActors.Empty();

		if (PreviewMeshComponent && PreviewMeshComponent->GetStaticMesh())
		{
			SceneCaptureComponent2D->ShowOnlyComponent(PreviewMeshComponent);
		}
		if (CaptureFloorMeshComponent && CaptureFloorMeshComponent->GetStaticMesh())
		{
			SceneCaptureComponent2D->ShowOnlyComponent(CaptureFloorMeshComponent);
		}
		SceneCaptureComponent2D->CaptureScene();

		// Avoid keeping references to Captured components
		SceneCaptureComponent2D->ClearShowOnlyComponents();
		SceneCaptureComponent2D->TextureTarget = nullptr;
	}

	// Draw
	if (Settings)
	{
		if (TObjectPtr<UMaterialInstanceDynamic>& MID = Settings->NormalizeDrawMID)
		{
			MID->SetScalarParameterValue(TEXT("CameraDistance"), EditAsset->CameraDistance);
			// @todo: pivot isn't bottom point?
			MID->SetScalarParameterValue(TEXT("ObjectHeight"), PreviewMeshComponent->Bounds.BoxExtent.Z * 2.f);
		}

		if (TObjectPtr<UMaterialInstanceDynamic>& MID = Settings->PostProcessDrawMID)
		{
			MID->SetScalarParameterValue(TEXT("Distance"), EditAsset->BlurDistance);
		}
		
		Settings->Draw();
	}

	// Update
	UpdateVisualizeMID(Settings ? Settings->FinalRT : nullptr);
}

void FAutoPaintEditorToolkit::UpdateRenderTargets()
{
	if (!EditAsset)
	{
		// @todo: Error
		return;
	}

	if (!Settings)
	{
		// @todo: Error
		return;
	}

	Settings->CreateOrUpdateRenderTarget(EditAsset->SceneCaptureResolution);
}

void FAutoPaintEditorToolkit::ClearRenderTargets()
{
	if (!Settings)
	{
		// @todo: Error
		return;
	}

	Settings->ClearRenderTarget();
}

void FAutoPaintEditorToolkit::UpdateMIDs()
{
	if (!Settings)
	{
		// @todo: Error
		return;
	}
	
	Settings->NormalizeDrawMID = UAutoPaintCaptureSettings::GetOrCreateTransientMID(Settings->NormalizeDrawMID, TEXT("Normalize MID"), Settings->GetDefaultNormalizeDrawMaterial());
	Settings->PostProcessDrawMID = UAutoPaintCaptureSettings::GetOrCreateTransientMID(Settings->PostProcessDrawMID, TEXT("Final MID"), Settings->GetDefaultPostProcessDrawMaterial());
	Settings->VisualizeMID = UAutoPaintCaptureSettings::GetOrCreateTransientMID(Settings->VisualizeMID, TEXT("Visualize MID"), Settings->GetDefaultVisualizeMaterial());
}

void FAutoPaintEditorToolkit::UpdateVisualizeMID(UTexture* InTexture)
{
	if (!InTexture)
	{
		// @todo: Error
		return;
	}
		
	if (!Settings || !EditAsset)
	{
		// @todo: Error
		return;
	}

	TObjectPtr<UMaterialInstanceDynamic>& MID = Settings->VisualizeMID;
	if (!MID)
	{
		// @todo: Error
		return;
	}

	MID->SetTextureParameterValue(TEXT("Texture"), InTexture);
	MID->SetScalarParameterValue(TEXT("HeightWPO"), EditAsset->HeightWPO);
}

void FAutoPaintEditorToolkit::SetTextureToData()
{
	if (!Settings)
	{
		// @todo: Error
		return;
	}

	if (!EditAsset)
	{
		// @todo: Error
		return;
	}

	EditAsset->TextureAsset = Settings->RenderTargetCreateStaticTextureEditorOnly(Settings->FinalRT, TEXT("T_AP_TextureAsset"), EditAsset);

	UpdateVisualizeMID(EditAsset->TextureAsset);
}
