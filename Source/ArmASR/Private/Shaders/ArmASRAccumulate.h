// Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
#include "ArmASRShaderParameters.h"
#include "ArmASRShaderUtils.h"

#include "ArmASRInfo.h"
#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

class FArmASR_DoSharpening : SHADER_PERMUTATION_BOOL("FFXM_FSR2_OPTION_APPLY_SHARPENING");

class FArmASRAccumulatePS : public FGlobalShader
{
public:
	using FPermutationDomain = TShaderPermutationDomain<FArmASR_DoSharpening, FArmASR_ApplyBalancedOpt, FArmASR_ApplyPerfOpt, FArmASR_ApplyUltraPerfOpt>;

	DECLARE_GLOBAL_SHADER(FArmASRAccumulatePS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRAccumulatePS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArmASRPassParameters, cbArmASR)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_LinearClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_PointClamp)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_exposure)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilated_reactive_masks)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilated_motion_vectors)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilated_depth_motion_vectors_input_luma)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_motion_vectors)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_internal_upscaled_color)
	    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_color_jittered)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_lock_status)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_prepared_input_color)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_imgMips)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_auto_exposure)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_luma_history)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_internal_temporal_reactive)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_new_locks)
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

// Function to setup Accumulate shader parameters. AccumulateParameters will be updated.
inline void SetAccumulateParameters(
	FArmASRAccumulatePS::FParameters* AccumulateParameters,
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParameters,
	const FRDGTextureSRVRef AutoExposureTexture,        // Generated UAV from CLP shader or Unreal Engine
	const FRDGTextureRef ImgMipsTexture,                // Generated UAV from CLP shader
	const FRDGTextureRef DilatedMotionVectorTexture,    // Generated RT from RPD shader
	const FRDGTextureRef DilatedDepthMotionVectorsInputLumaTexture, // Generated RT from RPD shader
	const FRDGTextureRef DilatedReactiveMaskTexture,    // Generated RT from DC shader
	const FRDGTextureRef PreparedInputColor,            // Generated RT from DC shader
	const FRDGTextureSRVRef SceneColorTexture,
	const FRDGTextureRef PrevLockStatusTexture,         // From history
	const FRDGTextureRef OutputTexture,
	const FRDGTextureRef MotionVectorTexture,
	const FRDGTextureRef PrevUpscaledColourTexture,     // From history
	const FRDGTextureRef PrevLumaHistoryTexture,        // From history
	const FRDGTextureRef PrevTemporalReactiveTexture,   // From history
	const FRDGTextureRef LockMaskTexture,               // Generated UAV from L shader
	const float Sharpness,
	const EShaderQualityPreset QualityPreset,
	const FIntPoint& OutputExtents,
	const FIntRect& OutputRect,
	FRDGBuilder& GraphBuilder)
{
	// Sampler states
	AccumulateParameters->s_LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();
	AccumulateParameters->s_PointClamp = TStaticSamplerState<SF_Point>::GetRHI();

	// As auto exposure is enabled, use for r_input_exposure
	AccumulateParameters->r_input_exposure = AutoExposureTexture;

	FRDGTextureSRVDesc DilatedReactiveMaskSRVDesc = FRDGTextureSRVDesc::Create(DilatedReactiveMaskTexture);
	FRDGTextureSRVRef DilatedReactiveMaskSRVTexture = GraphBuilder.CreateSRV(DilatedReactiveMaskSRVDesc);
	AccumulateParameters->r_dilated_reactive_masks = DilatedReactiveMaskSRVTexture;

	const bool bIsUltraPerformance = (QualityPreset == EShaderQualityPreset::ULTRA_PERFORMANCE);
	if (bIsUltraPerformance)
	{
		FRDGTextureSRVDesc DilatedDepthMotionVectorsInputLumaSRVDesc = FRDGTextureSRVDesc::Create(DilatedDepthMotionVectorsInputLumaTexture);
		FRDGTextureSRVRef DilatedDepthMotionVectorsInputLumaSRVTexture = GraphBuilder.CreateSRV(DilatedDepthMotionVectorsInputLumaSRVDesc);
		AccumulateParameters->r_dilated_depth_motion_vectors_input_luma = DilatedDepthMotionVectorsInputLumaSRVTexture;

		// input color
		AccumulateParameters->r_input_color_jittered = SceneColorTexture;
	}
	else
	{
		FRDGTextureSRVDesc DilatedMotionVectorSRVDesc = FRDGTextureSRVDesc::Create(DilatedMotionVectorTexture);
		FRDGTextureSRVRef DilatedMotionVectorSRVTexture = GraphBuilder.CreateSRV(DilatedMotionVectorSRVDesc);
		AccumulateParameters->r_dilated_motion_vectors = DilatedMotionVectorSRVTexture;

		// Prepared colour is created in Depth Clip shader.
		FRDGTextureSRVDesc PreparedInputColorSRVDesc = FRDGTextureSRVDesc::Create(PreparedInputColor);
		FRDGTextureSRVRef PreparedInputSRVTexture = GraphBuilder.CreateSRV(PreparedInputColorSRVDesc);
		AccumulateParameters->r_prepared_input_color = PreparedInputSRVTexture;

		// r_luma_history
		FRDGTextureSRVDesc LumaHistorySRVDesc = FRDGTextureSRVDesc::Create(PrevLumaHistoryTexture);
		AccumulateParameters->r_luma_history = GraphBuilder.CreateSRV(LumaHistorySRVDesc);

		// r_internal_temporal_reactive (Used by Balanced)
		FRDGTextureSRVDesc TemporalReactiveHistorySRVDesc = FRDGTextureSRVDesc::Create(PrevTemporalReactiveTexture);
		AccumulateParameters->r_internal_temporal_reactive = GraphBuilder.CreateSRV(TemporalReactiveHistorySRVDesc);

		// r_imgMips
		FRDGTextureSRVDesc ImgMipsSRVDesc = FRDGTextureSRVDesc::Create(ImgMipsTexture);
		FRDGTextureSRVRef ImgMipsSRVTexture = GraphBuilder.CreateSRV(ImgMipsSRVDesc);
		AccumulateParameters->r_imgMips = GraphBuilder.CreateSRV(ImgMipsSRVDesc);
	}

	FRDGTextureSRVDesc MotionVectorSRVDesc = FRDGTextureSRVDesc::Create(MotionVectorTexture);
	FRDGTextureSRVRef MotionVectorSRVTexture = GraphBuilder.CreateSRV(MotionVectorSRVDesc);
	AccumulateParameters->r_input_motion_vectors = MotionVectorSRVTexture;

	// Upscaled colour from previous frame.
	FRDGTextureSRVDesc InternalUpscaledPrevSRVDesc = FRDGTextureSRVDesc::Create(PrevUpscaledColourTexture);
	FRDGTextureSRVRef InternalUpscaledPrevSRVTexture = GraphBuilder.CreateSRV(InternalUpscaledPrevSRVDesc);
	AccumulateParameters->r_internal_upscaled_color = InternalUpscaledPrevSRVTexture;

	// Lock status for previous frame.
	FRDGTextureSRVDesc LockStatusSRVDesc = FRDGTextureSRVDesc::Create(PrevLockStatusTexture);
	FRDGTextureSRVRef LockStatusSRVTexture = GraphBuilder.CreateSRV(LockStatusSRVDesc);
	AccumulateParameters->r_lock_status = LockStatusSRVTexture;

	// r_auto_exposure
	AccumulateParameters->r_auto_exposure = AutoExposureTexture;

	// r_luma_history
	FRDGTextureSRVDesc LumaHistorySRVDesc = FRDGTextureSRVDesc::Create(PrevLumaHistoryTexture);
	AccumulateParameters->r_luma_history = GraphBuilder.CreateSRV(LumaHistorySRVDesc);

	// r_internal_temporal_reactive (Used by Balanced/Performance)
	FRDGTextureSRVDesc TemporalReactiveHistorySRVDesc = FRDGTextureSRVDesc::Create(PrevTemporalReactiveTexture);
	AccumulateParameters->r_internal_temporal_reactive = GraphBuilder.CreateSRV(TemporalReactiveHistorySRVDesc);

	// Lock mask for current frame from Lock shader. This gets cleared for the next frame.
	FRDGTextureSRVDesc LockMaskSRVDesc = FRDGTextureSRVDesc::Create(LockMaskTexture);
	AccumulateParameters->r_new_locks = GraphBuilder.CreateSRV(LockMaskSRVDesc);

	const bool bIsBalancedOrPerformance = (QualityPreset == EShaderQualityPreset::BALANCED) || (QualityPreset == EShaderQualityPreset::PERFORMANCE);
	const EPixelFormat InternalUpscaledFormat = (bIsUltraPerformance || bIsBalancedOrPerformance) ? PF_FloatR11G11B10 : PF_FloatRGBA;

	// Create textures for all RenderTargets
	FRDGTextureDesc InternalUpscaledOutputColorDesc = FRDGTextureDesc::Create2D(OutputExtents, InternalUpscaledFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
	FRDGTextureRef InternalUpscaledColorOutputTexture = GraphBuilder.CreateTexture(InternalUpscaledOutputColorDesc, TEXT("InternalUpscaledColorOutputTexture"));

	FRDGTextureDesc LockStatusOutputDesc = FRDGTextureDesc::Create2D(OutputExtents, PF_G16R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
	FRDGTextureRef LockStatusOutputTexture = GraphBuilder.CreateTexture(LockStatusOutputDesc, TEXT("LockStatusOutputTexture"));

	// Create RenderTargets and assign to parameters.
	const FScreenPassRenderTarget InternalUpscaledColorRT(InternalUpscaledColorOutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);
	const FScreenPassRenderTarget LockStatusRT(LockStatusOutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);
	const FScreenPassRenderTarget UpscaledOutput(OutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);

	AccumulateParameters->RenderTargets[0] = InternalUpscaledColorRT.GetRenderTargetBinding();
	if (bIsUltraPerformance)
	{
		AccumulateParameters->RenderTargets[1] = LockStatusRT.GetRenderTargetBinding();
	}
	else if (!bIsBalancedOrPerformance)
	{
		AccumulateParameters->RenderTargets[1] = LockStatusRT.GetRenderTargetBinding();

		FRDGTextureDesc LumaHistoryOutputDesc = FRDGTextureDesc::Create2D(OutputExtents, PF_R8G8B8A8, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		FRDGTextureRef LumaHistoryOutputTexture = GraphBuilder.CreateTexture(LumaHistoryOutputDesc, TEXT("LumaHistoryOutputTexture"));
		const FScreenPassRenderTarget LumaHistoryRT(LumaHistoryOutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);
		AccumulateParameters->RenderTargets[2] = LumaHistoryRT.GetRenderTargetBinding();
	}
	else
	{
		// UE5 doesn't expose R8_SNorm.
		const FRDGTextureDesc TemporalReactiveOutputDesc = FRDGTextureDesc::Create2D(OutputExtents, PF_R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_RenderTargetable, 1, 1);
		const FRDGTextureRef TemporalReactiveOutputTexture = GraphBuilder.CreateTexture(TemporalReactiveOutputDesc, TEXT("InternalReactiveOutput"));
		const FScreenPassRenderTarget TemporalReactiveRT(TemporalReactiveOutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);
		AccumulateParameters->RenderTargets[1] = TemporalReactiveRT.GetRenderTargetBinding();
		AccumulateParameters->RenderTargets[2] = LockStatusRT.GetRenderTargetBinding();
	}

	const bool bUseRCAS = (Sharpness > 0.0f);
	if (!bUseRCAS)
	{
		const size_t index = bIsUltraPerformance ? 2 : 3;
		AccumulateParameters->RenderTargets[index] = UpscaledOutput.GetRenderTargetBinding();
	}

	// Assign common parameters to constant buffer.
	AccumulateParameters->cbArmASR = ArmASRPassParameters;
}
