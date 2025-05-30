//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
#include "GlobalShader.h"
#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "SceneTextureParameters.h"

class FArmASRCopyExposureCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FArmASRCopyExposureCS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRCopyExposureCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float4>, EyeAdaptationBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, ExposureTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FArmASRGlobalShader::ShouldCompilePermutation(Parameters);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
	}
};

// Function to setup Copy Exposure Shader parameters.
inline void SetCopyExposureParameters(
	FArmASRCopyExposureCS::FParameters* CopyExposureParameters,
	const FSceneView& View,
	FRDGBuilder& GraphBuilder)
{
	// Setup CopyExposure shader to get Exposure from the engine if flag is enabled, otherwise use Compute Luminance Exposure.
	FRDGTextureDesc ExposureDesc = FRDGTextureDesc::Create2D({ 1,1 }, PF_A32B32G32R32F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV);
	FRDGTextureRef ExposureTexture = GraphBuilder.CreateTexture(ExposureDesc, TEXT("ExposureTexture"));

	CopyExposureParameters->EyeAdaptationBuffer = GraphBuilder.CreateSRV(GetEyeAdaptationBuffer(GraphBuilder, View));
	CopyExposureParameters->ExposureTexture = GraphBuilder.CreateUAV(ExposureTexture);
}