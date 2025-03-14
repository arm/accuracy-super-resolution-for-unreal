//
// Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
#pragma once

#include "Runtime/Launch/Resources/Version.h"
#include "ScreenSpaceDenoise.h"

struct FArmASRInfo;

class FArmASRPassthroughDenoiser final : public IScreenSpaceDenoiser
{
public:
	FArmASRPassthroughDenoiser(FArmASRInfo& Info) 
		: WrappedDenoiser(nullptr)
		, ArmASRInfo(Info) {}

	/** Debug name of the denoiser for draw event. */
	const TCHAR* GetDebugName() const override;

	/** Returns the ray tracing configuration that should be done for denoiser. */
	EShadowRequirements GetShadowRequirements(
		const FViewInfo& View,
		const FLightSceneInfo& LightSceneInfo,
		const FShadowRayTracingConfig& RayTracingConfig) const override;

	/** Entry point to denoise the visibility mask of multiple shadows at the same time. */
	void DenoiseShadowVisibilityMasks(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const TStaticArray<FShadowVisibilityParameters, IScreenSpaceDenoiser::kMaxBatchSize>& InputParameters,
		const int32 InputParameterCount,
		TStaticArray<FShadowVisibilityOutputs, IScreenSpaceDenoiser::kMaxBatchSize>& Outputs) const override;

	/** Entry point to denoise polychromatic penumbra of multiple light. */
	FPolychromaticPenumbraOutputs DenoisePolychromaticPenumbraHarmonics(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FPolychromaticPenumbraHarmonics& Inputs) const override;

	/** Entry point to denoise reflections. */
	FReflectionsOutputs DenoiseReflections(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FReflectionsInputs& ReflectionInputs,
		const FReflectionsRayTracingConfig RayTracingConfig) const override;

	/** Entry point to denoise water reflections. */
	FReflectionsOutputs DenoiseWaterReflections(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FReflectionsInputs& ReflectionInputs,
		const FReflectionsRayTracingConfig RayTracingConfig) const override;

	/** Entry point to denoise reflections. */
	FAmbientOcclusionOutputs DenoiseAmbientOcclusion(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FAmbientOcclusionInputs& ReflectionInputs,
		const FAmbientOcclusionRayTracingConfig RayTracingConfig) const override;

	/** Entry point to denoise diffuse indirect and AO. */
	FSSDSignalTextures DenoiseDiffuseIndirect(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FDiffuseIndirectInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const override;

	/** Entry point to denoise SkyLight diffuse indirect. */
	FDiffuseIndirectOutputs DenoiseSkyLight(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FDiffuseIndirectInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const override;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 4
	/** Entry point to denoise reflected SkyLight diffuse indirect. */
	FDiffuseIndirectOutputs DenoiseReflectedSkyLight(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FDiffuseIndirectInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const override;
#endif
	/** Entry point to denoise spherical harmonic for diffuse indirect. */
	FSSDSignalTextures DenoiseDiffuseIndirectHarmonic(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FDiffuseIndirectHarmonic& Inputs,
		const HybridIndirectLighting::FCommonParameters& CommonDiffuseParameters) const override;

	/** Entry point to denoise SSGI. */
	bool SupportsScreenSpaceDiffuseIndirectDenoiser(EShaderPlatform Platform) const override;
	FSSDSignalTextures DenoiseScreenSpaceDiffuseIndirect(
		FRDGBuilder& GraphBuilder,
		const FViewInfo& View,
		FPreviousViewInfo* PreviousViewInfos,
		const FSceneTextureParameters& SceneTextures,
		const FDiffuseIndirectInputs& Inputs,
		const FAmbientOcclusionRayTracingConfig Config) const override;

	mutable const IScreenSpaceDenoiser* WrappedDenoiser;
	FArmASRInfo& ArmASRInfo;
};


inline void InitFArmASRDenoiser(FArmASRPassthroughDenoiser& Denoiser)
{
	// Wrap any existing denoiser API as we override this to be able to generate the reactive mask.
	// If the global screen space denoiser is already our denoiser don't do anything
	if (GScreenSpaceDenoiser == &Denoiser)
	{
		return;
	}
	Denoiser.WrappedDenoiser = GScreenSpaceDenoiser;
	if (!Denoiser.WrappedDenoiser)
	{
		Denoiser.WrappedDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
	}
	//Denoiser.WrappedDenoiser = IScreenSpaceDenoiser::GetDefaultDenoiser();
	check(Denoiser.WrappedDenoiser);
	GScreenSpaceDenoiser = &Denoiser;
}