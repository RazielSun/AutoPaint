// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoPaintCircleHeightPatchPS.h"

#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PixelShaderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"

class FLandscapeCircleHeightPatchPS : public FGlobalShader
{
	DECLARE_EXPORTED_GLOBAL_SHADER(FLandscapeCircleHeightPatchPS, AUTOPAINTSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FLandscapeCircleHeightPatchPS, FGlobalShader);

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceTexture) // Our input texture
		SHADER_PARAMETER(FVector3f, InCenter)
		SHADER_PARAMETER(float, InRadius)
		SHADER_PARAMETER(float, InFalloff)
		RENDER_TARGET_BINDING_SLOTS() // Holds our output
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	AUTOPAINTSHADERS_API static void AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters);
};

bool FLandscapeCircleHeightPatchPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	// Apparently landscape requires a particular feature level
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5)
		&& !IsConsolePlatform(Parameters.Platform)
		&& !IsMetalMobilePlatform(Parameters.Platform);
}

void FLandscapeCircleHeightPatchPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("CIRCLE_HEIGHT_PATCH"), 1);
}

void FLandscapeCircleHeightPatchPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FLandscapeCircleHeightPatchPS> PixelShader(ShaderMap);

	FIntVector TextureSize = InParameters->InSourceTexture->Desc.Texture->Desc.GetSize();

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("LandmassCircleHeightPatch"),
		PixelShader,
		InParameters,
		FIntRect(0, 0, TextureSize.X, TextureSize.Y));
}

IMPLEMENT_GLOBAL_SHADER(FLandscapeCircleHeightPatchPS, "/Plugin/AutoPaint/Private/AutoPaintCircleHeightPatchPS.usf", "ApplyLandscapeCircleHeightPatch", SF_Pixel);

void FAutoPaintCircleHeightPatchGPUInterface::Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FAutoPaintCircleHeightPatchDispatchParams& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeCircleHeightPatch);

	const TCHAR* OutputName = TEXT("LandscapeCircleHeightPatchOutput");
	const TCHAR* InputCopyName = TEXT("LandscapeCircleHeightPatchInputCopy");

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyLandscapeCirclePatch"));
		
	TRefCountPtr<IPooledRenderTarget> RenderTarget = CreateRenderTarget(Params.CombinedResult->GetResource()->GetTexture2DRHI(), OutputName);
	FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(RenderTarget);

	// Make a copy of our heightmap input
	FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, InputCopyName);

	FRHICopyTextureInfo CopyTextureInfo;
	CopyTextureInfo.NumMips = 1;
	CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
	AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);
	FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));

	FLandscapeCircleHeightPatchPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FLandscapeCircleHeightPatchPS::FParameters>();
	ShaderParams->InCenter = (FVector3f)Params.CenterInHeightmapCoordinates;
	ShaderParams->InRadius = Params.HeightmapRadius;
	ShaderParams->InFalloff = Params.HeightmapFalloff;
	ShaderParams->InSourceTexture = InputCopySRV;
	ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

	FLandscapeCircleHeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams);

	GraphBuilder.Execute();
}

void FAutoPaintCircleHeightPatchGPUInterface::Dispatch_GameThread(const FAutoPaintCircleHeightPatchDispatchParams& Params)
{
	ENQUEUE_RENDER_COMMAND(LandscapeCircleHeightPatch)(
		[Params](FRHICommandListImmediate& RHICmdList)
		{
			Dispatch_RenderThread(RHICmdList, Params);
		});
}

void FAutoPaintCircleHeightPatchGPUInterface::Dispatch(const FAutoPaintCircleHeightPatchDispatchParams& Params)
{
	if (IsInRenderingThread())
	{
		Dispatch_RenderThread(GetImmediateCommandList_ForRenderCommand(), Params);
	}
	else
	{
		Dispatch_GameThread(Params);
	}
}