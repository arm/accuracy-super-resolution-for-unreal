//
// Copyright © 2022-2023 Advanced Micro Devices, Inc.
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
#pragma once

#include "GlobalShader.h"

class FArmASR_ApplyBalancedOpt : SHADER_PERMUTATION_BOOL("FFXM_FSR2_OPTION_SHADER_OPT_BALANCED");
class FArmASR_ApplyPerfOpt : SHADER_PERMUTATION_BOOL("FFXM_FSR2_OPTION_SHADER_OPT_PERFORMANCE");

class FArmASRGlobalShader : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

// Common uniform buffer structure used throughout shaders.
BEGIN_UNIFORM_BUFFER_STRUCT(FArmASRPassParameters, )
	SHADER_PARAMETER(FIntPoint, iRenderSize)
	SHADER_PARAMETER(FIntPoint, iMaxRenderSize)
	SHADER_PARAMETER(FIntPoint, iDisplaySize)
	SHADER_PARAMETER(FIntPoint, iInputColorResourceDimensions)
	SHADER_PARAMETER(FIntPoint, iLumaMipDimensions)
	SHADER_PARAMETER(int, iLumaMipLevelToUse)
	SHADER_PARAMETER(int, iFrameIndex)
	SHADER_PARAMETER(FVector4f, fDeviceToViewDepth)
	SHADER_PARAMETER(FVector2f, fJitter)
	SHADER_PARAMETER(FVector2f, fMotionVectorScale)
	SHADER_PARAMETER(FVector2f, fDownscaleFactor)
	SHADER_PARAMETER(FVector2f, fMotionVectorJitterCancellation)
	SHADER_PARAMETER(float, fPreExposure)
	SHADER_PARAMETER(float, fPreviousFramePreExposure)
	SHADER_PARAMETER(float, fTanHalfFOV)
	SHADER_PARAMETER(float, fJitterSequenceLength)
	SHADER_PARAMETER(float, fDeltaTime)
	SHADER_PARAMETER(float, fDynamicResChangeFactor)
	SHADER_PARAMETER(float, fViewSpaceToMetersFactor)
END_UNIFORM_BUFFER_STRUCT()

// Parameters for the compute luminance pyramid shader.
BEGIN_UNIFORM_BUFFER_STRUCT(FArmASRComputeLuminanceParameters, )
SHADER_PARAMETER(uint32, mips)
SHADER_PARAMETER(uint32, numWorkGroups)
SHADER_PARAMETER(FUintVector2, workGroupOffset)
SHADER_PARAMETER(FUintVector2, renderSize)
END_UNIFORM_BUFFER_STRUCT()

// Parameters for the RCAS shader.
BEGIN_UNIFORM_BUFFER_STRUCT(FArmASRRCASParameters, )
SHADER_PARAMETER(FUintVector4, rcasConfig)
END_UNIFORM_BUFFER_STRUCT()