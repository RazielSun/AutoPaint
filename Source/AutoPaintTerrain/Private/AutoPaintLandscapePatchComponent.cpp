// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoPaintLandscapePatchComponent.h"

#include "AutoPaintCircleHeightPatchPS.h"
#include "AutoPaintTexturePatchPS.h"
#include "LandscapePatchManager.h"
#include "AutoPaintData.h"
#include "Landscape.h"
#include "Engine/TextureRenderTarget2D.h"

UAutoPaintLandscapePatchComponent::UAutoPaintLandscapePatchComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	bIsEditorOnly = true;
}

UTextureRenderTarget2D* UAutoPaintLandscapePatchComponent::RenderLayer_Native(const FLandscapeBrushParameters& InParameters)
{
	if (!ensure(PatchManager.IsValid()))
	{
		return InParameters.CombinedResult;
	}

	// Circle height patch doesn't affect regular weightmap layers.
	if (InParameters.LayerType != ELandscapeToolTargetType::Heightmap)
	{
		return InParameters.CombinedResult;
	}

	// DEBUG ONLY
	// FTransform HeightmapCoordsToWorld = PatchManager->GetHeightmapCoordsToWorld();
	// double ToHeightmapRadiusScale = GetComponentTransform().GetScale3D().X / HeightmapCoordsToWorld.GetScale3D().X;
	// FVector3d CircleCenterWorld = GetComponentTransform().GetTranslation();
	
	// FAutoPaintCircleHeightPatchDispatchParams Params;
	// Params.CombinedResult = InParameters.CombinedResult;
	// Params.CenterInHeightmapCoordinates = HeightmapCoordsToWorld.InverseTransformPosition(CircleCenterWorld);
	// float RadiusAdjustment = bExclusiveRadius ? 0 : 1;
	// Params.HeightmapRadius = Radius * ToHeightmapRadiusScale + RadiusAdjustment;
	// Params.HeightmapFalloff = Falloff * ToHeightmapRadiusScale + RadiusAdjustment;
	//
	// FAutoPaintCircleHeightPatchGPUInterface::Dispatch(Params);

	if (!Asset)
	{
		return InParameters.CombinedResult;
	}

	UTexture* PatchUObject = Asset->TextureAsset;
	if (!IsValid(PatchUObject))
	{
		return InParameters.CombinedResult;
	}

	FTextureResource* Patch = PatchUObject->GetResource();
	if (!Patch)
	{
		return InParameters.CombinedResult;
	}

	FAutoPaintTexturePatchDispatchParams Params;
	Params.CombinedResult = InParameters.CombinedResult;
	Params.PatchTexture = PatchUObject;

	FIntPoint SourceResolutionIn = FIntPoint(Patch->GetSizeX(), Patch->GetSizeY());
	FIntPoint DestinationResolutionIn = FIntPoint(InParameters.CombinedResult->SizeX, InParameters.CombinedResult->SizeY);
	
	FTransform PatchToWorld;
	GetCommonShaderParams(SourceResolutionIn, DestinationResolutionIn,
		PatchToWorld, Params.PatchWorldDimensions, Params.HeightmapToPatch, 
		Params.DestinationBounds, Params.EdgeUVDeadBorder, Params.FalloffWorldMargin);

	
	double LandscapeHeightScale = Landscape.IsValid() ? Landscape->GetTransform().GetScale3D().Z : 1;
	LandscapeHeightScale = LandscapeHeightScale == 0 ? 1 : LandscapeHeightScale;
	
	// bool bNativeEncoding = HeightSourceMode == ELandscapeTexturePatchSourceMode::InternalTexture
	// 	|| HeightEncoding == ELandscapeTextureHeightPatchEncoding::NativePackedHeight;
	bool bNativeEncoding = false;
	
	// To get height scale in heightmap coordinates, we have to undo the scaling that happens to map the 16bit int to [-256, 256), and undo
	// the landscape actor scale.

	// HeightEncodingSettings.WorldSpaceEncodingScale
	double WorldSpaceEncodingScale = 1;
	// HeightEncodingSettings.ZeroInEncoding
	double ZeroInEncoding = 0;
	
	Params.HeightScale = bNativeEncoding ? 1 : LANDSCAPE_INV_ZSCALE * WorldSpaceEncodingScale / LandscapeHeightScale;

	/**
	 * Whether to apply the patch Z scale to the height stored in the patch.
	 */
	bool bApplyComponentZScale = true;
	if (bApplyComponentZScale)
	{
		FVector3d ComponentScale = PatchToWorld.GetScale3D();
		Params.HeightScale *= ComponentScale.Z;
	}

	Params.ZeroInEncoding = bNativeEncoding ? LandscapeDataAccess::MidValue : ZeroInEncoding;

	Params.HeightOffset = 0;
	// switch (ZeroHeightMeaning)
	// {
	// case ELandscapeTextureHeightPatchZeroHeightMeaning::LandscapeZ:
	// 	break; // no offset necessary
	// case ELandscapeTextureHeightPatchZeroHeightMeaning::PatchZ:
	{
		FVector3d Location = PatchToWorld.GetTranslation() + FVector3d::UpVector * Asset->HeightWPO;
		FVector3d PatchOriginInHeightmapCoords = PatchManager->GetHeightmapCoordsToWorld().InverseTransformPosition(Location);
		Params.HeightOffset = PatchOriginInHeightmapCoords.Z - LandscapeDataAccess::MidValue;
	// 	break;
	}
	// case ELandscapeTextureHeightPatchZeroHeightMeaning::WorldZero:
	// {
	// 	FVector3d WorldOriginInHeightmapCoords = PatchManager->GetHeightmapCoordsToWorld().InverseTransformPosition(FVector::ZeroVector);
	// 	ParamsOut.InHeightOffset = WorldOriginInHeightmapCoords.Z - LandscapeDataAccess::MidValue;
	// 	break;
	// }
	// default:
	// 	ensure(false);
	// }

	FAutoPaintTexturePatchGPUInterface::Dispatch(Params);

	return InParameters.CombinedResult;
}

FTransform UAutoPaintLandscapePatchComponent::GetPatchToWorldTransform() const
{
	FTransform PatchToWorld = GetComponentTransform();

	if (Landscape.IsValid())
	{
		FRotator3d PatchRotator = PatchToWorld.GetRotation().Rotator();
		FRotator3d LandscapeRotator = Landscape->GetTransform().GetRotation().Rotator();
		PatchToWorld.SetRotation(FRotator3d(LandscapeRotator.Pitch, PatchRotator.Yaw, LandscapeRotator.Roll).Quaternion());
	}

	return PatchToWorld;
}

FVector2D UAutoPaintLandscapePatchComponent::GetFullUnscaledWorldSize() const
{
	if (!Asset)
		return FVector2d::One();
	
	// FVector2D Resolution = GetResolution();
	FVector2D Resolution = FVector2D(Asset->SceneCaptureResolution);

	// UnscaledPatchCoverage is meant to represent the distance between the centers of the extremal pixels.
	// That distance in pixels is Resolution-1.
	FVector2d UnscaledPatchCoverage = Asset->TextureWorldSize;
	FVector2D TargetPixelSize(UnscaledPatchCoverage / FVector2D::Max(Resolution - 1, FVector2D(1, 1)));
	return TargetPixelSize * Resolution;
}

void UAutoPaintLandscapePatchComponent::GetCommonShaderParams(const FIntPoint& SourceResolutionIn, const FIntPoint& DestinationResolutionIn, 
	FTransform& PatchToWorldOut, FVector2f& PatchWorldDimensionsOut, FMatrix44f& HeightmapToPatchOut, FIntRect& DestinationBoundsOut, 
	FVector2f& EdgeUVDeadBorderOut, float& FalloffWorldMarginOut) const
{
	PatchToWorldOut = GetPatchToWorldTransform();

	FVector2D FullPatchDimensions = GetFullUnscaledWorldSize();
	PatchWorldDimensionsOut = FVector2f(FullPatchDimensions);

	FTransform FromPatchUVToPatch(FQuat4d::Identity, FVector3d(-FullPatchDimensions.X / 2, -FullPatchDimensions.Y / 2, 0),
		FVector3d(FullPatchDimensions.X, FullPatchDimensions.Y, 1));
	FMatrix44d PatchLocalToUVs = FromPatchUVToPatch.ToInverseMatrixWithScale();

	FTransform LandscapeHeightmapToWorld = PatchManager->GetHeightmapCoordsToWorld();
	FMatrix44d LandscapeToWorld = LandscapeHeightmapToWorld.ToMatrixWithScale();

	FMatrix44d WorldToPatch = PatchToWorldOut.ToInverseMatrixWithScale();

	// In unreal, matrix composition is done by multiplying the subsequent ones on the right, and the result
	// is transpose of what our shader will expect (because unreal right multiplies vectors by matrices).
	FMatrix44d LandscapeToPatchUVTransposed = LandscapeToWorld * WorldToPatch * PatchLocalToUVs;
	HeightmapToPatchOut = (FMatrix44f)LandscapeToPatchUVTransposed.GetTransposed();


	// Get the output bounds, which are used to limit the amount of landscape pixels we have to process. 
	// To get them, convert all of the corners into heightmap 2d coordinates and get the bounding box.
	auto PatchUVToHeightmap2DCoordinates = [&PatchToWorldOut, &FromPatchUVToPatch, &LandscapeHeightmapToWorld](const FVector2f& UV)
	{
		FVector WorldPosition = PatchToWorldOut.TransformPosition(
			FromPatchUVToPatch.TransformPosition(FVector(UV.X, UV.Y, 0)));
		FVector HeightmapCoordinates = LandscapeHeightmapToWorld.InverseTransformPosition(WorldPosition);
		return FVector2d(HeightmapCoordinates.X, HeightmapCoordinates.Y);
	};
	FBox2D FloatBounds(ForceInit);
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(0, 0));
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(0, 1));
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(1, 0));
	FloatBounds += PatchUVToHeightmap2DCoordinates(FVector2f(1, 1));

	DestinationBoundsOut = FIntRect(
		FMath::Clamp(FMath::Floor(FloatBounds.Min.X), 0, DestinationResolutionIn.X - 1),
		FMath::Clamp(FMath::Floor(FloatBounds.Min.Y), 0, DestinationResolutionIn.Y - 1),
		FMath::Clamp(FMath::CeilToInt(FloatBounds.Max.X) + 1, 0, DestinationResolutionIn.X),
		FMath::Clamp(FMath::CeilToInt(FloatBounds.Max.Y) + 1, 0, DestinationResolutionIn.Y));

	// The outer half-pixel shouldn't affect the landscape because it is not part of our official coverage area.
	EdgeUVDeadBorderOut = FVector2f::Zero();
	if (SourceResolutionIn.X * SourceResolutionIn.Y != 0)
	{
		EdgeUVDeadBorderOut = FVector2f(0.5 / SourceResolutionIn.X, 0.5 / SourceResolutionIn.Y);
	}

	FVector3d ComponentScale = PatchToWorldOut.GetScale3D();

	/**
	 * Distance (in unscaled world coordinates) across which to smoothly fall off the patch effects.
	 */
	float Falloff = 0;
	FalloffWorldMarginOut = Falloff / FMath::Min(ComponentScale.X, ComponentScale.Y);
}
