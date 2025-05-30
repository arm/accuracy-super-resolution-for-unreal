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

	if (IsOpenGLPlatform(Parameters.Platform))
	{
		OutEnvironment.SetDefine(TEXT("FFXM_SHADER_PLATFORM_GLES_3_2"), 1);
	}

	bool bUsingDxc = FDataDrivenShaderPlatformInfo::GetSupportsDxc(Parameters.Platform);
	OutEnvironment.SetDefine(TEXT("FFXM_HALF"), bUsingDxc); // FXC does not support any 16-bit types

	if (!bUsingDxc)
	{
		// Rmove the unorm attribute when compiling to avoid an fxc error.
		OutEnvironment.SetDefine(TEXT("unorm"), TEXT(" "));
	}

	bool bUsingSM6 = IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM6);
	OutEnvironment.SetDefine(TEXT("FFXM_HLSL_6_2"), bUsingSM6);
	if (bUsingSM6)
	{
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowRealTypes); // Required in order to use explicit 16-bit types, and
																// is only supported on D3D SM 6.2+
	}

	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_HDR_COLOR_INPUT"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_LOW_RESOLUTION_MOTION_VECTORS"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_JITTERED_MOTION_VECTORS"), 0);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_OPTION_INVERTED_DEPTH"), 1);
	OutEnvironment.SetDefine(TEXT("FFXM_FSR2_ENABLE_AUTO_EXPOSURE"), 0);
}