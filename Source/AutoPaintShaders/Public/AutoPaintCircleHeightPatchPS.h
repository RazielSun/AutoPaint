// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "RHI.h"

struct AUTOPAINTSHADERS_API FAutoPaintCircleHeightPatchDispatchParams
{
	UTextureRenderTarget2D* CombinedResult;
	FVector3d CenterInHeightmapCoordinates;
	float HeightmapRadius;
	float HeightmapFalloff;
};

class AUTOPAINTSHADERS_API FAutoPaintCircleHeightPatchGPUInterface
{
public:
	static void Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FAutoPaintCircleHeightPatchDispatchParams& Params);
	static void Dispatch_GameThread(const FAutoPaintCircleHeightPatchDispatchParams& Params);

	/** Dispatches the texture readback compute shader. Can be called from any thread. */
	static void Dispatch(const FAutoPaintCircleHeightPatchDispatchParams& Params);
};