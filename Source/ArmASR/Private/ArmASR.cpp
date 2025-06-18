//
// Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All rights reserved.
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#include "ArmASR.h"
#include "ArmASRTemporalUpscaler.h"
#include "TemporalUpscaler.h"
#include "ArmASRPassthroughDenoiser.h"
#include "ArmASRSettings.h"

#define ARM_ASR_ENABLE_VK 1

#if ARM_ASR_ENABLE_VK
#include "IVulkanDynamicRHI.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "FArmASRModule"
DEFINE_LOG_CATEGORY_STATIC(LogArmASR, Log, All);

// Needs to be same pointer value used for both places where this is used
const TCHAR* ArmASRUpscalerName = TEXT("Arm ASR");

TAutoConsoleVariable<int32> CVarArmASREnable(
	TEXT("r.ArmASR.Enable"),
	1,
	TEXT("Turn on Arm ASR."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarArmASRAutoExposure(
	TEXT("r.ArmASR.AutoExposure"),
	0,
	TEXT("True to use Arm ASR's own auto-exposure, otherwise the engine's auto-exposure value is used."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<float> CVarArmASRSharpness(
	TEXT("r.ArmASR.Sharpness"),
	0.0f,
	TEXT("Range from 0.0 to 1.0, when greater than 0 this enables Robust Contrast Adaptive Sharpening Filter to sharpen the output image. Default is 0."),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarArmASRShaderQuality(
	TEXT("r.ArmASR.ShaderQuality"),
	1,
	TEXT("Select shader quality preset. 1: Quality / 2: Balanced / 3: Performance / 4: Ultra Performance"),
	ECVF_RenderThreadSafe);

TAutoConsoleVariable<int32> CVarArmASRCreateReactiveMask(
	TEXT("r.ArmASR.CreateReactiveMask"),
	1,
	TEXT("Create the reactive mask. Default is 1"),
	ECVF_RenderThreadSafe);

// CVars for Reactive Mask are the same as the ones from FSR2.
TAutoConsoleVariable<float> CVarArmASRReactiveMaskReflectionScale(
	TEXT("r.ArmASR.ReactiveMaskReflectionScale"),
	0.4f,
	TEXT("Range from 0.0 to 1.0 (Default 0.4), scales the Unreal engine reflection contribution to the reactive mask, which can be used to control the amount of aliasing on reflective surfaces."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskRoughnessScale(
	TEXT("r.ArmASR.ReactiveMaskRoughnessScale"),
	0.15f,
	TEXT("Range from 0.0 to 1.0 (Default 0.15), scales the GBuffer roughness to provide a fallback value for the reactive mask when screenspace & planar reflections are disabled or don't affect a pixel."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskRoughnessBias(
	TEXT("r.ArmASR.ReactiveMaskRoughnessBias"),
	0.25f,
	TEXT("Range from 0.0 to 1.0 (Default 0.25), biases the reactive mask value when screenspace/planar reflections are weak with the GBuffer roughness to account for reflection environment captures."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskRoughnessMaxDistance(
	TEXT("r.ArmASR.ReactiveMaskRoughnessMaxDistance"),
	6000.0f,
	TEXT("Maximum distance in world units for using material roughness to contribute to the reactive mask, the maximum of this value and View.FurthestReflectionCaptureDistance will be used. Default is 6000.0."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarArmASRReactiveMaskRoughnessForceMaxDistance(
	TEXT("r.ArmASR.ReactiveMaskRoughnessForceMaxDistance"),
	0,
	TEXT("Enable to force the maximum distance in world units for using material roughness to contribute to the reactive mask rather than using View.FurthestReflectionCaptureDistance. Defaults to 0."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskReflectionLumaBias(
	TEXT("r.ArmASR.ReactiveMaskReflectionLumaBias"),
	0.0f,
	TEXT("Range from 0.0 to 1.0 (Default: 0.0), biases the reactive mask by the luminance of the reflection. Use to balance aliasing against ghosting on brightly lit reflective surfaces."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveHistoryTranslucencyBias(
	TEXT("r.ArmASR.ReactiveHistoryTranslucencyBias"),
	0.5f,
	TEXT("Range from 0.0 to 1.0 (Default: 1.0), scales how much translucency suppresses history via the reactive mask. Higher values will make translucent materials more reactive which can reduce smearing."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveHistoryTranslucencyLumaBias(
	TEXT("r.ArmASR.ReactiveHistoryTranslucencyLumaBias"),
	0.0f,
	TEXT("Range from 0.0 to 1.0 (Default 0.0), biases how much the translucency suppresses history via the reactive mask by the luminance of the transparency. Higher values will make bright translucent materials more reactive which can reduce smearing."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskTranslucencyBias(
	TEXT("r.ArmASR.ReactiveMaskTranslucencyBias"),
	1.0f,
	TEXT("Range from 0.0 to 1.0 (Default: 1.0), scales how much contribution translucency makes to the reactive mask. Higher values will make translucent materials more reactive which can reduce smearing."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskTranslucencyLumaBias(
	TEXT("r.ArmASR.ReactiveMaskTranslucencyLumaBias"),
	0.0f,
	TEXT("Range from 0.0 to 1.0 (Default 0.0), biases the translucency contribution to the reactive mask by the luminance of the transparency. Higher values will make bright translucent materials more reactive which can reduce smearing."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskTranslucencyMaxDistance(
	TEXT("r.ArmASR.ReactiveMaskTranslucencyMaxDistance"),
	500000.0f,
	TEXT("Maximum distance in world units for using translucency to contribute to the reactive mask. This is a way to remove sky-boxes and other back-planes from the reactive mask, at the expense of nearer translucency not being reactive. Default is 500000.0."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<float> CVarArmASRReactiveMaskForceReactiveMaterialValue(
	TEXT("r.ArmASR.ReactiveMaskForceReactiveMaterialValue"),
	0.0f,
	TEXT("Force the reactive mask value for Reactive Shading Model materials, when > 0 this value can be used to override the value supplied in the Material Graph. Default is 0 (Off)."),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarArmASRReactiveMaskReactiveShadingModelID(
	TEXT("r.ArmASR.ReactiveMaskReactiveShadingModelID"),
	MSM_NUM,
	TEXT("Treat the specified shading model as reactive, taking the CustomData0.x value as the reactive value to write into the mask. Default is MSM_NUM (Off)."),
	ECVF_RenderThreadSafe
);

IMPLEMENT_GLOBAL_SHADER(FArmASRAccumulatePS, "/Plugin/ArmASR/Private/AccumulatePass.usf", "main", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArmASRComputeLuminancePyramidCS, "/Plugin/ArmASR/Private/ComputeLuminancePyramidPass.usf",
						"main", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FArmASRConvertVelocity, "/Plugin/ArmASR/Private/ConvertVelocity.usf", "main", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArmASRCopyExposureCS, "/Plugin/ArmASR/Private/CopyExposure.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FArmASRCreateReactiveMaskPS, "/Plugin/ArmASR/Private/CreateReactiveMask.usf", "main", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArmASRDepthClipPS, "/Plugin/ArmASR/Private/DepthClipPass.usf", "main", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArmASRLockCS, "/Plugin/ArmASR/Private/LockPass.usf", "main", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FArmASRRCASPS, "/Plugin/ArmASR/Private/RCASPass.usf", "main", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FArmASRReconstructPrevDepthPS, "/Plugin/ArmASR/Private/ReconstructPrevDepthPass.usf", "main",
						SF_Pixel);

// History written by frame N and read by frame N + 1.
class FArmASRTemporalAAHistory : public UE::Renderer::Private::ITemporalUpscaler::IHistory, public FRefCountBase
{
public:
	FArmASRTemporalAAHistory() :
		UpscaledColour{ nullptr },
		InternalReactive{ nullptr },
		LumaHistory{ nullptr },
		DilatedMotionVectors{ nullptr },
		DilatedDepthMotionVectorsInputLuma{ nullptr },
		LockStatus{ nullptr },
		PreExposure { 0.0 }
	{};

	virtual ~FArmASRTemporalAAHistory() = default;

	const TCHAR* GetDebugName() const override
	{
		return ArmASRUpscalerName;
	}

	uint64 GetGPUSizeBytes() const override
	{
	   return ComputeMemorySize();
	}

	uint32 AddRef() const final
	{
		return FRefCountBase::AddRef();
	}

	uint32 Release() const final
	{
		return FRefCountBase::Release();
	}

	uint32 GetRefCount() const final
	{
		return FRefCountBase::GetRefCount();
	}

	bool IsValid(const EShaderQualityPreset QualityPreset) const
	{
		bool AreRelevantMembersSet = true;

		const bool bIsBalancedOrPerformance = (QualityPreset == EShaderQualityPreset::BALANCED) || (QualityPreset == EShaderQualityPreset::PERFORMANCE);
		const bool bIsUltraPerformance = (QualityPreset == EShaderQualityPreset::ULTRA_PERFORMANCE);
		const EPixelFormat InternalUpscaledFormat = (bIsUltraPerformance || bIsBalancedOrPerformance) ? PF_FloatR11G11B10 : PF_FloatRGBA;
		if (!UpscaledColour || UpscaledColour->GetDesc().Format != InternalUpscaledFormat)
		{
			AreRelevantMembersSet = false;
		}
		if (!LumaHistory && (QualityPreset == EShaderQualityPreset::QUALITY))
		{
			AreRelevantMembersSet = false;
		}
		if (!DilatedMotionVectors && (QualityPreset != EShaderQualityPreset::ULTRA_PERFORMANCE))
		{
			AreRelevantMembersSet = false;
		}
		if (!DilatedDepthMotionVectorsInputLuma && (QualityPreset == EShaderQualityPreset::ULTRA_PERFORMANCE))
		{
			AreRelevantMembersSet = false;
		}
		if (!LockStatus)
		{
			AreRelevantMembersSet = false;
		}
		if (!InternalReactive && (QualityPreset != EShaderQualityPreset::QUALITY))
		{
			AreRelevantMembersSet = false;
		}

		return AreRelevantMembersSet;
	}

	uint64 ComputeMemorySize() const
	{
		uint64 Size = 0;

		if (UpscaledColour)
		{
			Size += UpscaledColour->ComputeMemorySize();
		}
		if (LumaHistory)
		{
			Size += LumaHistory->ComputeMemorySize();
		}
		if (InternalReactive)
		{
			Size += InternalReactive->ComputeMemorySize();
		}
		if (DilatedMotionVectors)
		{
			Size += DilatedMotionVectors->ComputeMemorySize();
		}
		if (LockStatus)
		{
			Size += LockStatus->ComputeMemorySize();
		}

		Size += sizeof(PreExposure);

		return Size;
	}

	TRefCountPtr<IPooledRenderTarget> UpscaledColour;
	TRefCountPtr<IPooledRenderTarget> InternalReactive;
	TRefCountPtr<IPooledRenderTarget> LumaHistory;
	TRefCountPtr<IPooledRenderTarget> DilatedMotionVectors;
	TRefCountPtr<IPooledRenderTarget> DilatedDepthMotionVectorsInputLuma;
	TRefCountPtr<IPooledRenderTarget> LockStatus;
	TRefCountPtr<IPooledRenderTarget> NewLock;
	float PreExposure;
};


static int32_t FRAME_INDEX = 0;

FArmASRTemporalUpscaler::FArmASRTemporalUpscaler(FArmASRInfo& ArmASRInfo, FArmASRPassthroughDenoiser& Denoiser)
	: ArmASRInfo(ArmASRInfo), Denoiser(Denoiser)
{
	GEngine->GetDynamicResolutionCurrentStateInfos(DynamicResolutionStateInfos);
	FFXSystemInterface::RegisterCustomFXSystem(
		ArmASRFXSystem::FXName,
		// There is no guarantee that the FArmASRTemporalUpscaler is alive when this lambda is called.
		// but the ArmASRInfo should be alive, so use that directly rather than passing the Upscaler.
		FCreateCustomFXSystemDelegate::CreateLambda([&ArmASRInfo](ERHIFeatureLevel::Type InFeatureLevel, EShaderPlatform InShaderPlatform, FGPUSortManager* InGPUSortManager) -> FFXSystemInterface*
			{
				return new ArmASRFXSystem(ArmASRInfo, InGPUSortManager);
			}));
}

FArmASRTemporalUpscaler::~FArmASRTemporalUpscaler()
{
}

const TCHAR* FArmASRTemporalUpscaler::GetDebugName() const
{
	return ArmASRUpscalerName;
}

UE::Renderer::Private::ITemporalUpscaler::FOutputs FArmASRTemporalUpscaler::AddPasses(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	const FInputs& Inputs) const
{
	using namespace UE::Renderer::Private;
	FOutputs Outputs;

	check(GMaxRHIFeatureLevel >= ERHIFeatureLevel::ES3_1);

	// Check the flags specified by the user.
	const bool bRequestedAutoExposure = static_cast<bool>(CVarArmASRAutoExposure.GetValueOnRenderThread());

	const EShaderQualityPreset QualityPreset = EShaderQualityPreset(FMath::Clamp(CVarArmASRShaderQuality.GetValueOnRenderThread(), int32(EShaderQualityPreset::QUALITY), int32(EShaderQualityPreset::ULTRA_PERFORMANCE)));
	const bool bIsQuality = (QualityPreset == EShaderQualityPreset::QUALITY);
	const bool bIsBalancedOrPerformance = (QualityPreset == EShaderQualityPreset::BALANCED) || (QualityPreset == EShaderQualityPreset::PERFORMANCE);
	const bool bIsPerformance = (QualityPreset == EShaderQualityPreset::PERFORMANCE);
	const bool bIsUltraPerformance = (QualityPreset == EShaderQualityPreset::ULTRA_PERFORMANCE);

	const float Sharpness = FMath::Clamp(CVarArmASRSharpness.GetValueOnRenderThread(), 0.0f, 1.0f);
	const bool bApplySharpening = (Sharpness > 0.0f);

	// Check is running on GLES 3.2
	const EPixelFormat maskFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R32_FLOAT : PF_R8;

	// Setup input and output extents and viewports
	FViewInfo& ViewInfo = (FViewInfo&)(View);
	FIntPoint InputExtents = ViewInfo.ViewRect.Size();
	FIntPoint OutputExtents = ViewInfo.GetSecondaryViewRectSize();
	OutputExtents = FIntPoint(FMath::Max(InputExtents.X, OutputExtents.X), FMath::Max(InputExtents.Y, OutputExtents.Y));

	FScreenPassTextureViewport InputViewport(FIntRect(0, 0, InputExtents.X, InputExtents.Y));
	FScreenPassTextureViewport OutputViewport(FIntRect(0, 0, OutputExtents.X, OutputExtents.Y));

	FIntPoint InputExtentsQuantized;
	FIntPoint OutputExtentsQuantized;
	QuantizeSceneBufferSize(InputExtents, InputExtentsQuantized);
	QuantizeSceneBufferSize(OutputExtents, OutputExtentsQuantized);

	FRDGTextureRef SceneColor = Inputs.SceneColor.Texture;
	FRDGTextureRef SceneDepth = Inputs.SceneDepth.Texture;
	FRDGTextureRef VelocityTexture = Inputs.SceneVelocity.Texture;
	check(SceneColor);
	check(SceneDepth);
	check(VelocityTexture);

	// Core input textures
	FRDGTextureSRVDesc SceneColorDesc = FRDGTextureSRVDesc::Create(SceneColor);
	FRDGTextureSRVRef SceneColorTexture = GraphBuilder.CreateSRV(SceneColorDesc);
	FRDGTextureSRVDesc DepthDesc = FRDGTextureSRVDesc::Create(SceneDepth);
	FRDGTextureSRVRef DepthTexture = GraphBuilder.CreateSRV(DepthDesc);
	FRDGTextureSRVDesc MotionVectorDesc = FRDGTextureSRVDesc::Create(VelocityTexture);

	// Create the output texture and assign it to Outputs. This will be updated in the Accumulate or RCAS shader.
	FRDGTextureDesc OutputColorDesc = Inputs.SceneColor.Texture->Desc;
	OutputColorDesc.Extent = OutputExtents;
	OutputColorDesc.Flags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
	Outputs.FullRes.Texture = GraphBuilder.CreateTexture(
		OutputColorDesc,
		TEXT("ArmASROutputSceneColor"),
		ERDGTextureFlags::MultiFrame);
	Outputs.FullRes.ViewRect = Inputs.OutputViewRect;

	// Get previous history. These textures will be used as inputs for some of the shaders.
	FRDGTextureRef PrevUpscaledColour{ nullptr };
	FRDGTextureRef PrevInternalReactive{ nullptr };
	FRDGTextureRef PrevLumaHistory{ nullptr };
	FRDGTextureRef PrevDilatedMotionVectors{ nullptr };
	FRDGTextureRef PrevDilatedDepthMotionVectorsInputLuma{nullptr};
	FRDGTextureRef PrevLockStatus{ nullptr };
	FRDGTextureRef NewLock{ nullptr };
	float PrevPreExposure{ 0.0 };

	FArmASRTemporalAAHistory* PrevHistory = static_cast<FArmASRTemporalAAHistory*>(Inputs.PrevHistory.GetReference());

	// Check for camera cuts and a valid history.
	bool bCameraCut = View.bCameraCut || !ViewInfo.ViewState;
	bool ValidHistory = PrevHistory && PrevHistory->IsValid(QualityPreset) && !bCameraCut;

	if (ValidHistory)
	{
		PrevUpscaledColour = GraphBuilder.RegisterExternalTexture(PrevHistory->UpscaledColour, TEXT("PrevUpscaledColour"));

		// Balanced/Performance/UltraPerformance preset specific
		if (!bIsQuality)
		{
			// Internal reactive history
			PrevInternalReactive = GraphBuilder.RegisterExternalTexture(PrevHistory->InternalReactive, TEXT("InternalReactive"));

			// Luma history not used when using Balanced/Performance preset
			PrevLumaHistory = GSystemTextures.GetBlackDummy(GraphBuilder);
		}
		else
		{
			PrevLumaHistory = GraphBuilder.RegisterExternalTexture(PrevHistory->LumaHistory, TEXT("PrevLumaHistory"));

			// Separate internal reactive not used by Quality preset. Internal upscaled color (R16G16B16A16_Float) contains this in Alpha channel
			PrevInternalReactive = GSystemTextures.GetBlackDummy(GraphBuilder);
		}

		// Lock pass output (UAV-based feedback loop)
		if (!PrevHistory->NewLock.IsValid())
		{
			FRDGTextureDesc LockMaskDesc =
				FRDGTextureDesc::Create2D(OutputExtents, maskFormat, FClearValueBinding::Black,
										  TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, 1, 1);
			NewLock = GraphBuilder.CreateTexture(LockMaskDesc, TEXT("LockMaskTexture"));

		}
		else
		{
			NewLock = GraphBuilder.RegisterExternalTexture(PrevHistory->NewLock, TEXT("LockMaskTexture"));
		}

		if (bIsUltraPerformance)
		{
			PrevDilatedDepthMotionVectorsInputLuma = GraphBuilder.RegisterExternalTexture(PrevHistory->DilatedDepthMotionVectorsInputLuma, TEXT("PrevDilatedDepthMotionVectorsInputLuma"));
		}
		else
		{
			PrevDilatedMotionVectors = GraphBuilder.RegisterExternalTexture(PrevHistory->DilatedMotionVectors, TEXT("PrevDilatedMotionVectors"));
		}

		PrevLockStatus = GraphBuilder.RegisterExternalTexture(PrevHistory->LockStatus, TEXT("PrevLockStatus"));
		PrevPreExposure = PrevHistory->PreExposure;
	}
	else
	{
		PrevUpscaledColour = GSystemTextures.GetBlackDummy(GraphBuilder);
		PrevLumaHistory = GSystemTextures.GetBlackDummy(GraphBuilder);
		PrevDilatedMotionVectors = GSystemTextures.GetBlackDummy(GraphBuilder);
		PrevDilatedDepthMotionVectorsInputLuma = GSystemTextures.GetBlackDummy(GraphBuilder);
		PrevLockStatus = GSystemTextures.GetBlackDummy(GraphBuilder);
		PrevInternalReactive = GSystemTextures.GetBlackDummy(GraphBuilder);

		// Create New Lock texture.
		FRDGTextureDesc LockMaskDesc =
			FRDGTextureDesc::Create2D(OutputExtents, maskFormat, FClearValueBinding::Black,
									  TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, 1, 1);
		NewLock = GraphBuilder.CreateTexture(LockMaskDesc, TEXT("LockMaskTexture"));

		// Reset the frame index.
		FRAME_INDEX = 0;
	}
	AddClearRenderTargetPass(GraphBuilder, NewLock);

	FRDGTextureRef ReactiveMaskTexture = nullptr;
	FRDGTextureRef CompositeMaskTexture = nullptr;

	if (!bIsUltraPerformance && CVarArmASRCreateReactiveMask.GetValueOnRenderThread() &&
		ArmASRInfo.PostInputs.SceneTextures)
	{
		FRDGTextureDesc ReactiveMaskDesc =
			FRDGTextureDesc::Create2D(InputExtents, maskFormat, FClearValueBinding::Black,
									  TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
		FRDGTextureDesc CompositeMaskDesc =
			FRDGTextureDesc::Create2D(InputExtents, maskFormat, FClearValueBinding::Black,
									  TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

		ReactiveMaskTexture = GraphBuilder.CreateTexture(ReactiveMaskDesc, TEXT("ArmASRReactiveMaskTexture"));
		CompositeMaskTexture = GraphBuilder.CreateTexture(CompositeMaskDesc, TEXT("ArmASRCompositeMaskTexture"));

		FArmASRCreateReactiveMaskPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FArmASRCreateReactiveMaskPS::FParameters>();
		SetReactiveMaskParameters(GraphBuilder, PassParameters, ArmASRInfo,
			InputExtents,
			InputViewport.Rect,
			ReactiveMaskTexture,
			CompositeMaskTexture,
			SceneDepth,
			SceneColor,
			VelocityTexture,
			ValidHistory,
			View);
		TShaderMapRef<FArmASRCreateReactiveMaskPS> ReactiveMaskShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("Create Reactive Mask (PS)"),
			ReactiveMaskShader,
			PassParameters,
			InputViewport.Rect);
	}
	else
	{
		ReactiveMaskTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
		CompositeMaskTexture = GraphBuilder.RegisterExternalTexture(GSystemTextures.BlackDummy);
	}

	// Convert Motion Vectors texture to R16G16_Float, so they can be used correctly by the shaders.
	FRDGTextureDesc MotionVectorDescNew = FRDGTextureDesc::Create2D(InputExtentsQuantized, PF_G16R16F, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);
	FRDGTextureRef MotionVectorTextureNew = GraphBuilder.CreateTexture(MotionVectorDescNew, TEXT("ArmASRMotionVectorTexture"));
	{
		FArmASRConvertVelocity::FParameters* ConvertVelocityParameters = GraphBuilder.AllocParameters<FArmASRConvertVelocity::FParameters>();

		FRDGTextureUAVDesc OutputDesc(MotionVectorTextureNew);
		ConvertVelocityParameters->DepthTexture = SceneDepth;
		ConvertVelocityParameters->InputDepth = GraphBuilder.CreateSRV(DepthDesc);
		ConvertVelocityParameters->InputVelocity = GraphBuilder.CreateSRV(MotionVectorDesc);
		ConvertVelocityParameters->View = View.ViewUniformBuffer;

		const FScreenPassRenderTarget MotionVectorNewRT(MotionVectorTextureNew, InputViewport.Rect, ERenderTargetLoadAction::ENoAction);
		ConvertVelocityParameters->RenderTargets[0] = MotionVectorNewRT.GetRenderTargetBinding();

		TShaderMapRef<FArmASRConvertVelocity> ConvertVelocityShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("ConvertVelocity (PS)"),
			ConvertVelocityShader,
			ConvertVelocityParameters,
			InputViewport.Rect);
	}

	// Setup common parameters
	FArmASRPassParameters* ArmASRPassParameters = GraphBuilder.AllocParameters<FArmASRPassParameters>();
	const FIntPoint& ResourceDimensions = SceneColor->Desc.Extent;
	SetCommonParameters(ArmASRPassParameters, FRAME_INDEX, PrevPreExposure, InputExtents, OutputExtents, ViewInfo, ResourceDimensions);

	// Update frame index for next frame.
	FRAME_INDEX = (FRAME_INDEX + 1);

	// Assign common parameters to buffer.
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParametersBuffer = TUniformBufferRef<FArmASRPassParameters>::CreateUniformBufferImmediate(*ArmASRPassParameters, UniformBuffer_SingleDraw);

	// Compute Luminance Shader
	// ------------------------
	FArmASRComputeLuminancePyramidCS::FParameters* ClpShaderParameters = GraphBuilder.AllocParameters<FArmASRComputeLuminancePyramidCS::FParameters>();
	FArmASRComputeLuminanceParameters* ClpParameters = GraphBuilder.AllocParameters<FArmASRComputeLuminanceParameters>();
	if (!bIsUltraPerformance)
	{
		FIntVector workgroupCount(0, 0, 0);
		SetComputeLuminancePyramidParameters(
			ClpShaderParameters,
			ClpParameters,
			ArmASRPassParametersBuffer,
			SceneColorTexture,
			InputExtents,
			GraphBuilder,
			workgroupCount,
			ArmASRInfo);

		FArmASRComputeLuminancePyramidCS::FPermutationDomain PermutationVector;
		const ERHIFeatureSupport WaveOpsSupport = FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(View.GetShaderPlatform());
		const bool bUseWaveOps = (WaveOpsSupport == ERHIFeatureSupport::RuntimeGuaranteed) ||
								 (WaveOpsSupport == ERHIFeatureSupport::RuntimeDependent && GRHISupportsWaveOperations);
		PermutationVector.Set<FArmASR_UseWaveOps>(bUseWaveOps);
		TShaderMapRef<FArmASRComputeLuminancePyramidCS> ClpShader(ViewInfo.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Compute Luminance Pyramid (CS)"),
			ClpShader,
			ClpShaderParameters,
			workgroupCount
		);
	}

	// If AutoExposure is enabled use Exposure generated from Compute Luminance shader, otherwise use Engine exposure.
	FRDGTextureRef ExposureTexture = nullptr;
	if (bRequestedAutoExposure)
	{
		ExposureTexture = ClpShaderParameters->rw_auto_exposure->Desc.Texture;
	}
	else
	{
		// Setup and run shader to get exposure from Unreal Engine.
		FArmASRCopyExposureCS::FParameters* CopyExposureParameters = GraphBuilder.AllocParameters<FArmASRCopyExposureCS::FParameters>();
		SetCopyExposureParameters(CopyExposureParameters, View, GraphBuilder);

		TShaderMapRef<FArmASRCopyExposureCS> CopyExposureShader(ViewInfo.ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CopyExposure (CS)"),
			CopyExposureShader,
			CopyExposureParameters,
			FComputeShaderUtils::GetGroupCount(FIntVector(1, 1, 1), FIntVector(1, 1, 1))
		);
		ExposureTexture = CopyExposureParameters->ExposureTexture->Desc.Texture;
	}

	// Create Exposure SRV texture once and pass to other shaders.
	FRDGTextureSRVDesc AutoExposureDesc = FRDGTextureSRVDesc::Create(ExposureTexture);
	FRDGTextureSRVRef AutoExposureTexture = GraphBuilder.CreateSRV(AutoExposureDesc);

	// Reconstruct Prev Depth Shader
	// -----------------------------
	FArmASRReconstructPrevDepthPS::FParameters* RpdShaderParameters = GraphBuilder.AllocParameters<FArmASRReconstructPrevDepthPS::FParameters>();
	{
		SetReconstructPrevDepthParameters(
			bIsUltraPerformance,
			RpdShaderParameters,
			ArmASRPassParametersBuffer,
			MotionVectorTextureNew,
			DepthTexture,
			SceneColorTexture,
			AutoExposureTexture, // Generated from Compute Luminance Pyramid or Unreal Engine
			InputExtents,
			InputViewport,
			GraphBuilder);

		// Create shader and add pass.
		FArmASRReconstructPrevDepthPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FArmASR_ApplyUltraPerfOpt>(bIsUltraPerformance);
		TShaderMapRef<FArmASRReconstructPrevDepthPS> RpdShader(ViewInfo.ShaderMap, PermutationVector);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("Reconstruct Previous Depth (PS)"),
			RpdShader,
			RpdShaderParameters,
			InputViewport.Rect);
	}

	// Depth Clip Shader
	// -----------------
	FArmASRDepthClipPS::FParameters* DcShaderParameters = GraphBuilder.AllocParameters<FArmASRDepthClipPS::FParameters>();

	FRDGTextureRef DilatedDepthMotionVectorsInputLumaTexture = bIsUltraPerformance ? RpdShaderParameters->RenderTargets[0].GetTexture() : nullptr;
	FRDGTextureRef DilatedMotionVectorTexture = bIsUltraPerformance ? nullptr : RpdShaderParameters->RenderTargets[1].GetTexture();
	{
		SetDepthClipParameters(
			DcShaderParameters,
			ArmASRPassParametersBuffer,
			AutoExposureTexture, // Generated UAV from Compute Luminance Pyramid or Unreal Engine
			RpdShaderParameters->rw_reconstructed_previous_nearest_depth->Desc.Texture, // Generated UAV from Reconstruct Prev Depth
			RpdShaderParameters->RenderTargets[0].GetTexture(), // Generated RT from Reconstruct Prev Depth
			DilatedMotionVectorTexture, // Generated RT from Reconstruct Prev Depth
			PrevDilatedMotionVectors,
			DilatedDepthMotionVectorsInputLumaTexture,
			PrevDilatedDepthMotionVectorsInputLuma,
			MotionVectorTextureNew,
			ReactiveMaskTexture,
			CompositeMaskTexture,
			DepthTexture,
			SceneColorTexture,
			QualityPreset,
			InputExtents,
			InputViewport,
			GraphBuilder);

		FArmASRDepthClipPS::FPermutationDomain PermutationVector;
		// Depth clip applies optimizations for the Performance preset
		PermutationVector.Set<FArmASR_ApplyBalancedOpt>(bIsBalancedOrPerformance);
		PermutationVector.Set<FArmASR_ApplyPerfOpt>(bIsPerformance);
		PermutationVector.Set<FArmASR_ApplyUltraPerfOpt>(bIsUltraPerformance);
		TShaderMapRef<FArmASRDepthClipPS> DcShader(ViewInfo.ShaderMap, PermutationVector);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("Depth Clip (PS)"),
			DcShader,
			DcShaderParameters,
			InputViewport.Rect);
	}

	// Lock Shader
	// -----------
	FRDGTextureRef LockInputLumaTexture = bIsUltraPerformance ? nullptr : RpdShaderParameters->RenderTargets[2].GetTexture();
	FArmASRLockCS::FParameters* LShaderParameters = GraphBuilder.AllocParameters<FArmASRLockCS::FParameters>();
	{
		SetLockParameters(
			bIsUltraPerformance,
			LShaderParameters,
			ArmASRPassParametersBuffer,
			LockInputLumaTexture, // Generated RT from Reconstruct Prev Depth
		    DilatedDepthMotionVectorsInputLumaTexture,
			NewLock,
			OutputExtents,
			GraphBuilder
		);
		FArmASRLockCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FArmASR_ApplyUltraPerfOpt>(bIsUltraPerformance);
		TShaderMapRef<FArmASRLockCS> LShader(ViewInfo.ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Lock (CS)"),
			LShader,
			LShaderParameters,
			FComputeShaderUtils::GetGroupCount(Inputs.SceneColor.ViewRect.Size(), FComputeShaderUtils::kGolden2DGroupSize)
		);
	}

	// Accumulate Shader
	// -----------------
	FRDGTextureRef ImgMipShadingChangeTexture = bIsUltraPerformance ? nullptr : ClpShaderParameters->rw_img_mip_shading_change->Desc.Texture;
	FArmASRAccumulatePS::FParameters* AccumulateParameters = GraphBuilder.AllocParameters<FArmASRAccumulatePS::FParameters>();
	{
		SetAccumulateParameters(
			AccumulateParameters,
			ArmASRPassParametersBuffer,
			AutoExposureTexture,							   // Generated from Compute Luminance Pyramid or Unreal Engine
		    ImgMipShadingChangeTexture,						   // Generated from Compute Luminance Pyramid
		    DilatedMotionVectorTexture,						   // Dilated Motion vector is generated from Reconstruct Prev Depth
			DilatedDepthMotionVectorsInputLumaTexture,
			DcShaderParameters->RenderTargets[0].GetTexture(), // Dilated Reactive Mask is generated from Depth clip
			DcShaderParameters->RenderTargets[1].GetTexture(), // Prepared input colour is generated from Depth clip
		    SceneColorTexture,
			PrevLockStatus,
			Outputs.FullRes.Texture,
			MotionVectorTextureNew,
			PrevUpscaledColour,
			PrevLumaHistory,
			PrevInternalReactive,
			LShaderParameters->rw_new_locks->Desc.Texture, // Generated from Lock
			Sharpness,
			QualityPreset,
			OutputExtents,
			OutputViewport.Rect,
			GraphBuilder);

		FArmASRAccumulatePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FArmASR_DoSharpening>(bApplySharpening);
		// Choose the correct permutation based on quality preset
		PermutationVector.Set<FArmASR_ApplyBalancedOpt>(bIsBalancedOrPerformance ? 1 : 0);
		PermutationVector.Set<FArmASR_ApplyPerfOpt>(bIsPerformance ? 1 : 0);
		PermutationVector.Set<FArmASR_ApplyUltraPerfOpt>(bIsUltraPerformance ? 1 : 0);

		TShaderMapRef<FArmASRAccumulatePS> AccumulateShader(ViewInfo.ShaderMap, PermutationVector);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("Accumulate (PS)"),
			AccumulateShader,
			AccumulateParameters,
			OutputViewport.Rect);
	}

	// Add RCAS if necessary
	if (Sharpness > 0.0f)
	{
		FArmASRRCASPS::FParameters* RcasParameters = GraphBuilder.AllocParameters<FArmASRRCASPS::FParameters>();
		FArmASRRCASParameters* rcasPassParameters = GraphBuilder.AllocParameters<FArmASRRCASParameters>();
		SetRCASParameters(
			RcasParameters,
			rcasPassParameters,
			ArmASRPassParametersBuffer,
			ExposureTexture,
			AccumulateParameters->RenderTargets[0].GetTexture(),
			Outputs.FullRes.Texture,
			Sharpness,
			OutputViewport.Rect,
			GraphBuilder);

		TShaderMapRef<FArmASRRCASPS> RcasShader(ViewInfo.ShaderMap);
		FPixelShaderUtils::AddFullscreenPass(
			GraphBuilder, ViewInfo.ShaderMap,
			RDG_EVENT_NAME("RCAS (PS)"),
			RcasShader,
			RcasParameters,
			OutputViewport.Rect);
	}

	// Set up new history
	TRefCountPtr<FArmASRTemporalAAHistory> NewHistory(new FArmASRTemporalAAHistory());

	if (!bIsQuality)
	{
		NewHistory->LumaHistory = nullptr;
		const int32 TemporalReactiveIdx = (!bIsQuality) ? 1 : -1;
		FRDGTextureRef TemporalReactiveTexture = AccumulateParameters->RenderTargets[TemporalReactiveIdx].GetTexture();
		GraphBuilder.QueueTextureExtraction(TemporalReactiveTexture, &NewHistory->InternalReactive);
	}
	else
	{
		NewHistory->InternalReactive = nullptr;
		FRDGTextureRef LumaHistoryTexture = AccumulateParameters->RenderTargets[2].GetTexture();
		GraphBuilder.QueueTextureExtraction(LumaHistoryTexture, &NewHistory->LumaHistory);
	}

	const int32 LockStatusIdx = bIsUltraPerformance ? 1 : (bIsBalancedOrPerformance ? 2 : 1);
	FRDGTextureRef InternalUpscaledColor = AccumulateParameters->RenderTargets[0].GetTexture();
	FRDGTextureRef LockStatusTexture = AccumulateParameters->RenderTargets[LockStatusIdx].GetTexture();

	GraphBuilder.QueueTextureExtraction(NewLock, &NewHistory->NewLock);
	GraphBuilder.QueueTextureExtraction(InternalUpscaledColor, &NewHistory->UpscaledColour);
	GraphBuilder.QueueTextureExtraction(LockStatusTexture, &NewHistory->LockStatus);
	if (bIsUltraPerformance)
	{
		GraphBuilder.QueueTextureExtraction(DilatedDepthMotionVectorsInputLumaTexture, &NewHistory->DilatedDepthMotionVectorsInputLuma);
	}
	else
	{
		GraphBuilder.QueueTextureExtraction(DilatedMotionVectorTexture, &NewHistory->DilatedMotionVectors);
	}
	NewHistory->PreExposure = ArmASRPassParameters->fPreExposure;

	Outputs.NewHistory = NewHistory;

	return Outputs;
}

float FArmASRTemporalUpscaler::GetMinUpsampleResolutionFraction() const { return 0.1f; }
float FArmASRTemporalUpscaler::GetMaxUpsampleResolutionFraction() const { return 100.0f; }

UE::Renderer::Private::ITemporalUpscaler* FArmASRTemporalUpscaler::Fork_GameThread(const class FSceneViewFamily& ViewFamily) const
{
	return new FArmASRTemporalUpscaler(ArmASRInfo, Denoiser);
}

class FArmASRSceneViewExtension : public FSceneViewExtensionBase
{
public:
	FArmASRSceneViewExtension(const FAutoRegister& AutoRegister)
		 : FSceneViewExtensionBase(AutoRegister)
		, Denoiser(ArmASRInfo)
	{
	}

	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override
	{
		if (Context.Viewport == nullptr)
		{
			return false;
		}

		return true;
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily)
	{
	}

	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
	{
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& ViewFamily) override
	{
		bool Enable = false;

		if (CVarArmASREnable.GetValueOnGameThread() == 0)
		{
			CleanUpArmASRInfoAll(ArmASRInfo);
			return;
		}

		// Another plugin has already set a temporal upscaler interface - if we try to set it again
		// then it will assert, so we have to yield
		if (ViewFamily.GetTemporalUpscalerInterface() != nullptr)
		{
			return;
		}

		// Only support a single view now as there is state
		if (ViewFamily.Views.Num() != 1)
		{
			static bool AlreadyLogged = false;
			if (!AlreadyLogged)
			{
				UE_LOG(LogArmASR, Warning, TEXT("Arm ASR does not support multiple views. Disabling Arm ASR."));
				AlreadyLogged = true;
			}
			return;
		}

		const FSceneView* View = ViewFamily.Views[0];
		if (View->PrimaryScreenPercentageMethod == EPrimaryScreenPercentageMethod::TemporalUpscale)
		{
			Enable = true;
		}

		if (Enable)
		{
			ViewFamily.SetTemporalUpscalerInterface(new FArmASRTemporalUpscaler(ArmASRInfo, Denoiser));
			InitFArmASRDenoiser(Denoiser);
			return;
		}
	}

	void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessingInputs& Inputs)
	{
		if (View.GetFeatureLevel() >= ERHIFeatureLevel::ES3_1)
		{
			if (CVarArmASREnable.GetValueOnAnyThread())
			{
				ArmASRInfo.PostInputs = Inputs;
			}
		}
	}

	void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
	{
		if (InView.GetFeatureLevel() >= ERHIFeatureLevel::ES3_1)
		{
			if (CVarArmASREnable.GetValueOnAnyThread())
			{
				if (InView.State)
				{
					FReflectionTemporalState& ReflectionTemporalState = ((FSceneViewState*)InView.State)->Lumen.ReflectionState;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5
					ArmASRInfo.LumenReflections = ReflectionTemporalState.SpecularIndirectHistoryRT;
#else
					ArmASRInfo.LumenReflections = ReflectionTemporalState.SpecularAndSecondMomentHistory;
#endif
				}
			}
		}
	}

	void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
	{
		if (InViewFamily.GetFeatureLevel() >= ERHIFeatureLevel::ES3_1)
		{
			if (CVarArmASREnable.GetValueOnAnyThread())
			{
				CleanUpArmASRInfoFrameInfo(ArmASRInfo);
			}
		}
	}

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override
	{
	}

	FArmASRInfo& GetArmASRInfo()
	{
		return ArmASRInfo;
	}

	FArmASRPassthroughDenoiser& GetDenoiser()
	{
		return Denoiser;
	}

private:
	FArmASRInfo ArmASRInfo;
	FArmASRPassthroughDenoiser Denoiser;
};

void FArmASRModule::StartupModule()
{
#if ARM_ASR_ENABLE_VK
	{
		const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME  };
		((IVulkanDynamicRHI*)GDynamicRHI)->AddEnabledDeviceExtensionsAndLayers(ExtentionsToAdd, TArray<const ANSICHAR*>());
	}
#endif
	const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ArmASR"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/Plugin/ArmASR"), PluginShaderDir);

	const FString HeadersDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("ArmASR"))->GetBaseDir(), TEXT("Shaders/Private/fsr2"));
	AddShaderSourceDirectoryMapping(TEXT("/ThirdParty/ArmASR"), HeadersDir);

	// We can't register the SceneViewExtension yet, as the Engine hasn't been initialized yet.
	// Register a callback so that we do it later.
	OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FArmASRModule::OnPostEngineInit);
}

void FArmASRModule::ShutdownModule()
{
	SceneViewExtension = nullptr;

	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "ArmASR");
	}

#if WITH_EDITOR
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedHandle);
#endif
	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
}

void FArmASRModule::OnPostEngineInit()
{
	SceneViewExtension = FSceneViewExtensions::NewExtension<FArmASRSceneViewExtension>();
	TemporalUpscaler = MakeUnique<FArmASRTemporalUpscaler>(SceneViewExtension->GetArmASRInfo(), SceneViewExtension->GetDenoiser());

#if WITH_EDITOR
	OnObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FArmASRModule::OnObjectPropertyChanged);
#endif
}

#if WITH_EDITOR
void FArmASRModule::OnObjectPropertyChanged(UObject* Obj, FPropertyChangedEvent& Event)
{
	if (Obj == GetDefault<UArmASRSettings>())
	{
		bool Reload = false;
		if (Event.Property == nullptr)
		{
			Reload = true;
		}
		if (Event.Property != nullptr)
		{
			const FName PropertyName(Event.Property->GetFName());
		}
	}
}
#endif

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArmASRModule, ArmASR)
