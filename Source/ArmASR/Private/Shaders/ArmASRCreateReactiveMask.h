//
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
#include "../ArmASRTemporalUpscaler.h"
#include "../ArmASRInfo.h"

#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "DataDrivenShaderPlatformInfo.h"

extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskReflectionScale;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskRoughnessScale;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskRoughnessBias;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskRoughnessMaxDistance;
extern TAutoConsoleVariable<int32> CVarArmASRReactiveMaskRoughnessForceMaxDistance;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskReflectionLumaBias;
extern TAutoConsoleVariable<float> CVarArmASRReactiveHistoryTranslucencyBias;
extern TAutoConsoleVariable<float> CVarArmASRReactiveHistoryTranslucencyLumaBias;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskTranslucencyBias;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskTranslucencyLumaBias;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskTranslucencyMaxDistance;
extern TAutoConsoleVariable<float> CVarArmASRReactiveMaskForceReactiveMaterialValue;
extern TAutoConsoleVariable<int32> CVarArmASRReactiveMaskReactiveShadingModelID;

// Shader to create the reactive mask. Modified from FSR2's Unreal integration to use a pixel shader
class FArmASRCreateReactiveMaskPS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FArmASRCreateReactiveMaskPS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRCreateReactiveMaskPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_TEXTURE_ACCESS(DepthTexture, ERHIAccess::SRVGraphics)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, GBufferB)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, GBufferD)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, ReflectionTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputDepth)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneColor)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, SceneColorPreAlpha)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, LumenSpecular)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InputVelocity)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
		SHADER_PARAMETER(float, FurthestReflectionCaptureDistance)
		SHADER_PARAMETER(float, ReactiveMaskReflectionScale)
		SHADER_PARAMETER(float, ReactiveMaskRoughnessScale)
		SHADER_PARAMETER(float, ReactiveMaskRoughnessBias)
		SHADER_PARAMETER(float, ReactiveMaskReflectionLumaBias)
		SHADER_PARAMETER(float, ReactiveHistoryTranslucencyBias)
		SHADER_PARAMETER(float, ReactiveHistoryTranslucencyLumaBias)
		SHADER_PARAMETER(float, ReactiveMaskTranslucencyBias)
		SHADER_PARAMETER(float, ReactiveMaskTranslucencyLumaBias)
		SHADER_PARAMETER(float, ReactiveMaskTranslucencyMaxDistance)
		SHADER_PARAMETER(float, ForceLitReactiveValue)
		SHADER_PARAMETER(uint32, ReactiveShadingModelID)
		SHADER_PARAMETER(uint32, LumenSpecularCurrentFrame)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return FArmASRGlobalShader::ShouldCompilePermutation(Parameters);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FArmASRGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};


inline bool IsUsingLumenReflections(const FViewInfo& View)
{
	const FSceneViewState* ViewState = View.ViewState;
	if (ViewState && View.Family->Views.Num() == 1)
	{
		static const auto CVarLumenEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Supported"));
		static const auto CVarLumenReflectionsEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Lumen.Reflections.Allow"));
		return FDataDrivenShaderPlatformInfo::GetSupportsLumenGI(View.GetShaderPlatform())
			&& !IsForwardShadingEnabled(View.GetShaderPlatform())
			&& !View.bIsPlanarReflection
			&& !View.bIsSceneCapture
			&& !View.bIsReflectionCapture
			&& View.State
			&& View.FinalPostProcessSettings.ReflectionMethod == EReflectionMethod::Lumen
			&& View.Family->EngineShowFlags.LumenReflections
			&& CVarLumenEnabled
			&& CVarLumenEnabled->GetInt()
			&& CVarLumenReflectionsEnabled
			&& CVarLumenReflectionsEnabled->GetInt();
	}

	return false;
};

inline void SetReactiveMaskParameters(FRDGBuilder& GraphBuilder, FArmASRCreateReactiveMaskPS::FParameters* PassParameters, FArmASRInfo& ArmASRInfo,
	const FIntPoint& InputExtents,
	const FIntRect& InputRect,
	const FRDGTextureRef ReactiveMaskTexture,
	const FRDGTextureRef CompositeMaskTexture,
	const FRDGTextureRef SceneDepth,
	const FRDGTextureRef SceneColor,
	const FRDGTextureRef VelocityTexture,
	bool ValidHistory,
	const FSceneView& View)
{
	FViewInfo& ViewInfo = (FViewInfo&)(View);
	PassParameters->Sampler = TStaticSamplerState<SF_Point>::GetRHI();

	const FScreenPassRenderTarget ReactiveMaskRT(ReactiveMaskTexture, InputRect, ERenderTargetLoadAction::ENoAction);
	PassParameters->RenderTargets[0] = ReactiveMaskRT.GetRenderTargetBinding();

	const FScreenPassRenderTarget CompositeMaskRT(CompositeMaskTexture, InputRect, ERenderTargetLoadAction::ENoAction);
	PassParameters->RenderTargets[1] = CompositeMaskRT.GetRenderTargetBinding();
	if (ArmASRInfo.PostInputs.SceneTextures)
	{
		FRDGTextureRef GBufferB = (*ArmASRInfo.PostInputs.SceneTextures)->GBufferBTexture;
		if (!GBufferB)
		{
			GBufferB = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		}

		FRDGTextureRef GBufferD = (*ArmASRInfo.PostInputs.SceneTextures)->GBufferDTexture;
		if (!GBufferD)
		{
			GBufferD = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		}

		FRDGTextureRef Reflections = ArmASRInfo.ReflectionTexture;
		if (!Reflections)
		{
			Reflections = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		}

		PassParameters->DepthTexture = SceneDepth;
		FRDGTextureSRVDesc DepthDesc = FRDGTextureSRVDesc::Create(SceneDepth);
		PassParameters->InputDepth = GraphBuilder.CreateSRV(DepthDesc);

		FRDGTextureSRVDesc SceneColorDesc = FRDGTextureSRVDesc::Create(SceneColor);
		FRDGTextureSRVDesc SceneColorSRV = FRDGTextureSRVDesc::Create(SceneColor);
		PassParameters->SceneColor = GraphBuilder.CreateSRV(SceneColorSRV);

		EPixelFormat SceneColorFormat = SceneColorDesc.Format;

		if (ArmASRInfo.SceneColorPreAlpha)
		{
			PassParameters->SceneColorPreAlpha = GraphBuilder.CreateSRV(ArmASRInfo.SceneColorPreAlpha);
		}
		else
		{
			PassParameters->SceneColorPreAlpha = GraphBuilder.CreateSRV(SceneColorSRV);
		}

		FRDGTextureSRVDesc VelocityDesc = FRDGTextureSRVDesc::Create(VelocityTexture);
		PassParameters->InputVelocity = GraphBuilder.CreateSRV(VelocityDesc);

		FRDGTextureRef LumenSpecular;
		FRDGTextureRef CurrentLumenSpecular = nullptr;

		if ((CurrentLumenSpecular || ArmASRInfo.LumenReflections.IsValid()) && ValidHistory && IsUsingLumenReflections(ViewInfo))
		{
			LumenSpecular = CurrentLumenSpecular ? CurrentLumenSpecular : GraphBuilder.RegisterExternalTexture(ArmASRInfo.LumenReflections);
		}
		else
		{
			LumenSpecular = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		}

		FRDGTextureSRVDesc LumenSpecularDesc = FRDGTextureSRVDesc::Create(LumenSpecular);
		PassParameters->LumenSpecular = GraphBuilder.CreateSRV(LumenSpecularDesc);
		PassParameters->LumenSpecularCurrentFrame = (CurrentLumenSpecular && LumenSpecular == CurrentLumenSpecular);

		FRDGTextureSRVDesc GBufferBDesc = FRDGTextureSRVDesc::Create(GBufferB);
		FRDGTextureSRVDesc GBufferDDesc = FRDGTextureSRVDesc::Create(GBufferD);
		FRDGTextureSRVDesc ReflectionsDesc = FRDGTextureSRVDesc::Create(Reflections);
		FRDGTextureUAVDesc ReactiveDesc(ReactiveMaskTexture);
		FRDGTextureUAVDesc CompositeDesc(CompositeMaskTexture);

		PassParameters->GBufferB = GraphBuilder.CreateSRV(GBufferBDesc);
		PassParameters->GBufferD = GraphBuilder.CreateSRV(GBufferDDesc);
		PassParameters->ReflectionTexture = GraphBuilder.CreateSRV(ReflectionsDesc);

		PassParameters->View = View.ViewUniformBuffer;

		PassParameters->FurthestReflectionCaptureDistance = CVarArmASRReactiveMaskRoughnessForceMaxDistance.GetValueOnRenderThread() ? CVarArmASRReactiveMaskRoughnessMaxDistance.GetValueOnRenderThread() : CVarArmASRReactiveMaskRoughnessMaxDistance.GetValueOnRenderThread();
		PassParameters->ReactiveMaskReflectionScale = CVarArmASRReactiveMaskReflectionScale.GetValueOnRenderThread();
		PassParameters->ReactiveMaskRoughnessScale = CVarArmASRReactiveMaskRoughnessScale.GetValueOnRenderThread();
		PassParameters->ReactiveMaskRoughnessBias = CVarArmASRReactiveMaskRoughnessBias.GetValueOnRenderThread();
		PassParameters->ReactiveMaskReflectionLumaBias = CVarArmASRReactiveMaskReflectionLumaBias.GetValueOnRenderThread();
		PassParameters->ReactiveHistoryTranslucencyBias = CVarArmASRReactiveHistoryTranslucencyBias.GetValueOnRenderThread();
		PassParameters->ReactiveHistoryTranslucencyLumaBias = CVarArmASRReactiveHistoryTranslucencyLumaBias.GetValueOnRenderThread();
		PassParameters->ReactiveMaskTranslucencyBias = CVarArmASRReactiveMaskTranslucencyBias.GetValueOnRenderThread();
		PassParameters->ReactiveMaskTranslucencyLumaBias = CVarArmASRReactiveMaskTranslucencyLumaBias.GetValueOnRenderThread();

		PassParameters->ReactiveMaskTranslucencyMaxDistance = CVarArmASRReactiveMaskTranslucencyMaxDistance.GetValueOnRenderThread();
		PassParameters->ForceLitReactiveValue = CVarArmASRReactiveMaskForceReactiveMaterialValue.GetValueOnRenderThread();
		PassParameters->ReactiveShadingModelID = (uint32)CVarArmASRReactiveMaskReactiveShadingModelID.GetValueOnRenderThread();
	}
}