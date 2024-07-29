// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "AutoPaintCaptureSettings.generated.h"

class UTextureRenderTarget;
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UTexture;

enum ETextureRenderTargetFormat : int;

UCLASS(Config = Engine, DefaultConfig)
class UAutoPaintCaptureSettings : public UObject
{
	GENERATED_BODY()

public:
	UAutoPaintCaptureSettings();
	
	UPROPERTY(VisibleAnywhere, Category = SceneCapture, Transient)
	TObjectPtr<UTextureRenderTarget2D> SceneCaptureRT = nullptr;

	UPROPERTY(EditAnywhere, config, Category = SceneCapture)
	TEnumAsByte<ETextureRenderTargetFormat> SCRenderTargetFormat;

	UPROPERTY(EditAnywhere, config, Category = SceneCapture)
	TEnumAsByte<ESceneCaptureSource> CaptureSource;

	UPROPERTY(VisibleAnywhere, Category = Draw, Transient)
	TObjectPtr<UTextureRenderTarget2D> NormalizeRT = nullptr;

	UPROPERTY(EditAnywhere, config, Category = Draw)
	TEnumAsByte<ETextureRenderTargetFormat> NRenderTargetFormat;

	UPROPERTY(VisibleAnywhere, Category = Draw, Transient)
	TObjectPtr<UTextureRenderTarget2D> FinalRT = nullptr;

	UPROPERTY(EditAnywhere, config, Category = Draw)
	TEnumAsByte<ETextureRenderTargetFormat> FRenderTargetFormat;

	UPROPERTY(VisibleAnywhere, Category = Draw, Transient)
	TObjectPtr<UMaterialInstanceDynamic> NormalizeDrawMID = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Draw, Transient)
	TObjectPtr<UMaterialInstanceDynamic> PostProcessDrawMID = nullptr;

	UPROPERTY(VisibleAnywhere, Category = Visualize, Transient)
	TObjectPtr<UMaterialInstanceDynamic> VisualizeMID = nullptr;

	UPROPERTY(EditAnywhere, config, Category = Materials, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultVisualizeMaterial;

	UPROPERTY(EditAnywhere, config, Category = Materials, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultNormalizeDrawMaterial;

	UPROPERTY(EditAnywhere, config, Category = Materials, AdvancedDisplay)
	TSoftObjectPtr<UMaterialInterface> DefaultPostProcessDrawMaterial;

	void CreateOrUpdateRenderTarget(const FIntPoint& SceneCaptureResolution);
	void ClearRenderTarget();

	void Draw();
	
	UTexture* RenderTargetCreateStaticTextureEditorOnly(UTextureRenderTarget* InRenderTarget, FString InName, UObject* InOuter);

	UMaterialInterface* GetDefaultVisualizeMaterial() const;
	FSoftObjectPath GetDefaultVisualizeMaterialPath() const { return DefaultVisualizeMaterial.ToSoftObjectPath(); }

	UMaterialInterface* GetDefaultNormalizeDrawMaterial() const;
	FSoftObjectPath GetDefaultNormalizeDrawMaterialPath() const { return DefaultNormalizeDrawMaterial.ToSoftObjectPath(); }

	UMaterialInterface* GetDefaultPostProcessDrawMaterial() const;
	FSoftObjectPath GetDefaultPostProcessDrawMaterialPath() const { return DefaultPostProcessDrawMaterial.ToSoftObjectPath(); }

	static UMaterialInstanceDynamic* GetOrCreateTransientMID(UMaterialInstanceDynamic* InMID, FName InMIDName, UMaterialInterface* InMaterialInterface, EObjectFlags InAdditionalObjectFlags = RF_NoFlags);
	static UTextureRenderTarget2D* GetOrCreateTransientRenderTarget2D(UTextureRenderTarget2D* InRenderTarget, FName InRenderTargetName, const FIntPoint& InSize, ETextureRenderTargetFormat InFormat, 
		const FLinearColor& InClearColor = FLinearColor::Black, bool bInAutoGenerateMipMaps = false);
};