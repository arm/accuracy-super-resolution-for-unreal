//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#pragma once

#include "PostProcess/PostProcessing.h"
#include "RenderGraphUtils.h"
#include "ArmASRPassthroughDenoiser.h"

// Struct to hold constant resources that are used every frame
struct FArmASRResource
{
	TRefCountPtr<IPooledRenderTarget> RenderTarget;
	FTextureRHIRef Texture;
};

struct FArmASRInfo
{
	FPostProcessingInputs PostInputs;
	FRDGTextureRef SceneColorPreAlpha = nullptr;
	TRefCountPtr<IPooledRenderTarget> LumenReflections;
	FRDGTextureRef ReflectionTexture = nullptr;

	// Defaultly not set
	TOptional<FArmASRResource> Atomic = TOptional<FArmASRResource>();
};

// Free up per frame information at the end of the frame.
inline void CleanUpArmASRInfoFrameInfo(FArmASRInfo& Info)
{
	Info.SceneColorPreAlpha = nullptr;
	Info.LumenReflections.SafeRelease();
	Info.PostInputs.SceneTextures = nullptr;
	Info.ReflectionTexture = nullptr;
}

// Clean up all information. 
// ArmASR has some static data which we only upload to the GPU once and it is reused every frame.
// If ArmASR has been disabled then we should clean this up.
inline void CleanUpArmASRInfoAll(FArmASRInfo& Info)
{
	CleanUpArmASRInfoFrameInfo(Info);
	if (Info.Atomic)
	{
		Info.Atomic.GetValue().Texture.SafeRelease();
		Info.Atomic.GetValue().RenderTarget.SafeRelease();
		Info.Atomic.Reset();
	}
}