//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#include "ArmASRShaderParameters.h"

#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"

class FArmASRReconstructPrevDepthPS : public FGlobalShader
{
public:
	using FPermutationDomain = TShaderPermutationDomain<FArmASR_ApplyUltraPerfOpt>;

	DECLARE_GLOBAL_SHADER(FArmASRReconstructPrevDepthPS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRReconstructPrevDepthPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArmASRPassParameters, cbArmASR)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_motion_vectors)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_depth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_color_jittered)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_exposure)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_reconstructed_previous_nearest_depth)
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

// Function to setup Reconstruct Previous Depth Shader parameters. RpdShaderParameters will be updated.
inline void SetReconstructPrevDepthParameters(
	bool bIsUltraPerformance,
	FArmASRReconstructPrevDepthPS::FParameters* RpdShaderParameters,
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParameters,
	const FRDGTextureRef MotionVectorTexture,
	const FRDGTextureSRVRef DepthTexture,
	const FRDGTextureSRVRef SceneColorTexture,
	const FRDGTextureSRVRef AutoExposureTexture, // Generated from CLP shader or Unreal Engine
	const FIntPoint& InputExtents,
	const FScreenPassTextureViewport& Viewport,
	FRDGBuilder& GraphBuilder)
{
	FRDGTextureSRVDesc MotionVectorSRVDesc = FRDGTextureSRVDesc::Create(MotionVectorTexture);
	FRDGTextureSRVRef MotionVectorSRVTexture = GraphBuilder.CreateSRV(MotionVectorSRVDesc);
	RpdShaderParameters->r_input_motion_vectors = MotionVectorSRVTexture;
	// SRV's
	RpdShaderParameters->r_input_depth = DepthTexture;
	RpdShaderParameters->r_input_color_jittered = SceneColorTexture;

	// As auto exposure is enabled, use for r_input_exposure
	RpdShaderParameters->r_input_exposure = AutoExposureTexture;

	// UAV's
	FRDGTextureDesc NearestDepthDesc = FRDGTextureDesc::Create2D(InputExtents, PF_R32_UINT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, 1, 1);
	FRDGTextureRef NearestDepthTexture = GraphBuilder.CreateTexture(NearestDepthDesc, TEXT("ReconstructedPreviousNearestDepthTexture"));
	RpdShaderParameters->rw_reconstructed_previous_nearest_depth = GraphBuilder.CreateUAV(NearestDepthTexture);
	// Clear the reconstructed previous nearest depth texture as the shader doesn't always write to all elements
	AddClearRenderTargetPass(GraphBuilder, NearestDepthTexture);

	if (bIsUltraPerformance)
	{
		FRDGTextureDesc DilatedDepthVelocityLumaDesc = FRDGTextureDesc::Create2D(InputExtents, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		FRDGTextureRef DilatedDepthVelocityLumaTexture = GraphBuilder.CreateTexture(DilatedDepthVelocityLumaDesc, TEXT("DilatedDepthVelocityLumaTexture"));

		const FScreenPassRenderTarget DilatedDepthVelocityLumaRT(DilatedDepthVelocityLumaTexture, Viewport.Rect, ERenderTargetLoadAction::ENoAction);
		RpdShaderParameters->RenderTargets[0] = DilatedDepthVelocityLumaRT.GetRenderTargetBinding();
	}
	else
	{
		// Create textures for all RenderTargets
		FRDGTextureDesc DilatedDepthDesc = FRDGTextureDesc::Create2D(InputExtents, PF_R32_FLOAT, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		FRDGTextureRef DilatedDepthTexture = GraphBuilder.CreateTexture(DilatedDepthDesc, TEXT("DilatedDepthTexture"));

		FRDGTextureDesc DilatedVelocityDesc = FRDGTextureDesc::Create2D(InputExtents, PF_G16R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		FRDGTextureRef DilatedVelocityTexture = GraphBuilder.CreateTexture(DilatedVelocityDesc, TEXT("DilatedVelocityTexture"));

		FRDGTextureDesc LockLumaDesc = FRDGTextureDesc::Create2D(InputExtents, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		FRDGTextureRef LockLumaTexture = GraphBuilder.CreateTexture(LockLumaDesc, TEXT("LockLumaTexture"));

		// Create RenderTargets and assign to parameters.
		const FScreenPassRenderTarget DilatedDepthRT(DilatedDepthTexture, Viewport.Rect, ERenderTargetLoadAction::ENoAction);
		RpdShaderParameters->RenderTargets[0] = DilatedDepthRT.GetRenderTargetBinding();

		const FScreenPassRenderTarget DilatedVelocityRT(DilatedVelocityTexture, Viewport.Rect, ERenderTargetLoadAction::ENoAction);
		RpdShaderParameters->RenderTargets[1] = DilatedVelocityRT.GetRenderTargetBinding();

		const FScreenPassRenderTarget LockLumaRT(LockLumaTexture, Viewport.Rect, ERenderTargetLoadAction::ENoAction);
		RpdShaderParameters->RenderTargets[2] = LockLumaRT.GetRenderTargetBinding();
	}

	// Assign common parameters to constant buffer.
	RpdShaderParameters->cbArmASR = ArmASRPassParameters;
}
