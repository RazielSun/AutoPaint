// Fill out your copyright notice in the Description page of Project Settings.


#include "AutoPaintTexturePatchPS.h"

#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "PixelShaderUtils.h"
#include "LandscapeUtils.h"

/**
 * Shader that applies a texture-based height patch to a landscape heightmap.
 */
class FApplyLandscapeTextureHeightPatchPS : public FGlobalShader
{
	DECLARE_EXPORTED_GLOBAL_SHADER(FApplyLandscapeTextureHeightPatchPS, AUTOPAINTSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FApplyLandscapeTextureHeightPatchPS, FGlobalShader);

public:
	enum class EBlendMode : uint8
	{
		/** Desired height is alpha blended with the current. */
		AlphaBlend,

		/** Desired height is multiplied by alpha and added to current. */
		Additive,

		/** Like AlphaBlend, but patch is limited to only lowering the landscape. */
		Min,

		/** Like AlphaBlend, but patch is limited to only raising the landscape. */
		Max
	};

	// Flags that get packed into a bitfield because we're not allowed to use bool shader parameters:
	enum class EFlags : uint8
	{
		None = 0,

		// When false, falloff is circular.
		RectangularFalloff = 1 << 0,

		// When true, the texture alpha channel is considered for blending (in addition to falloff, if nonzero)
		ApplyPatchAlpha = 1 << 1,

		// When false, the input is directly interpreted as being the height value to process. When true, the height
		// is unpacked from the red and green channels to make a 16 bit int.
		InputIsPackedHeight = 1 << 2
	};

	// TODO: We could consider exposing an additional global alpha setting that we can use to pass in the given
	// edit layer alpha value... On the other hand, we currently don't bother doing this in any existing blueprint
	// brushes, and it would be hard to support in a way that doesn't require each blueprint brush to respect it
	// individually... Not clear whether this is something worth doing yet.

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceHeightmap)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InHeightPatch)
		SHADER_PARAMETER_SAMPLER(SamplerState, InHeightPatchSampler)
		SHADER_PARAMETER(FMatrix44f, InHeightmapToPatch)
		// Value in patch that corresponds to the landscape mid value, which is our "0 height".
		SHADER_PARAMETER(float, InZeroInEncoding)
		// Scale to apply to source values relative to the value that represents 0 height.
		SHADER_PARAMETER(float, InHeightScale)
		// Offset to apply to height result after applying height scale
		SHADER_PARAMETER(float, InHeightOffset)
		// Amount of the patch edge to not apply in UV space. Generally set to 0.5/Dimensions to avoid applying
		// the edge half-pixels.
		SHADER_PARAMETER(FVector2f, InEdgeUVDeadBorder)
		// In world units, the size of the margin across which the alpha falls from 1 to 0
		SHADER_PARAMETER(float, InFalloffWorldMargin)
		// Size of the patch in world units (used for falloff)
		SHADER_PARAMETER(FVector2f, InPatchWorldDimensions)
		SHADER_PARAMETER(uint32, InBlendMode)
		// Some combination of the flags (see constants above).
		SHADER_PARAMETER(uint32, InFlags)

		RENDER_TARGET_BINDING_SLOTS() // Holds our output
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	static void AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters, const FIntRect& DestinationBounds);
};

bool FApplyLandscapeTextureHeightPatchPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return UE::Landscape::DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FApplyLandscapeTextureHeightPatchPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("APPLY_HEIGHT_PATCH"), 1);

	// Make our flag choices match in the shader.
	OutEnvironment.SetDefine(TEXT("RECTANGULAR_FALLOFF_FLAG"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EFlags::RectangularFalloff));
	OutEnvironment.SetDefine(TEXT("APPLY_PATCH_ALPHA_FLAG"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EFlags::ApplyPatchAlpha));
	OutEnvironment.SetDefine(TEXT("INPUT_IS_PACKED_HEIGHT_FLAG"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EFlags::InputIsPackedHeight));

	OutEnvironment.SetDefine(TEXT("ADDITIVE_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::Additive));
	OutEnvironment.SetDefine(TEXT("ALPHA_BLEND_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::AlphaBlend));
	OutEnvironment.SetDefine(TEXT("MIN_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::Min));
	OutEnvironment.SetDefine(TEXT("MAX_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::Max));
}

void FApplyLandscapeTextureHeightPatchPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters, const FIntRect& DestinationBounds)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FApplyLandscapeTextureHeightPatchPS> PixelShader(ShaderMap);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("LandscapeTextureHeightPatch"),
		PixelShader,
		InParameters,
		DestinationBounds);
}

IMPLEMENT_GLOBAL_SHADER(FApplyLandscapeTextureHeightPatchPS, "/Plugin/AutoPaint/Private/AutoPaintTexturePatchPS.usf", "ApplyLandscapeTextureHeightPatch", SF_Pixel);

void FAutoPaintTexturePatchHeightmapGPUInterface::Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FAutoPaintTexturePatchDispatchParams& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeTextureHeightPatch_Render);

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyTextureHeightPatch"));

	TRefCountPtr<IPooledRenderTarget> DestinationRenderTarget = CreateRenderTarget(Params.CombinedResult->GetResource()->GetTexture2DRHI(), TEXT("LandscapeTextureHeightPatchOutput"));
	FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(DestinationRenderTarget);

	// Make a copy of our heightmap input so we can read and write at the same time (needed for blending)
	FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, TEXT("LandscapeTextureHeightPatchInputCopy"));

	FRHICopyTextureInfo CopyTextureInfo;
	CopyTextureInfo.NumMips = 1;
	CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
	AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);

	FApplyLandscapeTextureHeightPatchPS::FParameters* ShaderParams =
		GraphBuilder.AllocParameters<FApplyLandscapeTextureHeightPatchPS::FParameters>();

	ShaderParams->InHeightmapToPatch = Params.HeightmapToPatch;
	ShaderParams->InEdgeUVDeadBorder = Params.EdgeUVDeadBorder;
	ShaderParams->InFalloffWorldMargin = Params.FalloffWorldMargin;
	ShaderParams->InPatchWorldDimensions = Params.PatchWorldDimensions;
	
	ShaderParams->InZeroInEncoding = Params.ZeroInEncoding;
	ShaderParams->InHeightScale = Params.HeightScale;
	ShaderParams->InHeightOffset = Params.HeightOffset;
	
	using EShaderBlendMode = FApplyLandscapeTextureHeightPatchPS::EBlendMode;
	EShaderBlendMode BlendMode = EShaderBlendMode::AlphaBlend;
	ShaderParams->InBlendMode = static_cast<uint32>(BlendMode);
	
	using EShaderFlags = FApplyLandscapeTextureHeightPatchPS::EFlags;
	EShaderFlags Flags = EShaderFlags::None;
	// 	// Pack our booleans into a bitfield
	// 	Flags |= (FalloffMode == ELandscapeTexturePatchFalloffMode::RoundedRectangle) ? EShaderFlags::RectangularFalloff : EShaderFlags::None;
	// 	Flags |= bUseTextureAlphaForHeight ? EShaderFlags::ApplyPatchAlpha : EShaderFlags::None;
	// 	Flags |= bNativeEncoding ? EShaderFlags::InputIsPackedHeight : EShaderFlags::None;
	ShaderParams->InFlags = static_cast<uint8>(Flags);

	TRefCountPtr<IPooledRenderTarget> PatchRenderTarget = CreateRenderTarget(Params.PatchTexture->GetResource()->GetTexture2DRHI(), TEXT("LandscapeTextureHeightPatch"));
	FRDGTextureRef PatchTexture = GraphBuilder.RegisterExternalTexture(PatchRenderTarget);
	FRDGTextureSRVRef PatchSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(PatchTexture, 0));
	ShaderParams->InHeightPatch = PatchSRV;
	ShaderParams->InHeightPatchSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();

	FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));
	ShaderParams->InSourceHeightmap = InputCopySRV;

	ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

	FApplyLandscapeTextureHeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams, Params.DestinationBounds);

	GraphBuilder.Execute();
}

void FAutoPaintTexturePatchHeightmapGPUInterface::Dispatch_GameThread(const FAutoPaintTexturePatchDispatchParams& Params)
{
	ENQUEUE_RENDER_COMMAND(LandscapeCircleHeightPatch)(
		[Params](FRHICommandListImmediate& RHICmdList)
		{
			Dispatch_RenderThread(RHICmdList, Params);
		});
}

void FAutoPaintTexturePatchHeightmapGPUInterface::Dispatch(const FAutoPaintTexturePatchDispatchParams& Params)
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


class FApplyLandscapeTextureWeightPatchPS : public FGlobalShader
{
	DECLARE_EXPORTED_GLOBAL_SHADER(FApplyLandscapeTextureWeightPatchPS, AUTOPAINTSHADERS_API);
	SHADER_USE_PARAMETER_STRUCT(FApplyLandscapeTextureWeightPatchPS, FGlobalShader);

public:

	enum class EBlendMode : uint8
	{
		/** Desired height is alpha blended with the current. */
		AlphaBlend,

		/** Desired height is multiplied by alpha and added to current (and clamped). */
		Additive,

		/** Like AlphaBlend, but patch is limited to only lowering the landscape. */
		Min,

		/** Like AlphaBlend, but patch is limited to only raising the landscape. */
		Max
	};

	// Flags that get packed into a bitfield because we're not allowed to use bool shader parameters:
	enum class EFlags : uint8
	{
		None = 0,

		// When false, falloff is circular.
		RectangularFalloff = 1 << 0,

		// When true, the texture alpha channel is considered for blending (in addition to falloff, if nonzero)
		ApplyPatchAlpha = 1 << 1,
	};

	// TODO: We could consider exposing an additional global alpha setting that we can use to pass in the given
	// edit layer alpha value... On the other hand, we currently don't bother doing this in any existing blueprint
	// brushes, and it would be hard to support in a way that doesn't require each blueprint brush to respect it
	// individually... Not clear whether this is something worth doing yet.

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceWeightmap)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InWeightPatch)
		SHADER_PARAMETER_SAMPLER(SamplerState, InWeightPatchSampler)
		SHADER_PARAMETER(FMatrix44f, InWeightmapToPatch)
		// Amount of the patch edge to not apply in UV space. Generally set to 0.5/Dimensions to avoid applying
		// the edge half-pixels.
		SHADER_PARAMETER(FVector2f, InEdgeUVDeadBorder)
		// In world units, the size of the margin across which the alpha falls from 1 to 0
		SHADER_PARAMETER(float, InFalloffWorldMargin)
		// Size of the patch in world units (used for falloff)
		SHADER_PARAMETER(FVector2f, InPatchWorldDimensions)
		SHADER_PARAMETER(uint32, InBlendMode)
		// Some combination of the flags (see constants above).
		SHADER_PARAMETER(uint32, InFlags)

		RENDER_TARGET_BINDING_SLOTS() // Holds our output
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment);

	static void AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters, const FIntRect& DestinationBounds);
};

bool FApplyLandscapeTextureWeightPatchPS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return UE::Landscape::DoesPlatformSupportEditLayers(Parameters.Platform);
}

void FApplyLandscapeTextureWeightPatchPS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("APPLY_WEIGHT_PATCH"), 1);

	// Make our flag choices match in the shader.
	OutEnvironment.SetDefine(TEXT("RECTANGULAR_FALLOFF_FLAG"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EFlags::RectangularFalloff));
	OutEnvironment.SetDefine(TEXT("APPLY_PATCH_ALPHA_FLAG"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EFlags::ApplyPatchAlpha));

	OutEnvironment.SetDefine(TEXT("ADDITIVE_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::Additive));
	OutEnvironment.SetDefine(TEXT("ALPHA_BLEND_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::AlphaBlend));
	OutEnvironment.SetDefine(TEXT("MIN_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::Min));
	OutEnvironment.SetDefine(TEXT("MAX_MODE"), static_cast<uint8>(FApplyLandscapeTextureHeightPatchPS::EBlendMode::Max));
}

void FApplyLandscapeTextureWeightPatchPS::AddToRenderGraph(FRDGBuilder& GraphBuilder, FParameters* InParameters, const FIntRect& DestinationBounds)
{
	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
	TShaderMapRef<FApplyLandscapeTextureWeightPatchPS> PixelShader(ShaderMap);

	FPixelShaderUtils::AddFullscreenPass(
		GraphBuilder,
		ShaderMap,
		RDG_EVENT_NAME("LandscapeTextureWeightPatch"),
		PixelShader,
		InParameters,
		DestinationBounds);
}

IMPLEMENT_GLOBAL_SHADER(FApplyLandscapeTextureWeightPatchPS, "/Plugin/AutoPaint/Private/AutoPaintTexturePatchPS.usf", "ApplyLandscapeTextureWeightPatch", SF_Pixel);

void FAutoPaintTexturePatchWeightmapGPUInterface::Dispatch_RenderThread(FRHICommandListImmediate& RHICmdList, const FAutoPaintTexturePatchDispatchParams& Params)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(LandscapeTextureHeightPatch_Render);

	FRDGBuilder GraphBuilder(RHICmdList, RDG_EVENT_NAME("ApplyTextureWeightPatch"));

	TRefCountPtr<IPooledRenderTarget> DestinationRenderTarget = CreateRenderTarget(Params.CombinedResult->GetResource()->GetTexture2DRHI(), TEXT("LandscapeTextureWeightPatchOutput"));
	FRDGTextureRef DestinationTexture = GraphBuilder.RegisterExternalTexture(DestinationRenderTarget);

	// Make a copy of our heightmap input so we can read and write at the same time (needed for blending)
	FRDGTextureRef InputCopy = GraphBuilder.CreateTexture(DestinationTexture->Desc, TEXT("LandscapeTextureWeightPatchInputCopy"));

	FRHICopyTextureInfo CopyTextureInfo;
	CopyTextureInfo.NumMips = 1;
	CopyTextureInfo.Size = FIntVector(DestinationTexture->Desc.GetSize().X, DestinationTexture->Desc.GetSize().Y, 0);
	AddCopyTexturePass(GraphBuilder, DestinationTexture, InputCopy, CopyTextureInfo);

	FApplyLandscapeTextureWeightPatchPS::FParameters* ShaderParams = GraphBuilder.AllocParameters<FApplyLandscapeTextureWeightPatchPS::FParameters>();

	ShaderParams->InWeightmapToPatch = Params.HeightmapToPatch;
	ShaderParams->InEdgeUVDeadBorder = Params.EdgeUVDeadBorder;
	ShaderParams->InFalloffWorldMargin = Params.FalloffWorldMargin;
	ShaderParams->InPatchWorldDimensions = Params.PatchWorldDimensions;

	// @todo:
	using EShaderBlendMode = FApplyLandscapeTextureWeightPatchPS::EBlendMode;
	EShaderBlendMode BlendMode = EShaderBlendMode::AlphaBlend;
	ShaderParams->InBlendMode = static_cast<uint32>(BlendMode);
	
	using EShaderFlags = FApplyLandscapeTextureWeightPatchPS::EFlags;
	EShaderFlags Flags = EShaderFlags::None;
	ShaderParams->InFlags = static_cast<uint8>(Flags);

	TRefCountPtr<IPooledRenderTarget> PatchRenderTarget = CreateRenderTarget(Params.PatchTexture->GetResource()->GetTexture2DRHI(), TEXT("LandscapeTextureWeightPatch"));
	FRDGTextureRef PatchTexture = GraphBuilder.RegisterExternalTexture(PatchRenderTarget);
	FRDGTextureSRVRef PatchSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(PatchTexture, 0));
	ShaderParams->InWeightPatch = PatchSRV;
	ShaderParams->InWeightPatchSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp>::GetRHI();

	FRDGTextureSRVRef InputCopySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(InputCopy, 0));
	ShaderParams->InSourceWeightmap = InputCopySRV;

	ShaderParams->RenderTargets[0] = FRenderTargetBinding(DestinationTexture, ERenderTargetLoadAction::ENoAction, /*InMipIndex = */0);

	FApplyLandscapeTextureWeightPatchPS::AddToRenderGraph(GraphBuilder, ShaderParams, Params.DestinationBounds);

	GraphBuilder.Execute();
}

void FAutoPaintTexturePatchWeightmapGPUInterface::Dispatch_GameThread(const FAutoPaintTexturePatchDispatchParams& Params)
{
	ENQUEUE_RENDER_COMMAND(LandscapeCircleHeightPatch)(
		[Params](FRHICommandListImmediate& RHICmdList)
		{
			Dispatch_RenderThread(RHICmdList, Params);
		});
}

void FAutoPaintTexturePatchWeightmapGPUInterface::Dispatch(const FAutoPaintTexturePatchDispatchParams& Params)
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