//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
#include "ArmASRShaderParameters.h"

#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

class FArmASRDepthClipPS : public FGlobalShader
{
public:
	using FPermutationDomain = TShaderPermutationDomain<FArmASR_ApplyBalancedOpt, FArmASR_ApplyPerfOpt, FArmASR_ApplyUltraPerfOpt>;

	DECLARE_GLOBAL_SHADER(FArmASRDepthClipPS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRDepthClipPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArmASRPassParameters, cbArmASR)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_LinearClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_PointClamp)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_reconstructed_previous_nearest_depth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilated_motion_vectors)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilatedDepth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_reactive_mask)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_transparency_and_composition_mask)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_previous_dilated_motion_vectors)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_motion_vectors)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_color_jittered)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_depth)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_exposure)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilated_depth_motion_vectors_input_luma)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_prev_dilated_depth_motion_vectors_input_luma)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FArmASRGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Define common shader flags.
		FArmASRGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};


// Function to setup Depth Clip shader parameters. DcShaderParameters will be updated.
inline void SetDepthClipParameters(
	FArmASRDepthClipPS::FParameters* DcShaderParameters,
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParameters,
	const FRDGTextureSRVRef AutoExposureTexture,        // Generated UAV from CLP shader
	const FRDGTextureRef RecontructedPrevDepthTexture,  // Generated UAV from RPD shader
	const FRDGTextureRef DilatedDepthTexture,           // Generated RT from RPD shader
	const FRDGTextureRef DilatedMotionVectorTexture,    // Generated RT from RPD shader
	const FRDGTextureRef PrevDilatedMotionVectors,      // From history
	const FRDGTextureRef DilatedDepthMotionVectorsInputLumaTexture,
	const FRDGTextureRef PrevDilatedDepthMotionVectorsInputLumaTexture,
	const FRDGTextureRef MotionVectorTexture,
	const FRDGTextureRef ReactiveMaskTexture,
	const FRDGTextureRef CompositeMaskTexture,
	const FRDGTextureSRVRef DepthTexture,
	const FRDGTextureSRVRef SceneColorTexture,
	const EShaderQualityPreset qualityPreset,
	const FIntPoint& InputExtents,
	const FScreenPassTextureViewport& Viewport,
	FRDGBuilder& GraphBuilder)
{
	// Sampler states
	DcShaderParameters->s_LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();
	DcShaderParameters->s_PointClamp = TStaticSamplerState<SF_Point>::GetRHI();

	// SRV's
	FRDGTextureSRVDesc ReconstructPrevDepthSRVDesc = FRDGTextureSRVDesc::Create(RecontructedPrevDepthTexture);
	FRDGTextureSRVRef ReconstructPrevDepthSRVTexture = GraphBuilder.CreateSRV(ReconstructPrevDepthSRVDesc);
	DcShaderParameters->r_reconstructed_previous_nearest_depth = ReconstructPrevDepthSRVTexture;

	const bool bIsUltraPerformance = (qualityPreset == EShaderQualityPreset::ULTRA_PERFORMANCE);

	if (bIsUltraPerformance)
	{
		FRDGTextureSRVDesc DilatedDepthMotionVectorsInputLumaSRVDesc = FRDGTextureSRVDesc::Create(DilatedDepthMotionVectorsInputLumaTexture);
		FRDGTextureSRVRef DilatedDepthMotionVectorsInputLumaSRVTexture = GraphBuilder.CreateSRV(DilatedDepthMotionVectorsInputLumaSRVDesc);
		DcShaderParameters->r_dilated_depth_motion_vectors_input_luma = DilatedDepthMotionVectorsInputLumaSRVTexture;

		FRDGTextureSRVDesc PrevDilatedDepthMotionVectorsInputLumaSRVDesc = FRDGTextureSRVDesc::Create(PrevDilatedDepthMotionVectorsInputLumaTexture);
		FRDGTextureSRVRef PrevDilatedDepthMotionVectorsInputLumaSRVTexture = GraphBuilder.CreateSRV(PrevDilatedDepthMotionVectorsInputLumaSRVDesc);
		DcShaderParameters->r_prev_dilated_depth_motion_vectors_input_luma = PrevDilatedDepthMotionVectorsInputLumaSRVTexture;
	}
	else
	{
		FRDGTextureSRVDesc DilatedMotionVectorSRVDesc = FRDGTextureSRVDesc::Create(DilatedMotionVectorTexture);
		FRDGTextureSRVRef DilatedMotionVectorSRVTexture = GraphBuilder.CreateSRV(DilatedMotionVectorSRVDesc);
		DcShaderParameters->r_dilated_motion_vectors = DilatedMotionVectorSRVTexture;

		FRDGTextureSRVDesc DilatedDepthSRVDesc = FRDGTextureSRVDesc::Create(DilatedDepthTexture);
		FRDGTextureSRVRef DilatedDepthSRVTexture = GraphBuilder.CreateSRV(DilatedDepthSRVDesc);
		DcShaderParameters->r_dilatedDepth = DilatedDepthSRVTexture;

		FRDGTextureSRVDesc PrevDilatedMotionVectorsSRVDesc = FRDGTextureSRVDesc::Create(PrevDilatedMotionVectors);
		FRDGTextureSRVRef PrevDilatedMotionVectorsSRVTexture = GraphBuilder.CreateSRV(PrevDilatedMotionVectorsSRVDesc);
		DcShaderParameters->r_previous_dilated_motion_vectors = PrevDilatedMotionVectorsSRVTexture;
	}

	FRDGTextureSRVDesc ReactiveMaskSRVDesc = FRDGTextureSRVDesc::Create(ReactiveMaskTexture);
	FRDGTextureSRVRef ReactiveMaskSRVTexture = GraphBuilder.CreateSRV(ReactiveMaskSRVDesc);
	DcShaderParameters->r_reactive_mask = ReactiveMaskSRVTexture;

	FRDGTextureSRVDesc CompositeMaskSRVDesc = FRDGTextureSRVDesc::Create(CompositeMaskTexture);
	FRDGTextureSRVRef CompositeMaskSRVTexture = GraphBuilder.CreateSRV(CompositeMaskSRVDesc);
	DcShaderParameters->r_transparency_and_composition_mask = CompositeMaskSRVTexture;

	FRDGTextureSRVDesc MotionVectorSRVDesc = FRDGTextureSRVDesc::Create(MotionVectorTexture);
	FRDGTextureSRVRef MotionVectorSRVTexture = GraphBuilder.CreateSRV(MotionVectorSRVDesc);
	DcShaderParameters->r_input_motion_vectors = MotionVectorSRVTexture;

	DcShaderParameters->r_input_depth = DepthTexture;
	DcShaderParameters->r_input_color_jittered = SceneColorTexture;
	DcShaderParameters->r_input_exposure = AutoExposureTexture;

	// Create textures for all RenderTargets
	FRDGTextureDesc DilatedReactiveMaskDesc = FRDGTextureDesc::Create2D(InputExtents, PF_R8G8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
	FRDGTextureRef DilatedReactiveMaskTexture = GraphBuilder.CreateTexture(DilatedReactiveMaskDesc, TEXT("DilatedReactiveMaskTexture"));

	// Create RenderTargets and assign to parameters.
	const FScreenPassRenderTarget DilatedReactiveMaskRT(DilatedReactiveMaskTexture, Viewport.Rect, ERenderTargetLoadAction::ENoAction);
	DcShaderParameters->RenderTargets[0] = DilatedReactiveMaskRT.GetRenderTargetBinding();

	if (!bIsUltraPerformance)
	{
		FRDGTextureDesc PreparedInputColorDesc = FRDGTextureDesc::Create2D(InputExtents, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		FRDGTextureRef PreparedInputColorTexture = GraphBuilder.CreateTexture(PreparedInputColorDesc, TEXT("PreparedInputColorTexture"));
		const FScreenPassRenderTarget PreparedInputColorRT(PreparedInputColorTexture, Viewport.Rect, ERenderTargetLoadAction::ENoAction);
		DcShaderParameters->RenderTargets[1] = PreparedInputColorRT.GetRenderTargetBinding();
	}
	// Assign common parameters to constant buffer.
	DcShaderParameters->cbArmASR = ArmASRPassParameters;
}
