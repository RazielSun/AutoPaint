// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoPaintCaptureSettings.h"

#include "Kismet/KismetRenderingLibrary.h"

UAutoPaintCaptureSettings::UAutoPaintCaptureSettings() :
	SCRenderTargetFormat(RTF_R16f),
	CaptureSource(SCS_SceneDepth),
	NRenderTargetFormat(RTF_RGBA8),
	FRenderTargetFormat(RTF_RGBA8),
	DefaultVisualizeMaterial(FSoftObjectPath(TEXT("/AutoPaint/Resources/Materials/M_Visualize.M_Visualize"))),
	DefaultNormalizeDrawMaterial(FSoftObjectPath(TEXT("/AutoPaint/Resources/Materials/M_NormalizeDraw.M_NormalizeDraw"))),
	DefaultPostProcessDrawMaterial(FSoftObjectPath(TEXT("/AutoPaint/Resources/Materials/M_PostProcessDraw.M_PostProcessDraw")))
{
}

void UAutoPaintCaptureSettings::CreateOrUpdateRenderTarget(const FIntPoint& SceneCaptureResolution)
{
	SceneCaptureRT = GetOrCreateTransientRenderTarget2D(SceneCaptureRT, TEXT("Scene Capture RT"), SceneCaptureResolution, SCRenderTargetFormat);
	NormalizeRT = GetOrCreateTransientRenderTarget2D(NormalizeRT, TEXT("Normalized RT"), SceneCaptureResolution, NRenderTargetFormat);
	FinalRT = GetOrCreateTransientRenderTarget2D(FinalRT, TEXT("Final RT"), SceneCaptureResolution, FRenderTargetFormat);
}

void UAutoPaintCaptureSettings::ClearRenderTarget()
{
	UKismetRenderingLibrary::ClearRenderTarget2D(GWorld, SceneCaptureRT, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(GWorld, NormalizeRT, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(GWorld, FinalRT, FLinearColor::Black);
}

void UAutoPaintCaptureSettings::Draw()
{
	if (!SceneCaptureRT)
	{
		return;
	}
	
	if (!NormalizeRT || !NormalizeDrawMID)
	{
		return;
	}
	
	NormalizeDrawMID->SetTextureParameterValue(FName(TEXT("RT")), SceneCaptureRT);
	UKismetRenderingLibrary::ClearRenderTarget2D(GWorld, NormalizeRT, FLinearColor::Black);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(GWorld, NormalizeRT, NormalizeDrawMID);

	if (!FinalRT || !PostProcessDrawMID)
	{
		return;
	}
	
	PostProcessDrawMID->SetTextureParameterValue(FName(TEXT("RT")), NormalizeRT);
	UKismetRenderingLibrary::ClearRenderTarget2D(GWorld, FinalRT, FLinearColor::Black);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(GWorld, FinalRT, PostProcessDrawMID);
}

UTexture* UAutoPaintCaptureSettings::RenderTargetCreateStaticTextureEditorOnly(UTextureRenderTarget* InRenderTarget, FString InName, UObject* InOuter)
{
	if (InRenderTarget == nullptr)
	{
		return nullptr;
	}
	else if (InRenderTarget->GetResource() == nullptr)
	{
		return nullptr;
	}
	else
	{
		FString Name = InName;
		// if (Name.IsEmpty())
		// 	Name = InRenderTarget->GetOutermost()->GetName();

		UObject* NewObj = nullptr;

		// create a static texture
		FText ErrorMessage;
		NewObj = InRenderTarget->ConstructTexture(InOuter, Name, InRenderTarget->GetMaskedFlags() | RF_Public | RF_Standalone, 
			static_cast<EConstructTextureFlags>(CTF_Default | CTF_AllowMips | CTF_SkipPostEdit), /*InAlphaOverride = */nullptr, &ErrorMessage);

		UTexture* NewTex = Cast<UTexture>(NewObj);
		if (NewTex == nullptr)
		{
			return nullptr;
		}

		// package needs saving
		NewObj->MarkPackageDirty();

		// Update Compression and Mip settings
		NewTex->SRGB = false;
		NewTex->Filter = TextureFilter::TF_Bilinear;
		NewTex->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
		NewTex->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
		NewTex->PostEditChange();

		return NewTex;
	}
	return nullptr;
}

UMaterialInterface* UAutoPaintCaptureSettings::GetDefaultVisualizeMaterial() const
{
	return DefaultVisualizeMaterial.LoadSynchronous();
}

UMaterialInterface* UAutoPaintCaptureSettings::GetDefaultNormalizeDrawMaterial() const
{
	return DefaultNormalizeDrawMaterial.LoadSynchronous();
}

UMaterialInterface* UAutoPaintCaptureSettings::GetDefaultPostProcessDrawMaterial() const
{
	return DefaultPostProcessDrawMaterial.LoadSynchronous();
}

UMaterialInstanceDynamic* UAutoPaintCaptureSettings::GetOrCreateTransientMID(UMaterialInstanceDynamic* InMID, FName InMIDName, UMaterialInterface* InMaterialInterface, EObjectFlags InAdditionalObjectFlags)
{
	if (!IsValid(InMaterialInterface))
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* ResultMID = InMID;

	// If there's no MID yet or if the MID's parent material interface (could be a Material or a MIC) doesn't match the requested material interface (could be a Material, MIC or MID), create a new one : 
	if (!IsValid(InMID) || (InMID->Parent != InMaterialInterface))
	{
		// If the requested material is already a UMaterialInstanceDynamic, we can use it as is :
		ResultMID = Cast<UMaterialInstanceDynamic>(InMaterialInterface);

		if (ResultMID != nullptr)
		{
			ensure(EnumHasAnyFlags(InMaterialInterface->GetFlags(), EObjectFlags::RF_Transient)); // The name of the function implies we're dealing with transient MIDs
		}
		else
		{
			// If it's not a UMaterialInstanceDynamic, it's a UMaterialInstanceConstant or a UMaterial, both of which can be used to create a MID : 
			ResultMID = UMaterialInstanceDynamic::Create(InMaterialInterface, nullptr, MakeUniqueObjectName(GetTransientPackage(), UMaterialInstanceDynamic::StaticClass(), InMIDName));
			ResultMID->SetFlags(InAdditionalObjectFlags);
		}
	}

	check(ResultMID != nullptr);
	return ResultMID;
}

UTextureRenderTarget2D* UAutoPaintCaptureSettings::GetOrCreateTransientRenderTarget2D(UTextureRenderTarget2D* InRenderTarget, FName InRenderTargetName, const FIntPoint& InSize, ETextureRenderTargetFormat InFormat, 
	const FLinearColor& InClearColor, bool bInAutoGenerateMipMaps)
{
	EPixelFormat PixelFormat = GetPixelFormatFromRenderTargetFormat(InFormat);
	if ((InSize.X <= 0) 
		|| (InSize.Y <= 0) 
		|| (PixelFormat == EPixelFormat::PF_Unknown))
	{
		return nullptr;
	}

	if (IsValid(InRenderTarget))
	{
		if ((InRenderTarget->SizeX == InSize.X) 
			&& (InRenderTarget->SizeY == InSize.Y) 
			&& (InRenderTarget->GetFormat() == PixelFormat) // Watch out : GetFormat() returns a EPixelFormat (non-class enum), so we can't compare with a ETextureRenderTargetFormat
			&& (InRenderTarget->ClearColor == InClearColor)
			&& (InRenderTarget->bAutoGenerateMips == bInAutoGenerateMipMaps))
		{
			return InRenderTarget;
		}
	}

	UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UTextureRenderTarget2D::StaticClass(), InRenderTargetName));
	check(NewRenderTarget2D);
	NewRenderTarget2D->RenderTargetFormat = InFormat;
	NewRenderTarget2D->ClearColor = InClearColor;
	NewRenderTarget2D->bAutoGenerateMips = bInAutoGenerateMipMaps;
	NewRenderTarget2D->InitAutoFormat(InSize.X, InSize.Y);
	NewRenderTarget2D->UpdateResourceImmediate(true);

	// Flush RHI thread after creating texture render target to make sure that RHIUpdateTextureReference is executed before doing any rendering with it
	// This makes sure that Value->TextureReference.TextureReferenceRHI->GetReferencedTexture() is valid so that FUniformExpressionSet::FillUniformBuffer properly uses the texture for rendering, instead of using a fallback texture
	ENQUEUE_RENDER_COMMAND(FlushRHIThreadToUpdateTextureRenderTargetReference)(
	[](FRHICommandListImmediate& RHICmdList)
	{
		RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
	});

	return NewRenderTarget2D;
}