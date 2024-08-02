// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AutoPaintData.generated.h"

class UStaticMesh;

UCLASS()
class AUTOPAINT_API UAutoPaintData : public UObject
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* PrimaryAssetType = TEXT("AutoPaintData");
	
	//~ Begin UObject Interface
	virtual bool IsEditorOnly() const override
	{
		return true;
	}
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(PrimaryAssetType, GetFName());
	}
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface

	UPROPERTY(VisibleAnywhere, Category = Default)
	TSoftObjectPtr<UStaticMesh> ReferencedStaticMesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Default)
	TObjectPtr<UTexture> TextureAsset = nullptr;

	// Size in cm for Texture/Preview
	UPROPERTY(EditAnywhere, Category = Default)
	FVector2D TextureWorldSize = FVector2D(300.0);

	UPROPERTY(EditAnywhere, Category = Default)
	FVector WorldOffset = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, Category = Capture)
	FIntPoint SceneCaptureResolution = FIntPoint(32, 32);

	/**
	 * Distance (in unscaled world coordinates) across which to smoothly fall off the patch effects.
	 */
	UPROPERTY(EditAnywhere, Category = Render, meta = (ClampMin = "0", UIMax = "2000"))
	float Falloff = 100;

	// In cm
	UPROPERTY(EditAnywhere, Category = Render)
	float HeightWPO = 50.f;

	UPROPERTY(EditAnywhere, Category = Render)
	float BlurDistance = 0.1f;

	UPROPERTY(EditAnywhere, Category = Projection)
	TEnumAsByte<ECameraProjectionMode::Type> ProjectionType = ECameraProjectionMode::Type::Perspective;

	// Distance at which to capture. The Projection Distance should be set as far back as it can without introducing too many Z fighting artifacts.
	UPROPERTY(EditAnywhere, Category = Projection, meta = (EditCondition = "ProjectionType==0", EditConditionHides))
	float CameraDistance = 300.f;

	UPROPERTY(EditAnywhere, Category = Projection, meta = (EditCondition = "ProjectionType==0", EditConditionHides))
	FRotator CameraRotation = FRotator(-90.f, 0.f, -90.f);

	UPROPERTY(EditAnywhere, Category = Projection, meta = (EditCondition = "ProjectionType==0", EditConditionHides))
	float CameraFOV = 60.f;

	UPROPERTY(EditAnywhere, Category = Projection, meta = (EditCondition = "ProjectionType==1", EditConditionHides))
	float CameraOrthoWidth = 100.f;

	void AssignStaticMesh(const FAssetData& InStaticMeshAssetData);
};
