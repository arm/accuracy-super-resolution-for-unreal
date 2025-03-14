//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#pragma once
#include "ArmASR.h"

#include "Shaders/ArmASRComputeLuminancePyramid.h"
#include "Shaders/ArmASRConvertVelocity.h"
#include "Shaders/ArmASRCopyExposure.h"
#include "Shaders/ArmASRDepthClip.h"
#include "Shaders/ArmASRLock.h"
#include "Shaders/ArmASRReconstructPrevDepth.h"
#include "Shaders/ArmASRAccumulate.h"
#include "Shaders/ArmASRRCAS.h"
#include "Shaders/ArmASRShaderUtils.h"
#include "Shaders/ArmASRCreateReactiveMask.h"

#include "SceneViewExtension.h"
#include "PostProcess/TemporalAA.h"
#include "Interfaces/IPluginManager.h"
#include "UObject/ConstructorHelpers.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessVisualizeBuffer.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterial.h"
#include "TemporalUpscaler.h"
#include "Developer/Settings/Public/ISettingsModule.h"
#include "PostProcess/PostProcessDownsample.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "HAL/UnrealMemory.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessing.h"
#include "FXSystem.h"
#include "RenderGraphUtils.h"
#include "ArmASRFXSystem.h"
#include "ArmASRInfo.h"

extern TAutoConsoleVariable<int32> CVarArmASRCreateReactiveMask;
extern TAutoConsoleVariable<int32> CVarArmASREnable;

class FArmASRTemporalUpscaler final : public UE::Renderer::Private::ITemporalUpscaler
{
public:
	FArmASRTemporalUpscaler(FArmASRInfo& ArmASRInfo, FArmASRPassthroughDenoiser& Denoiser);
	~FArmASRTemporalUpscaler();
	virtual const TCHAR* GetDebugName() const;

	virtual FOutputs AddPasses(
		FRDGBuilder& GraphBuilder,
		const FSceneView& View,
		const FInputs& Inputs) const;

	virtual float GetMinUpsampleResolutionFraction() const;
	virtual float GetMaxUpsampleResolutionFraction() const;

	virtual ITemporalUpscaler* Fork_GameThread(const class FSceneViewFamily& ViewFamily) const;

private:
	FDynamicResolutionStateInfos DynamicResolutionStateInfos;
	FArmASRInfo& ArmASRInfo;
	FArmASRPassthroughDenoiser& Denoiser;
};