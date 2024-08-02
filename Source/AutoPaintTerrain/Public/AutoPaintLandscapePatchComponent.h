// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LandscapePatchComponent.h"
#include "AutoPaintLandscapePatchComponent.generated.h"

class UAutoPaintData;

UCLASS(Blueprintable, BlueprintType, ClassGroup = Landscape, meta=(BlueprintSpawnableComponent))
class AUTOPAINTTERRAIN_API UAutoPaintLandscapePatchComponent : public ULandscapePatchComponent
{
	GENERATED_BODY()

public:
	UAutoPaintLandscapePatchComponent();

	UPROPERTY(EditAnywhere, Category = AutoPaint)
	TSoftObjectPtr<UAutoPaintData> Asset = nullptr;

	// DEBUG ONLY
	// UPROPERTY(EditAnywhere, Category = AutoPaint)
	// float Radius = 500;
	//
	// /** Distance across which the alpha will go from 1 down to 0 outside of circle. */
	// UPROPERTY(EditAnywhere, Category = AutoPaint)
	// float Falloff = 500;
	//
	// /** When true, only the vertices in the circle have alpha 1. If false, the radius is expanded slightly so that neighboring 
	//   vertices are also included and the whole circle is able to lie flat. */
	// UPROPERTY(EditAnywhere, Category = AutoPaint, AdvancedDisplay)
	// bool bExclusiveRadius = false;

	UPROPERTY(EditAnywhere, Category = AutoPaint)
	bool bAffectHeightmap = true;

	UPROPERTY(EditAnywhere, Category = AutoPaint)
	TArray<FName> AffectWeightmap;

	/**
	 * Gets the transform from patch to world. The transform is based off of the component
	 * transform, but with rotation changed to align to the landscape, only using the yaw
	 * to rotate it relative to the landscape.
	 */
	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	virtual FTransform GetPatchToWorldTransform() const;

	/**
	 * Gives size in unscaled world coordinates of the patch in the world, based off of UnscaledCoverage and
	 * texture resolution (i.e., adds a half-pixel around UnscaledCoverage).
	 */
	UFUNCTION(BlueprintCallable, Category = LandscapePatch)
	virtual FVector2D GetFullUnscaledWorldSize() const;
	
protected:

	virtual UTextureRenderTarget2D* RenderLayer_Native(const FLandscapeBrushParameters& InParameters) override;

	UTextureRenderTarget2D* ApplyToHeightmap(UTextureRenderTarget2D* InCombinedResult);
	UTextureRenderTarget2D* ApplyToWeightmap(UTextureRenderTarget2D* InCombinedResult);

	void GetCommonShaderParams(const FIntPoint& SourceResolutionIn, const FIntPoint& DestinationResolutionIn, 
		FTransform& PatchToWorldOut, FVector2f& PatchWorldDimensionsOut, FMatrix44f& HeightmapToPatchOut, 
		FIntRect& DestinationBoundsOut, FVector2f& EdgeUVDeadBorderOut, float& FalloffWorldMarginOut) const;

	virtual bool AffectsWeightmapLayer(const FName& InLayerName) const override;
	virtual bool AffectsVisibilityLayer() const override { return false; }

};