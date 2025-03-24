//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
#include "ArmASRShaderParameters.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "ShaderCompilerCore.h"

// To generate shaders this is required in .cpp file.
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FArmASRPassParameters, "cbArmASR");
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FArmASRComputeLuminanceParameters, "cbArmASRSPD");
IMPLEMENT_UNIFORM_BUFFER_STRUCT(FArmASRRCASParameters, "cbArmASRRCAS");

bool FArmASRGlobalShader::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::ES3_1);
}

void FArmASRGlobalShader::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("FFXM_GPU"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_HLSL"), 1);
	OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);

	bool bIsOpenGL = IsOpenGLPlatform(Parameters.Platform);
	bool bIsVulkan = IsVulkanPlatform(Parameters.Platform);
	bool bIsDx11   = RHIGetInterfaceType() == ERHIInterfaceType::D3D11;
	bool bIsDx12   = RHIGetInterfaceType() == ERHIInterfaceType::D3D12;
	bool bIsHlslcc = FGenericDataDrivenShaderPlatformInfo::GetIsHlslcc(Parameters.Platform);
	bool bUsingDxc = FDataDrivenShaderPlatformInfo::GetSupportsDxc(Parameters.Platform);
	bool bUsingSM6 = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);

	if (bIsOpenGL)
	{
		OutEnvironment.SetDefine(TEXT("FFXM_SHADER_PLATFORM_GLES_3_2"), 1);
	}

	// Disable FP16 for OpenGL with HLSLCC or for DX11, as it is not supported.
	bool bDisableFp16 = (bIsOpenGL && bIsHlslcc) || bIsDx11;
	OutEnvironment.SetDefine(TEXT("FFXM_HALF"), bDisableFp16 ? 0 : 1);

	if (!bUsingDxc)
	{
		// Remove the unorm attribute when compiling to avoid an fxc error.
		OutEnvironment.SetDefine(TEXT("unorm"), TEXT(" "));
	}

	// If OpenGL without HLSLCC, DX12 or Vulkan enable CFLAG_AllowRealTypes to use explicit 16-bit types.
	if (bIsVulkan || bIsDx12 || (bIsOpenGL && !bIsHlslcc))
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes);
	}

	OutEnvironment.SetDefine(TEXT("FFXM_HLSL_6_2"), bUsingSM6);

	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_HDR_COLOR_INPUT"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_JITTERED_MOTION_VECTORS"), 0);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_INVERTED_DEPTH"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_ENABLE_AUTO_EXPOSURE"), 0);
}