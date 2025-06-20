//
// Copyright © 2024-2025 Arm Limited.
// SPDX-License-Identifier: MIT
//

#include "ArmASRShaderParameters.h"
#include "ArmASRShaderUtils.h"
#include "ArmASRInfo.h"

#include "RenderGraphFwd.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterStruct.h"
#include "DataDrivenShaderPlatformInfo.h"

#include <array>

class FArmASR_UseWaveOps : SHADER_PERMUTATION_BOOL("FFXM_SPD_WAVE_OPERATIONS");

class FArmASRComputeLuminancePyramidCS : public FGlobalShader
{
public:
	using FPermutationDomain = TShaderPermutationDomain<FArmASR_UseWaveOps>;

	DECLARE_GLOBAL_SHADER(FArmASRComputeLuminancePyramidCS);
	SHADER_USE_PARAMETER_STRUCT(FArmASRComputeLuminancePyramidCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FArmASRPassParameters, cbArmASR)
		SHADER_PARAMETER_STRUCT_REF(FArmASRComputeLuminanceParameters, cbArmASRSPD)
		SHADER_PARAMETER_SAMPLER(SamplerState, s_LinearClamp)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, r_input_color_jittered)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_spd_global_atomic)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_img_mip_shading_change)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_img_mip_5)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, rw_auto_exposure)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		if (!FArmASRGlobalShader::ShouldCompilePermutation(Parameters))
		{
			return false;
		}

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		bool bPemutationUseWaveOps = PermutationVector.Get<FArmASR_UseWaveOps>();

		ERHIFeatureSupport WaveOpsSupport =
			FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Parameters.Platform);
		if (bPemutationUseWaveOps && WaveOpsSupport == ERHIFeatureSupport::Unsupported)
		{
			// Don't compile a permutation that uses wave ops if this platform can never support them
			return false;
		}
		else if (!bPemutationUseWaveOps && WaveOpsSupport == ERHIFeatureSupport::RuntimeGuaranteed)
		{
			// Don't bother to compile a permutation that doesn't use wave ops if this platform always supports support
			// them, as we would never use such a permutation.
			return false;
		}

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters &Parameters,
											 FShaderCompilerEnvironment &OutEnvironment)
	{
		// Define common shader flags.
		FArmASRGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		bool const bUseWaveOps = PermutationVector.Get<FArmASR_UseWaveOps>();
		if (bUseWaveOps)
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};

// Function to setup Compute Luminance Shader parameters. ClpShaderParameters and ClpParameters will be updated.
inline void SetComputeLuminancePyramidParameters(
	FArmASRComputeLuminancePyramidCS::FParameters* ClpShaderParameters,
	FArmASRComputeLuminanceParameters* ClpParameters,
	TUniformBufferRef<FArmASRPassParameters> ArmASRPassParameters,
	const FRDGTextureSRVRef SceneColorTexture,
	const FIntPoint InputExtents,
	FRDGBuilder& GraphBuilder,
	FIntVector& workGroups,
	FArmASRInfo& ArmASRInfo)
{
	// Sampler state
	ClpShaderParameters->s_LinearClamp = TStaticSamplerState<SF_Bilinear>::GetRHI();

	// SRV's
	ClpShaderParameters->r_input_color_jittered = SceneColorTexture;

	// UAV's
	const FIntPoint Size = { 1, 1 };
	if (ArmASRInfo.Atomic)
	{
		FRDGTextureRef GlobalAtomicTexture = GraphBuilder.RegisterExternalTexture(ArmASRInfo.Atomic.GetValue().RenderTarget, TEXT("GlobalAtomicTexture"));
		FRDGTextureUAVDesc GlobalAtomicUAVDesc(GlobalAtomicTexture);
		ClpShaderParameters->rw_spd_global_atomic = GraphBuilder.CreateUAV(GlobalAtomicUAVDesc);
	}
	else
	{
		FArmASRResource Atomic;
		const uint32 AtomicInitValue = 0U;
		TextureBulkData AtomicValueBulkData(&AtomicInitValue, sizeof(AtomicInitValue));

		FRHITextureCreateDesc AtomicDesc = FRHITextureCreateDesc::Create2D(TEXT("GlobalAtomicTexture2D"), Size.X, Size.Y, PF_R32_UINT);
		AtomicDesc.SetBulkData(&AtomicValueBulkData);
		AtomicDesc.SetNumMips(1);
		AtomicDesc.SetInitialState(ERHIAccess::SRVCompute);
		AtomicDesc.SetNumSamples(1);
		AtomicDesc.SetFlags(TexCreate_UAV | TexCreate_ShaderResource);

		Atomic.Texture = RHICreateTexture(AtomicDesc);
		Atomic.Texture->AddRef();

		Atomic.RenderTarget = CreateRenderTarget(Atomic.Texture.GetReference(), TEXT("GlobalAtomicTextureRT"));

		FRDGTextureRef GlobalAtomicTexture = GraphBuilder.RegisterExternalTexture(Atomic.RenderTarget, TEXT("GlobalAtomicTexture"));
		FRDGTextureUAVDesc GlobalAtomicUAVDesc(GlobalAtomicTexture);

		ClpShaderParameters->rw_spd_global_atomic = GraphBuilder.CreateUAV(GlobalAtomicUAVDesc);
		ArmASRInfo.Atomic = std::move(Atomic);
	}

	using IntType = FIntPoint::IntType;
	const FIntPoint MipSize = { static_cast<IntType>(0.5 * InputExtents.X), static_cast<IntType>(0.5 * InputExtents.Y) };
	const bool bIsOpenGL = IsOpenGLPlatform(GMaxRHIShaderPlatform);

	const uint32 MipCount = uint32(1 + floor(log2(FMath::Max(MipSize.X, MipSize.Y))));
	const EPixelFormat MipShadingFormat = bIsOpenGL ? PF_R32_FLOAT : PF_R16F;
	FRDGTextureDesc MipShadingChangeDesc = FRDGTextureDesc::Create2D(
		MipSize, MipShadingFormat, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, MipCount, 1);
	FRDGTextureRef MipShadingChangeTexture = GraphBuilder.CreateTexture(MipShadingChangeDesc, TEXT("MipShadingChangeTexture"));
	FRDGTextureUAVDesc MipShadingChangeUAVDesc(MipShadingChangeTexture, FFXM_FSR2_SHADING_CHANGE_MIP_LEVEL);
	ClpShaderParameters->rw_img_mip_shading_change = GraphBuilder.CreateUAV(MipShadingChangeUAVDesc);

	FRDGTextureUAVDesc Mip5UAVDesc(MipShadingChangeTexture, FFXM_FSR2_SHADING_CHANGE_MIPMAP_5);
	ClpShaderParameters->rw_img_mip_5 = GraphBuilder.CreateUAV(Mip5UAVDesc);

	const EPixelFormat AutoExposureFormat = bIsOpenGL ? PF_FloatRGBA : PF_G32R32F;
	FRDGTextureDesc AutoExposureDesc =
		FRDGTextureDesc::Create2D(Size, AutoExposureFormat, FClearValueBinding::Black,
								  TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable, 1, 1);
	FRDGTextureRef AutoExposureTexture = GraphBuilder.CreateTexture(AutoExposureDesc, TEXT("AutoExposureTexture"));
	FRDGTextureUAVDesc AutoExposureUAVDesc(AutoExposureTexture);
	ClpShaderParameters->rw_auto_exposure = GraphBuilder.CreateUAV(AutoExposureUAVDesc);

	// Setup FArmASRComputeLuminanceParameters
	std::array<uint32_t, 4> RectInfo = { 0, 0, InputExtents.X, InputExtents.Y };
	SpdConfig spdConfig = SpdConfig();
	spdConfig.Setup(RectInfo, -1);

	// Assign values from SpdSetup
	ClpParameters->numWorkGroups = spdConfig.NumWorkGroupsAndMips[0];
	ClpParameters->mips = spdConfig.NumWorkGroupsAndMips[1];
	ClpParameters->workGroupOffset = { spdConfig.WorkGroupOffset[0], spdConfig.WorkGroupOffset[1] };
	ClpParameters->renderSize = { static_cast<uint32_t>(InputExtents.X), static_cast<uint32_t>(InputExtents.Y) };

	// Assign common and specific parameters to buffers.
	ClpShaderParameters->cbArmASRSPD = TUniformBufferRef<FArmASRComputeLuminanceParameters>::CreateUniformBufferImmediate(*ClpParameters, UniformBuffer_SingleDraw);
	ClpShaderParameters->cbArmASR = ArmASRPassParameters;

	workGroups = FIntVector(spdConfig.DispatchThreadGroupCountXY[0], spdConfig.DispatchThreadGroupCountXY[1], 1);
}
