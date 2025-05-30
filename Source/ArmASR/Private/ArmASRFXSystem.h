//
// Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//
#pragma once

#include "FXSystem.h"
#include "ArmASRTemporalUpscaler.h"
#include "ArmASRInfo.h"

class ArmASRFXSystem : public FFXSystemInterface
{
private:
	FArmASRInfo& Info;
	FGPUSortManager* GpuSortManager;
public:
	static const FName FXName;

	FFXSystemInterface* GetInterface(const FName& InName) final;
	void Tick(UWorld*, float DeltaSeconds) final;

#if WITH_EDITOR
	void Suspend() final;

	void Resume() final;
#endif // #if WITH_EDITOR

	void DrawDebug(FCanvas* Canvas) final;

	void AddVectorField(UVectorFieldComponent* VectorFieldComponent) final;

	void RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) final;

	void UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) final;

	void PreInitViews(FRDGBuilder&, bool, const TArrayView<const FSceneViewFamily*>&, const FSceneViewFamily*) final;
	void PostInitViews(FRDGBuilder&, TConstStridedView<FSceneView>, bool) final;


	bool UsesGlobalDistanceField() const final;

	bool UsesDepthBuffer() const final;

	bool RequiresEarlyViewUniformBuffer() const final;

	bool RequiresRayTracingScene() const final;

	void PreRender(FRDGBuilder&, TConstStridedView<FSceneView>, FSceneUniformBuffer&, bool) final;
	void PostRenderOpaque(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, FSceneUniformBuffer& SceneUniformBuffer, bool bAllowGPUParticleUpdate) final;

	FGPUSortManager* GetGPUSortManager() const;

	ArmASRFXSystem(FArmASRInfo& Info, FGPUSortManager* GpuSortManager);
	~ArmASRFXSystem();
};

