//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#include "ArmASRShaderParameters.h"
#include "ArmASRShaderUtils.h"

#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "SystemTextures.h"

class FArmASRRCASPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FArmASRRCASPS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRRCASPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArmASRPassParameters, cbArmASR)
		SHADER_PARAMETER_STRUCT_REF(FArmASRRCASParameters, cbArmASRRCAS)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_exposure)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_rcas_input)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_upscaled_output)
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
inline void SetRCASParameters(
	FArmASRRCASPS::FParameters* RCASParameters,
	FArmASRRCASParameters* RCASConstantParameters,
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParameters,
	const FRDGTextureRef ExposureTexture,
	const FRDGTextureRef InputTexture,
	const FRDGTextureRef OutputTexture,
	const float Sharpness,
	const FIntRect& OutputRect,
	FRDGBuilder& GraphBuilder)
{
	FUintVector4 RcasConfig = { 0, 0, 0, 0 };
	float SharpenessRemapped = (-2.0f * Sharpness) + 2.0f;

	// Transform from stops to linear value.
	SharpenessRemapped = exp2(-SharpenessRemapped);
	memcpy(&RcasConfig[0], &SharpenessRemapped, sizeof(float));
	RcasConfig[1] = FFloat16(SharpenessRemapped).Encoded + (FFloat16(SharpenessRemapped).Encoded << 16);
	RcasConfig[2] = 0;
	RcasConfig[3] = 0;

	RCASConstantParameters->rcasConfig = FUintVector4(RcasConfig[0], RcasConfig[1], RcasConfig[2], RcasConfig[3]);
	RCASParameters->cbArmASR = ArmASRPassParameters;
	RCASParameters->cbArmASRRCAS = TUniformBufferRef<FArmASRRCASParameters>::CreateUniformBufferImmediate(*RCASConstantParameters, UniformBuffer_SingleDraw);

	FRDGTextureSRVDesc ExposureSRVDesc = FRDGTextureSRVDesc::Create(ExposureTexture);
	RCASParameters->r_input_exposure = GraphBuilder.CreateSRV(ExposureSRVDesc);

	FRDGTextureSRVDesc RcasInputSRVDesc = FRDGTextureSRVDesc::Create(InputTexture);
	RCASParameters->r_rcas_input = GraphBuilder.CreateSRV(RcasInputSRVDesc);

	// RenderTarget
	const FScreenPassRenderTarget UpscaledOutput(OutputTexture, OutputRect, ERenderTargetLoadAction::ENoAction);
	RCASParameters->RenderTargets[0] = UpscaledOutput.GetRenderTargetBinding();
}
