// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RHI.h"

struct AUTOPAINTSHADERS_API FAutoPaintTexturePatchDispatchParams
{
	UTextureRenderTarget2D* CombinedResult;
	UTexture* PatchTexture;
	FIntRect DestinationBounds;

	FMatrix44f HeightmapToPatch;
	FVector2f EdgeUVDeadBorder;
	float FalloffWorldMargin;
	FVector2f PatchWorldDimensions;

	float ZeroInEncoding;
	float HeightScale;
	float HeightOffset;
};

class AUTOPAINTSHADERS_API FAutoPaintTexturePatchGPUInterface
{
public:
	static void Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FAutoPaintTexturePatchDispatchParams& Params);
	static void Dispatch_GameThread(const FAutoPaintTexturePatchDispatchParams& Params);

	/** Dispatches the texture readback compute shader. Can be called from any thread. */
	static void Dispatch(const FAutoPaintTexturePatchDispatchParams& Params);
};