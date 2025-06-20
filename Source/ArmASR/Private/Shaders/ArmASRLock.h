//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#include "ArmASRShaderParameters.h"

class FArmASRLockCS : public FGlobalShader
{
public:
	using FPermutationDomain = TShaderPermutationDomain<FArmASR_ApplyUltraPerfOpt>;

	DECLARE_GLOBAL_SHADER(FArmASRLockCS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRLockCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArmASRPassParameters, cbArmASR)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_LinearClamp)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_PointClamp)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_lock_input_luma)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_dilated_depth_motion_vectors_input_luma)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_new_locks)
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

// Function to setup Lock shader parameters
inline void SetLockParameters(
	bool bIsUltraPerformance,
	FArmASRLockCS::FParameters* LShaderParameters,
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParameters,
	const FRDGTextureRef LockLumaTexture,           // Generated RT from RPD shader
	const FRDGTextureRef DilatedDepthMotionVectorsInputLumaTexture,
	FRDGTextureRef OutLockMaskTexture,
	const FIntPoint& OutputExtents,
	FRDGBuilder& GraphBuilder)
{
	LShaderParameters->s_LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();
	LShaderParameters->s_PointClamp = TStaticSamplerState<SF_Point>::GetRHI();

	// SRVs
	if (bIsUltraPerformance)
	{
		FRDGTextureSRVDesc DilatedDepthMotionVectorsInputLumaSRVDesc =
			FRDGTextureSRVDesc::Create(DilatedDepthMotionVectorsInputLumaTexture);
		FRDGTextureSRVRef DilatedDepthMotionVectorsInputLumaSRVTexture =
			GraphBuilder.CreateSRV(DilatedDepthMotionVectorsInputLumaSRVDesc);
		LShaderParameters->r_dilated_depth_motion_vectors_input_luma = DilatedDepthMotionVectorsInputLumaSRVTexture;
	}
	else
	{
		FRDGTextureSRVDesc LockLumaSRVDesc = FRDGTextureSRVDesc::Create(LockLumaTexture);
		FRDGTextureSRVRef LockLumaSRVTexture = GraphBuilder.CreateSRV(LockLumaSRVDesc);
		LShaderParameters->r_lock_input_luma = LockLumaSRVTexture;
	}

	// UAVs
	FRDGTextureUAVDesc LockMaskUAVDesc(OutLockMaskTexture);
	LShaderParameters->rw_new_locks = GraphBuilder.CreateUAV(LockMaskUAVDesc);

	// Assign common parameters to constant buffer
	LShaderParameters->cbArmASR = ArmASRPassParameters;
}
