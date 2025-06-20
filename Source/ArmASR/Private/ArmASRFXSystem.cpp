//
// Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#include "ArmASRFXSystem.h"
#include "ArmASRTemporalUpscaler.h"

FFXSystemInterface* ArmASRFXSystem::GetInterface(const FName& InName)
{
	return InName == ArmASRFXSystem::FXName ? this : nullptr;
}

void ArmASRFXSystem::Tick(UWorld*, float DeltaSeconds) {}

#if WITH_EDITOR
void ArmASRFXSystem::Suspend() {}

void ArmASRFXSystem::Resume() {}
#endif // #if WITH_EDITOR

void ArmASRFXSystem::DrawDebug(FCanvas* Canvas) {}

void ArmASRFXSystem::AddVectorField(UVectorFieldComponent* VectorFieldComponent) {}

void ArmASRFXSystem::RemoveVectorField(UVectorFieldComponent* VectorFieldComponent) {}

void ArmASRFXSystem::UpdateVectorField(UVectorFieldComponent* VectorFieldComponent) {}

void ArmASRFXSystem::PreInitViews(FRDGBuilder&, bool, const TArrayView<const FSceneViewFamily*>&, const FSceneViewFamily*) {};
void ArmASRFXSystem::PostInitViews(FRDGBuilder&, TConstStridedView<FSceneView>, bool) {};

bool ArmASRFXSystem::UsesGlobalDistanceField() const { return false; }

bool ArmASRFXSystem::UsesDepthBuffer() const { return false; }

bool ArmASRFXSystem::RequiresEarlyViewUniformBuffer() const { return false; }

bool ArmASRFXSystem::RequiresRayTracingScene() const { return false; }

void ArmASRFXSystem::PreRender(FRDGBuilder&, TConstStridedView<FSceneView>, FSceneUniformBuffer&, bool) {};
void ArmASRFXSystem::PostRenderOpaque(FRDGBuilder& GraphBuilder, TConstStridedView<FSceneView> Views, FSceneUniformBuffer& SceneUniformBuffer, bool bAllowGPUParticleUpdate)
{
	if (CVarArmASRCreateReactiveMask.GetValueOnRenderThread() && (CVarArmASREnable.GetValueOnRenderThread()) && Views.Num() > 0)
	{
		FRHIUniformBuffer* ViewUniformBuffer = SceneUniformBuffer.GetBufferRHI(GraphBuilder);

		const FViewInfo& View = (FViewInfo&)(Views[0]);
		const FSceneTextures* SceneTextures = ((FViewFamilyInfo*)View.Family)->GetSceneTexturesChecked();

		FRDGTextureMSAA PreAlpha = SceneTextures->Color;
		auto const& Config = SceneTextures->Config;

		EPixelFormat SceneColorFormat = Config.ColorFormat;
		uint32 NumSamples = Config.NumSamples;

		FIntPoint SceneColorSize = FIntPoint::ZeroValue;

		SceneColorSize.X = FMath::Max(SceneColorSize.X, View.ViewRect.Max.X);
		SceneColorSize.Y = FMath::Max(SceneColorSize.Y, View.ViewRect.Max.Y);

		check(SceneColorSize.X > 0 && SceneColorSize.Y > 0);

		FIntPoint QuantizedSize;
		QuantizeSceneBufferSize(SceneColorSize, QuantizedSize);

		FRDGTextureDesc SceneColorPreAlphaCreateDesc = FRDGTextureDesc::Create2D(
			FIntPoint(QuantizedSize.X, QuantizedSize.Y), SceneColorFormat, FClearValueBinding::Black, ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource, 1, NumSamples);
		Info.SceneColorPreAlpha = GraphBuilder.CreateTexture(SceneColorPreAlphaCreateDesc, TEXT("ArmASRSceneColorPreAlphaTexture"), ERDGTextureFlags::MultiFrame);

		AddCopyTexturePass(GraphBuilder, PreAlpha.Target, Info.SceneColorPreAlpha, FIntPoint::ZeroValue, FIntPoint::ZeroValue, View.ViewRect.Size());
	}
}

FGPUSortManager* ArmASRFXSystem::GetGPUSortManager() const
{
	return GpuSortManager;
}

ArmASRFXSystem::ArmASRFXSystem(FArmASRInfo& Info, FGPUSortManager* GpuSortManager)
	: Info(Info), GpuSortManager(GpuSortManager)
{
}

ArmASRFXSystem::~ArmASRFXSystem() {}

FName const ArmASRFXSystem::FXName(TEXT("ArmASRFXSystem"));
