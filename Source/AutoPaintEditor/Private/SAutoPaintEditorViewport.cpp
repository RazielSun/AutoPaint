// Fill out your copyright notice in the Description page of Project Settings.


#include "SAutoPaintEditorViewport.h"

#include "AdvancedPreviewScene.h"
#include "AssetViewerSettings.h"
#include "AutoPaintEditorToolkit.h"
#include "ComponentReregisterContext.h"
#include "UnrealWidget.h"
#include "Components/SceneCaptureComponent2D.h"

#define LOCTEXT_NAMESPACE "AutoPaintEditor"

/** Viewport Client for the preview viewport */
class FAutoPaintEditorViewportClient : public FEditorViewportClient
{
public:
	FAutoPaintEditorViewportClient(TWeakPtr<FAutoPaintEditorToolkit> InEditor, FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SAutoPaintEditorViewport>& InEditorViewport);

	// FEditorViewportClient interface
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	virtual bool InputAxis(FViewport* InViewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples/* =1 */, bool bGamepad/* =false */) override;
	virtual FLinearColor GetBackgroundColor() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas) override;
	virtual bool ShouldOrbitCamera() const override;

	/**
	* Focuses the viewport to the center of the bounding box/sphere ensuring that the entire bounds are in view
	*
	* @param Bounds   The bounds to focus
	* @param bInstant Whether or not to focus the viewport instantly or over time
	*/
	void FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant = false);

private:

	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<FAutoPaintEditorToolkit> EditorPtr;

	/** Preview Scene - uses advanced preview settings */
	class FAdvancedPreviewScene* AdvancedPreviewScene;
};


FAutoPaintEditorViewportClient::FAutoPaintEditorViewportClient(TWeakPtr<FAutoPaintEditorToolkit> InEditor, FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SAutoPaintEditorViewport>& InEditorViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InEditorViewport))
	, EditorPtr(InEditor)
{
	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.bDrawGrid = false;
	DrawHelper.GridColorAxis = FColor(80,80,80);
	DrawHelper.GridColorMajor = FColor(72,72,72);
	DrawHelper.GridColorMinor = FColor(64,64,64);
	DrawHelper.PerspectiveGridSize = UE_OLD_HALF_WORLD_MAX1;
	
	SetViewMode(VMI_Lit);
	
	EngineShowFlags.DisableAdvancedFeatures();
	EngineShowFlags.SetSnap(0);
	EngineShowFlags.SetSeparateTranslucency(true);

	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;

	// Don't want to display the widget in this viewport
	Widget->SetDefaultVisibility(false);

	AdvancedPreviewScene = &InPreviewScene;
}

void FAutoPaintEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// Tick the preview scene world.
	PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
}

void FAutoPaintEditorViewportClient::Draw(FViewport* InViewport,FCanvas* Canvas)
{
	FEditorViewportClient::Draw(InViewport, Canvas);
	// EditorPtr.Pin()->DrawMessages(InViewport, Canvas);
}

bool FAutoPaintEditorViewportClient::ShouldOrbitCamera() const
{
	// Should always orbit around the preview object to keep it in view.
	return true;
}

bool FAutoPaintEditorViewportClient::InputKey(const FInputKeyEventArgs& EventArgs)
{
	bool bHandled = FEditorViewportClient::InputKey(EventArgs);

	// Handle viewport screenshot.
	bHandled |= InputTakeScreenshot(EventArgs.Viewport, EventArgs.Key, EventArgs.Event);

	bHandled |= AdvancedPreviewScene->HandleInputKey(EventArgs);

	return bHandled;
}

bool FAutoPaintEditorViewportClient::InputAxis(FViewport* InViewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples/* =1 */, bool bGamepad/* =false */)
{
	bool bResult = true;

	if (!bDisableInput)
	{
		bResult = AdvancedPreviewScene->HandleViewportInput(InViewport, DeviceId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		if (bResult)
		{
			Invalidate();
		}
		else
		{
			bResult = FEditorViewportClient::InputAxis(InViewport, DeviceId, Key, Delta, DeltaTime, NumSamples, bGamepad);
		}
	}

	return bResult;
}

FLinearColor FAutoPaintEditorViewportClient::GetBackgroundColor() const
{
	if (AdvancedPreviewScene != nullptr)
	{
		return AdvancedPreviewScene->GetBackgroundColor();
	}
	return FLinearColor::Black;
}

void FAutoPaintEditorViewportClient::FocusViewportOnBounds(const FBoxSphereBounds Bounds, bool bInstant /*= false*/)
{
	const FVector Position = Bounds.Origin;
	float Radius = Bounds.SphereRadius;

	float AspectToUse = AspectRatio;
	FIntPoint ViewportSize = Viewport->GetSizeXY();
	if (!bUseControllingActorViewInfo && ViewportSize.X > 0 && ViewportSize.Y > 0)
	{
		AspectToUse = Viewport->GetDesiredAspectRatio();
	}

	const bool bEnable = false;
	ToggleOrbitCamera(bEnable);

	/**
	* We need to make sure we are fitting the sphere into the viewport completely, so if the height of the viewport is less
	* than the width of the viewport, we scale the radius by the aspect ratio in order to compensate for the fact that we have
	* less visible vertically than horizontally.
	*/
	if (AspectToUse > 1.0f)
	{
		Radius *= AspectToUse;
	}

	/**
	* Now that we have a adjusted radius, we are taking half of the viewport's FOV,
	* converting it to radians, and then figuring out the camera's distance from the center
	* of the bounding sphere using some simple trig.  Once we have the distance, we back up
	* along the camera's forward vector from the center of the sphere, and set our new view location.
	*/
	const float HalfFOVRadians = FMath::DegreesToRadians(ViewFOV / 2.0f);
	const float DistanceFromSphere = Radius / FMath::Sin(HalfFOVRadians);
	FViewportCameraTransform& ViewTransform = GetViewTransform();
	FVector CameraOffsetVector = ViewTransform.GetRotation().Vector() * -DistanceFromSphere;

	ViewTransform.SetLookAt(Position);
	ViewTransform.TransitionToLocation(Position + CameraOffsetVector, EditorViewportWidget, bInstant);

	// Tell the viewport to redraw itself.
	Invalidate();
}

void SAutoPaintEditorViewport::Construct(const FArguments& InArgs)
{
	AutoPaintEditorPtr = InArgs._AutoPaintEditorPtr;
	
	AdvancedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	AdvancedPreviewScene->SetFloorVisibility(false, true);

	// restore last used feature level
	UWorld* PreviewWorld = AdvancedPreviewScene->GetWorld();
	if (PreviewWorld != nullptr)
	{
		PreviewWorld->ChangeFeatureLevel(GWorld->GetFeatureLevel());
	}	

	UEditorEngine* Editor = CastChecked<UEditorEngine>(GEngine);
	// PreviewFeatureLevelChangedHandle = Editor->OnPreviewFeatureLevelChanged().AddLambda([this](ERHIFeatureLevel::Type NewFeatureLevel)
	// 	{
	// 		AdvancedPreviewScene->GetWorld()->ChangeFeatureLevel(NewFeatureLevel);
	// 	});

	SEditorViewport::Construct( SEditorViewport::FArguments() );

	{
		auto AutoPaintEditor = AutoPaintEditorPtr.Pin();
		AddComponent(AutoPaintEditor->GetPreviewMeshComponent());
		AddComponent(AutoPaintEditor->GetCaptureComponent());
		AddComponent(AutoPaintEditor->GetCameraComponent());
		AddComponent(AutoPaintEditor->GetFloorMeshComponent());
		// AddComponent(AutoPaintEditor->GetCaptureFloorMeshComponent()); // DEBUG
	}
	
	UAssetViewerSettings* Settings = UAssetViewerSettings::Get();
	const int32 ProfileIndex = AdvancedPreviewScene->GetCurrentProfileIndex();
	if (Settings->Profiles.IsValidIndex(ProfileIndex))
	{
		AdvancedPreviewScene->SetEnvironmentVisibility(Settings->Profiles[ProfileIndex].bShowEnvironment, true);
	}

	// OnPropertyChangedHandle = FCoreUObjectDelegates::FOnObjectPropertyChanged::FDelegate::CreateRaw(this, &SMaterialEditor3DPreviewViewport::OnPropertyChanged);
	// OnPropertyChangedHandleDelegateHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.Add(OnPropertyChangedHandle);
}

SAutoPaintEditorViewport::~SAutoPaintEditorViewport()
{
	// CastChecked<UEditorEngine>(GEngine)->OnPreviewFeatureLevelChanged().Remove(PreviewFeatureLevelChangedHandle);
	//
	// UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().RemoveAll(this);
	
	if (EditorViewportClient.IsValid())
	{
		EditorViewportClient->Viewport = NULL;
	}
	//
	// FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnPropertyChangedHandleDelegateHandle);
}

void SAutoPaintEditorViewport::AddReferencedObjects(FReferenceCollector& Collector)
{
	// Collector.AddReferencedObject( PreviewMeshComponent );
	// Collector.AddReferencedObject( PreviewMaterial );
	// Collector.AddReferencedObject( PostProcessVolumeActor );
}

TSharedRef<FEditorViewportClient> SAutoPaintEditorViewport::MakeEditorViewportClient()
{
	EditorViewportClient = MakeShareable( new FAutoPaintEditorViewportClient(AutoPaintEditorPtr, *AdvancedPreviewScene.Get(), SharedThis(this)) );
	// UAssetViewerSettings::Get()->OnAssetViewerSettingsChanged().AddRaw(this, &SAutoPaintEditorViewport::OnAssetViewerSettingsChanged);
	EditorViewportClient->SetRealtime(true);
	EditorViewportClient->SetViewLocation( FVector::ZeroVector );
	EditorViewportClient->SetViewRotation( FRotator(-15.0f, -90.0f, 0.0f) );
	EditorViewportClient->SetViewLocationForOrbiting( FVector::ZeroVector );
	EditorViewportClient->bSetListenerPosition = false;
	EditorViewportClient->EngineShowFlags.EnableAdvancedFeatures();
	EditorViewportClient->EngineShowFlags.SetLighting(true);
	EditorViewportClient->EngineShowFlags.SetIndirectLightingCache(true);
	// EditorViewportClient->EngineShowFlags.SetPostProcessing(true);
	EditorViewportClient->Invalidate();
	EditorViewportClient->VisibilityDelegate.BindSP( this, &SAutoPaintEditorViewport::IsVisible );

	return EditorViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SAutoPaintEditorViewport::MakeViewportToolbar()
{
	return SEditorViewport::MakeViewportToolbar();
}

void SAutoPaintEditorViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);
}

bool SAutoPaintEditorViewport::IsVisible() const
{
	return SEditorViewport::IsVisible();
}

void SAutoPaintEditorViewport::BindCommands()
{
	SEditorViewport::BindCommands();
}

void SAutoPaintEditorViewport::OnFocusViewportToSelection()
{
	if (auto Editor = AutoPaintEditorPtr.Pin())
	{
		EditorViewportClient->FocusViewportOnBounds( Editor->GetComponentsBounds() );
	}
}

void SAutoPaintEditorViewport::AddComponent(USceneComponent* SceneComponent) const
{
	if (!ensure(AdvancedPreviewScene))
	{
		return;
	}

	FComponentReregisterContext ReregisterContext(SceneComponent);;
	AdvancedPreviewScene->AddComponent(SceneComponent, SceneComponent->GetRelativeTransform());
}

void SAutoPaintEditorViewport::DestroyComponent(USceneComponent* SceneComponent) const
{
	if (!ensure(AdvancedPreviewScene))
	{
		return;
	}

	AdvancedPreviewScene->RemoveComponent(SceneComponent);
}

#undef LOCTEXT_NAMESPACE