// Copyright  © 2023 Advanced Micro Devices, Inc.
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

#include "ffxm_fsr2_resources.h"

#if defined(FFXM_GPU)
#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic push
#pragma dxc diagnostic ignored "-Wambig-lit-shift"
#endif //__hlsl_dx_compiler
#include "ffxm_core.h"
#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic pop
#endif //__hlsl_dx_compiler
#endif // #if defined(FFXM_GPU)

#if defined(FFXM_GPU)
#ifndef FFXM_PREFER_WAVE64
#define FFXM_PREFER_WAVE64
#endif // FFXM_PREFER_WAVE64

#if defined(FFXM_GPU)
//#pragma warning(disable: 3205)  // conversion from larger type to smaller
#endif // #if defined(FFXM_GPU)

#define DECLARE_SRV_REGISTER(regIndex)  t##regIndex
#define DECLARE_UAV_REGISTER(regIndex)  u##regIndex
#define DECLARE_CB_REGISTER(regIndex)   b##regIndex
#define FFXM_FSR2_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFXM_FSR2_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFXM_FSR2_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if FFXM_SHADER_PLATFORM_GLES_3_2
#define FFXM_UAV_RG_QUALIFIER FfxFloat32x4
#else
#define FFXM_UAV_RG_QUALIFIER FfxFloat32x2
#endif

#if defined(FSR2_BIND_CB_FSR2)
    cbuffer cbArmASR : FFXM_FSR2_DECLARE_CB(FSR2_BIND_CB_FSR2)
    {
        FfxInt32x2    iRenderSize;
        FfxInt32x2    iMaxRenderSize;
        FfxInt32x2    iDisplaySize;
        FfxInt32x2    iInputColorResourceDimensions;
        FfxInt32x2    iLumaMipDimensions;
        FfxInt32      iLumaMipLevelToUse;
        FfxInt32      iFrameIndex;

        FfxFloat32x4  fDeviceToViewDepth;
        FfxFloat32x2  fJitter;
        FfxFloat32x2  fMotionVectorScale;
        FfxFloat32x2  fDownscaleFactor;
        FfxFloat32x2  fMotionVectorJitterCancellation;
        FfxFloat32    fPreExposure;
        FfxFloat32    fPreviousFramePreExposure;
        FfxFloat32    fTanHalfFOV;
        FfxFloat32    fJitterSequenceLength;
        FfxFloat32    fDeltaTime;
        FfxFloat32    fDynamicResChangeFactor;
        FfxFloat32    fViewSpaceToMetersFactor;
    };

#define FFXM_FSR2_CONSTANT_BUFFER_1_SIZE (sizeof(cbArmASR) / 4)  // Number of 32-bit values. This must be kept in sync with the cbArmASR size.

/* Define getter functions in the order they are defined in the CB! */
FfxInt32x2 RenderSize()
{
    return iRenderSize;
}

FfxInt32x2 MaxRenderSize()
{
    return iMaxRenderSize;
}

FfxInt32x2 DisplaySize()
{
    return iDisplaySize;
}

FfxInt32x2 InputColorResourceDimensions()
{
    return iInputColorResourceDimensions;
}

FfxInt32x2 LumaMipDimensions()
{
    return iLumaMipDimensions;
}

FfxInt32  LumaMipLevelToUse()
{
    return iLumaMipLevelToUse;
}

FfxInt32 FrameIndex()
{
    return iFrameIndex;
}

FfxFloat32x2 Jitter()
{
    return fJitter;
}

FfxFloat32x4 DeviceToViewSpaceTransformFactors()
{
    return fDeviceToViewDepth;
}

FfxFloat32x2 MotionVectorScale()
{
    return fMotionVectorScale;
}

FfxFloat32x2 DownscaleFactor()
{
    return fDownscaleFactor;
}

FfxFloat32x2 MotionVectorJitterCancellation()
{
    return fMotionVectorJitterCancellation;
}

FfxFloat32 PreExposure()
{
    return fPreExposure;
}

FfxFloat32 PreviousFramePreExposure()
{
    return fPreviousFramePreExposure;
}

FfxFloat32 TanHalfFoV()
{
    return fTanHalfFOV;
}

FfxFloat32 JitterSequenceLength()
{
    return fJitterSequenceLength;
}

FfxFloat32 DeltaTime()
{
    return fDeltaTime;
}

FfxFloat32 DynamicResChangeFactor()
{
    return fDynamicResChangeFactor;
}

FfxFloat32 ViewSpaceToMetersFactor()
{
    return fViewSpaceToMetersFactor;
}
#endif // #if defined(FSR2_BIND_CB_FSR2)

#define FFXM_FSR2_ROOTSIG_STRINGIFY(p) FFXM_FSR2_ROOTSIG_STR(p)
#define FFXM_FSR2_ROOTSIG_STR(p) #p
#define FFXM_FSR2_ROOTSIG [RootSignature( "DescriptorTable(UAV(u0, numDescriptors = " FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "DescriptorTable(SRV(t0, numDescriptors = " FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "RootConstants(num32BitConstants=" FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_CONSTANT_BUFFER_1_SIZE) ", b0), " \
                                    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_POINT, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK), " \
                                    "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK)" )]

#define FFXM_FSR2_CONSTANT_BUFFER_2_SIZE 6  // Number of 32-bit values. This must be kept in sync with max( cbArmASRRCAS , cbArmASRSPD) size.

#define FFXM_FSR2_CB2_ROOTSIG [RootSignature( "DescriptorTable(UAV(u0, numDescriptors = " FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "DescriptorTable(SRV(t0, numDescriptors = " FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "RootConstants(num32BitConstants=" FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_CONSTANT_BUFFER_1_SIZE) ", b0), " \
                                    "RootConstants(num32BitConstants=" FFXM_FSR2_ROOTSIG_STRINGIFY(FFXM_FSR2_CONSTANT_BUFFER_2_SIZE) ", b1), " \
                                    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_POINT, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK), " \
                                    "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK)" )]
#if defined(FFXM_FSR2_EMBED_ROOTSIG)
#define FFXM_FSR2_EMBED_ROOTSIG_CONTENT FFXM_FSR2_ROOTSIG
#define FFXM_FSR2_EMBED_CB2_ROOTSIG_CONTENT FFXM_FSR2_CB2_ROOTSIG
#else
#define FFXM_FSR2_EMBED_ROOTSIG_CONTENT
#define FFXM_FSR2_EMBED_CB2_ROOTSIG_CONTENT
#endif // #if FFXM_FSR2_EMBED_ROOTSIG

#if defined(FSR2_BIND_CB_RCAS)
cbuffer cbArmASRRCAS : FFXM_FSR2_DECLARE_CB(FSR2_BIND_CB_RCAS)
{
    FfxUInt32x4 rcasConfig;
};

FfxUInt32x4 RCASConfig()
{
    return rcasConfig;
}
#endif // #if defined(FSR2_BIND_CB_RCAS)


#if defined(FSR2_BIND_CB_REACTIVE)
cbuffer cbGenerateReactive : FFXM_FSR2_DECLARE_CB(FSR2_BIND_CB_REACTIVE)
{
    FfxFloat32   gen_reactive_scale;
    FfxFloat32   gen_reactive_threshold;
    FfxFloat32   gen_reactive_binaryValue;
    FfxUInt32    gen_reactive_flags;
};

FfxFloat32 GenReactiveScale()
{
    return gen_reactive_scale;
}

FfxFloat32 GenReactiveThreshold()
{
    return gen_reactive_threshold;
}

FfxFloat32 GenReactiveBinaryValue()
{
    return gen_reactive_binaryValue;
}

FfxUInt32 GenReactiveFlags()
{
    return gen_reactive_flags;
}
#endif // #if defined(FSR2_BIND_CB_REACTIVE)

#if defined(FSR2_BIND_CB_SPD)
cbuffer cbArmASRSPD : FFXM_FSR2_DECLARE_CB(FSR2_BIND_CB_SPD) {

    FfxUInt32   mips;
    FfxUInt32   numWorkGroups;
    FfxUInt32x2 workGroupOffset;
    FfxUInt32x2 renderSize;
};

FfxUInt32 MipCount()
{
    return mips;
}

FfxUInt32 NumWorkGroups()
{
    return numWorkGroups;
}

FfxUInt32x2 WorkGroupOffset()
{
    return workGroupOffset;
}

FfxUInt32x2 SPD_RenderSize()
{
    return renderSize;
}
#endif // #if defined(FSR2_BIND_CB_SPD)

SamplerState s_PointClamp : register(s0);
SamplerState s_LinearClamp : register(s1);

    // SRVs
    #if defined FSR2_BIND_SRV_INPUT_COLOR
        Texture2D<FfxFloat32x4> r_input_color_jittered : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_INPUT_COLOR);
    #endif
    #if defined FSR2_BIND_SRV_INPUT_OPAQUE_ONLY
        Texture2D<FfxFloat32x4> r_input_opaque_only : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_INPUT_OPAQUE_ONLY);
    #endif
    #if defined FSR2_BIND_SRV_INPUT_MOTION_VECTORS
        Texture2D<FfxFloat32x4> r_input_motion_vectors : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_INPUT_MOTION_VECTORS);
    #endif
    #if defined FSR2_BIND_SRV_INPUT_DEPTH
        Texture2D<FfxFloat32> r_input_depth : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_INPUT_DEPTH);
    #endif
    #if defined FSR2_BIND_SRV_INPUT_EXPOSURE
        Texture2D<FfxFloat32x2> r_input_exposure : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_INPUT_EXPOSURE);
    #endif
    #if defined FSR2_BIND_SRV_AUTO_EXPOSURE
        Texture2D<FfxFloat32x2> r_auto_exposure : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_AUTO_EXPOSURE);
    #endif
    #if defined FSR2_BIND_SRV_REACTIVE_MASK
        Texture2D<FfxFloat32> r_reactive_mask : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_REACTIVE_MASK);
    #endif
    #if defined FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK
        Texture2D<FfxFloat32> r_transparency_and_composition_mask : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK);
    #endif
    #if defined FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH
        Texture2D<FfxUInt32> r_reconstructed_previous_nearest_depth : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH);
    #endif
    #if defined FSR2_BIND_SRV_DILATED_MOTION_VECTORS
       Texture2D<FfxFloat32x2> r_dilated_motion_vectors : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_DILATED_MOTION_VECTORS);
    #endif
    #if defined FSR2_BIND_SRV_PREVIOUS_DILATED_MOTION_VECTORS
        Texture2D<FfxFloat32x2> r_previous_dilated_motion_vectors : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_PREVIOUS_DILATED_MOTION_VECTORS);
    #endif
    #if defined FSR2_BIND_SRV_DILATED_DEPTH
        Texture2D<FfxFloat32> r_dilatedDepth : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_DILATED_DEPTH);
    #endif
    #if defined FSR2_BIND_SRV_INTERNAL_UPSCALED
        Texture2D<FfxFloat32x4> r_internal_upscaled_color : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_INTERNAL_UPSCALED);
    #endif
    #if defined FSR2_BIND_SRV_LOCK_STATUS
        Texture2D<unorm FfxFloat32x2> r_lock_status : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_LOCK_STATUS);
    #endif
    #if defined FSR2_BIND_SRV_LOCK_INPUT_LUMA
        Texture2D<FfxFloat32> r_lock_input_luma : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_LOCK_INPUT_LUMA);
    #endif
    #if defined FSR2_BIND_SRV_NEW_LOCKS
        Texture2D<unorm FfxFloat32> r_new_locks : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_NEW_LOCKS);
    #endif
    #if defined FSR2_BIND_SRV_PREPARED_INPUT_COLOR
        Texture2D<FfxFloat32x4> r_prepared_input_color : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_PREPARED_INPUT_COLOR);
    #endif
    #if defined FSR2_BIND_SRV_LUMA_HISTORY
        Texture2D<unorm FfxFloat32x4> r_luma_history : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_LUMA_HISTORY);
    #endif
    #if defined FSR2_BIND_SRV_RCAS_INPUT
        Texture2D<FfxFloat32x4> r_rcas_input : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_RCAS_INPUT);
    #endif
    #if defined FSR2_BIND_SRV_LANCZOS_LUT
        Texture2D<FfxFloat32> r_lanczos_lut : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_LANCZOS_LUT);
    #endif
    #if defined FSR2_BIND_SRV_SCENE_LUMINANCE_MIPS
        Texture2D<FfxFloat32> r_imgMips : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_SCENE_LUMINANCE_MIPS);
    #endif
    #if defined FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT
        Texture2D<FfxFloat32> r_upsample_maximum_bias_lut : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT);
    #endif
    #if defined FSR2_BIND_SRV_DILATED_REACTIVE_MASKS
        Texture2D<unorm FfxFloat32x2> r_dilated_reactive_masks : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_DILATED_REACTIVE_MASKS);
    #endif

    #if defined FSR2_BIND_SRV_TEMPORAL_REACTIVE
        Texture2D<FfxFloat32> r_internal_temporal_reactive : FFXM_FSR2_DECLARE_SRV(FSR2_BIND_SRV_TEMPORAL_REACTIVE);
    #endif

    // UAV declarations
    #if defined FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH
        RWTexture2D<FfxUInt32> rw_reconstructed_previous_nearest_depth : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH);
    #endif
    #if defined FSR2_BIND_UAV_DILATED_MOTION_VECTORS
        RWTexture2D<FfxFloat32x2> rw_dilated_motion_vectors : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_DILATED_MOTION_VECTORS);
    #endif
    #if defined FSR2_BIND_UAV_DILATED_DEPTH
        RWTexture2D<FfxFloat32> rw_dilatedDepth : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_DILATED_DEPTH);
    #endif
    #if defined FSR2_BIND_UAV_INTERNAL_UPSCALED
        RWTexture2D<FfxFloat32x4> rw_internal_upscaled_color : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_INTERNAL_UPSCALED);
    #endif
    #if defined FSR2_BIND_UAV_LOCK_STATUS
        RWTexture2D<unorm FfxFloat32x2> rw_lock_status : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_LOCK_STATUS);
    #endif
    #if defined FSR2_BIND_UAV_LOCK_INPUT_LUMA
        RWTexture2D<FfxFloat32> rw_lock_input_luma : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_LOCK_INPUT_LUMA);
    #endif
    #if defined FSR2_BIND_UAV_NEW_LOCKS
        RWTexture2D<unorm FfxFloat32> rw_new_locks : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_NEW_LOCKS);
    #endif
    #if defined FSR2_BIND_UAV_PREPARED_INPUT_COLOR
        RWTexture2D<FfxFloat32x4> rw_prepared_input_color : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_PREPARED_INPUT_COLOR);
    #endif
    #if defined FSR2_BIND_UAV_LUMA_HISTORY
        RWTexture2D<FfxFloat32x4> rw_luma_history : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_LUMA_HISTORY);
    #endif
    #if defined FSR2_BIND_UAV_UPSCALED_OUTPUT
        RWTexture2D<FfxFloat32x4> rw_upscaled_output : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_UPSCALED_OUTPUT);
    #endif
    #if defined FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE
        globallycoherent RWTexture2D<FfxFloat32> rw_img_mip_shading_change : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE);
    #endif
    #if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
        globallycoherent RWTexture2D<FfxFloat32> rw_img_mip_5 : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_EXPOSURE_MIP_5);
    #endif
    #if defined FSR2_BIND_UAV_DILATED_REACTIVE_MASKS
        RWTexture2D<unorm FfxFloat32x2> rw_dilated_reactive_masks : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_DILATED_REACTIVE_MASKS);
    #endif
    #if defined FSR2_BIND_UAV_EXPOSURE
        RWTexture2D<FFXM_UAV_RG_QUALIFIER> rw_exposure : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_EXPOSURE);
    #endif
    #if defined FSR2_BIND_UAV_AUTO_EXPOSURE
        RWTexture2D<FFXM_UAV_RG_QUALIFIER> rw_auto_exposure : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_AUTO_EXPOSURE);
    #endif
    #if defined FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC
        globallycoherent RWTexture2D<FfxUInt32> rw_spd_global_atomic : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC);
    #endif

    #if defined FSR2_BIND_UAV_AUTOREACTIVE
        RWTexture2D<float> rw_output_autoreactive : FFXM_FSR2_DECLARE_UAV(FSR2_BIND_UAV_AUTOREACTIVE);
    #endif

#if defined(FSR2_BIND_SRV_SCENE_LUMINANCE_MIPS)
FfxFloat32 LoadMipLuma(FfxUInt32x2 iPxPos, FfxUInt32 mipLevel)
{
    return r_imgMips.mips[mipLevel][iPxPos];
}
#endif

#if defined(FSR2_BIND_SRV_SCENE_LUMINANCE_MIPS)
FfxFloat32 SampleMipLuma(FfxFloat32x2 fUV, FfxUInt32 mipLevel)
{
    return r_imgMips.SampleLevel(s_LinearClamp, fUV, mipLevel);
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_DEPTH)
FfxFloat32 LoadInputDepth(FfxUInt32x2 iPxPos)
{
    return r_input_depth[iPxPos];
}
/*
   dd00 (-1,1)  *------* dd10 (0,-1)
                |      |
                |      |
   dd01 (-1,0)  *------* dd11 (0,0)
*/
void GatherInputDepthRQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FfxFloat32 dd00,
    FFXM_PARAMETER_INOUT FfxFloat32 dd10,
    FFXM_PARAMETER_INOUT FfxFloat32 dd01,
    FFXM_PARAMETER_INOUT FfxFloat32 dd11)
{
    FfxFloat32x4 rrrr = r_input_depth.GatherRed(s_PointClamp, fUV);
    dd01 = FfxFloat32(rrrr.x);
    dd11 = FfxFloat32(rrrr.y);
    dd10 = FfxFloat32(rrrr.z);
    dd00 = FfxFloat32(rrrr.w);
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_DEPTH)
FfxFloat32 SampleInputDepth(FfxFloat32x2 fUV)
{
    return r_input_depth.SampleLevel(s_LinearClamp, fUV, 0).x;
}
#endif

#if defined(FSR2_BIND_SRV_REACTIVE_MASK)
FfxFloat32 LoadReactiveMask(FfxUInt32x2 iPxPos)
{
    return r_reactive_mask[iPxPos];
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherReactiveRQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col11)
{
    FFXM_MIN16_F4 rrrr = r_reactive_mask.GatherRed(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F(rrrr.x);
    col11 = FFXM_MIN16_F(rrrr.y);
    col10 = FFXM_MIN16_F(rrrr.z);
    col00 = FFXM_MIN16_F(rrrr.w);
}
#endif

#if defined(FSR2_BIND_SRV_TRANSPARENCY_AND_COMPOSITION_MASK)
FfxFloat32 LoadTransparencyAndCompositionMask(FfxUInt32x2 iPxPos)
{
    return r_transparency_and_composition_mask[iPxPos];
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherTransparencyAndCompositionMaskRQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col11)
{
    FFXM_MIN16_F4 rrrr = r_transparency_and_composition_mask.GatherRed(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F(rrrr.x);
    col11 = FFXM_MIN16_F(rrrr.y);
    col10 = FFXM_MIN16_F(rrrr.z);
    col00 = FFXM_MIN16_F(rrrr.w);
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_COLOR)
FFXM_MIN16_F3 LoadInputColor(FfxUInt32x2 iPxPos)
{
    return r_input_color_jittered[iPxPos].rgb;
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherInputColorRGBQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col11)
{
    FFXM_MIN16_F4 rrrr = r_input_color_jittered.GatherRed(s_PointClamp, fUV);
    FFXM_MIN16_F4 gggg = r_input_color_jittered.GatherGreen(s_PointClamp, fUV);
    FFXM_MIN16_F4 bbbb = r_input_color_jittered.GatherBlue(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F3(rrrr.x, gggg.x, bbbb.x);
    col11 = FFXM_MIN16_F3(rrrr.y, gggg.y, bbbb.y);
    col10 = FFXM_MIN16_F3(rrrr.z, gggg.z, bbbb.z);
    col00 = FFXM_MIN16_F3(rrrr.w, gggg.w, bbbb.w);
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_COLOR)
FFXM_MIN16_F3 SampleInputColor(FfxFloat32x2 fUV)
{
    return r_input_color_jittered.SampleLevel(s_LinearClamp, fUV, 0).rgb;
}
#endif

#if defined(FSR2_BIND_SRV_PREPARED_INPUT_COLOR)
FFXM_MIN16_F3 LoadPreparedInputColor(FfxUInt32x2 iPxPos)
{
    return r_prepared_input_color[iPxPos].xyz;
}
FFXM_MIN16_F3 SamplePreparedInputColor(FfxFloat32x2 fUV)
{
    return r_prepared_input_color.SampleLevel(s_PointClamp, fUV, 0).xyz;
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherPreparedInputColorRGBQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F3 col11)
{
    FFXM_MIN16_F4 rrrr = r_prepared_input_color.GatherRed(s_PointClamp, fUV);
    FFXM_MIN16_F4 gggg = r_prepared_input_color.GatherGreen(s_PointClamp, fUV);
    FFXM_MIN16_F4 bbbb = r_prepared_input_color.GatherBlue(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F3(rrrr.x, gggg.x, bbbb.x);
    col11 = FFXM_MIN16_F3(rrrr.y, gggg.y, bbbb.y);
    col10 = FFXM_MIN16_F3(rrrr.z, gggg.z, bbbb.z);
    col00 = FFXM_MIN16_F3(rrrr.w, gggg.w, bbbb.w);
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_MOTION_VECTORS)
FFXM_MIN16_F2 LoadInputMotionVector(FfxUInt32x2 iPxDilatedMotionVectorPos)
{
    FFXM_MIN16_F2 fSrcMotionVector = r_input_motion_vectors[iPxDilatedMotionVectorPos].xy;

    FFXM_MIN16_F2 fUvMotionVector = fSrcMotionVector * MotionVectorScale();

#if FFXM_FSR2_OPTION_JITTERED_MOTION_VECTORS
    fUvMotionVector -= MotionVectorJitterCancellation();
#endif

    return fUvMotionVector;
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherInputMotionVectorRGQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F2 col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F2 col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F2 col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F2 col11)
{
    FFXM_MIN16_F4 rrrr = r_input_motion_vectors.GatherRed(s_PointClamp, fUV);
    FFXM_MIN16_F4 gggg = r_input_motion_vectors.GatherGreen(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F2(rrrr.x, gggg.x) * MotionVectorScale();
    col11 = FFXM_MIN16_F2(rrrr.y, gggg.y) * MotionVectorScale();
    col10 = FFXM_MIN16_F2(rrrr.z, gggg.z) * MotionVectorScale();
    col00 = FFXM_MIN16_F2(rrrr.w, gggg.w) * MotionVectorScale();
#if FFXM_FSR2_OPTION_JITTERED_MOTION_VECTORS
    col01 -= MotionVectorJitterCancellation();
    col11 -= MotionVectorJitterCancellation();
    col10 -= MotionVectorJitterCancellation();
    col00 -= MotionVectorJitterCancellation();
#endif
}
#endif

#if defined(FSR2_BIND_SRV_INTERNAL_UPSCALED)
FFXM_MIN16_F4 LoadHistory(FfxUInt32x2 iPxHistory)
{
    return r_internal_upscaled_color[iPxHistory];
}
FFXM_MIN16_F4 SampleUpscaledHistory(FfxFloat32x2 fUV)
{
    return r_internal_upscaled_color.SampleLevel(s_LinearClamp, fUV, 0);
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherHistoryColorRGBQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F4 col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F4 col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F4 col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F4 col11)
{
    FFXM_MIN16_F4 rrrr = r_internal_upscaled_color.GatherRed(s_PointClamp, fUV);
    FFXM_MIN16_F4 gggg = r_internal_upscaled_color.GatherGreen(s_PointClamp, fUV);
    FFXM_MIN16_F4 bbbb = r_internal_upscaled_color.GatherBlue(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F4(rrrr.x, gggg.x, bbbb.x, 0.0f);
    col11 = FFXM_MIN16_F4(rrrr.y, gggg.y, bbbb.y, 0.0f);
    col10 = FFXM_MIN16_F4(rrrr.z, gggg.z, bbbb.z, 0.0f);
    col00 = FFXM_MIN16_F4(rrrr.w, gggg.w, bbbb.w, 0.0f);
}
#endif

#if defined(FSR2_BIND_UAV_LUMA_HISTORY)
void StoreLumaHistory(FfxUInt32x2 iPxPos, FfxFloat32x4 fLumaHistory)
{
    rw_luma_history[iPxPos] = fLumaHistory;
}
#endif

#if defined(FSR2_BIND_SRV_LUMA_HISTORY)
FFXM_MIN16_F4 SampleLumaHistory(FfxFloat32x2 fUV)
{
    return r_luma_history.SampleLevel(s_LinearClamp, fUV, 0);
}
#endif

FFXM_MIN16_F4 LoadRCAS_Input(FfxInt32x2 iPxPos)
{
#if defined(FSR2_BIND_SRV_RCAS_INPUT)
    return r_rcas_input.Load(FfxInt32x3(iPxPos, 0));
#else
    return 0.0;
#endif
}

#if defined(FSR2_BIND_UAV_INTERNAL_UPSCALED)
void StoreReprojectedHistory(FfxUInt32x2 iPxHistory, FfxFloat32x4 fHistory)
{
    rw_internal_upscaled_color[iPxHistory] = fHistory;
}
#endif

#if defined(FSR2_BIND_UAV_INTERNAL_UPSCALED)
void StoreInternalColorAndWeight(FfxUInt32x2 iPxPos, FfxFloat32x4 fColorAndWeight)
{
    rw_internal_upscaled_color[iPxPos] = fColorAndWeight;
}
#endif

#if defined(FSR2_BIND_UAV_UPSCALED_OUTPUT)
void StoreUpscaledOutput(FfxUInt32x2 iPxPos, FfxFloat32x3 fColor)
{
    rw_upscaled_output[iPxPos] = FfxFloat32x4(fColor, 1.f);
}
#endif

//LOCK_LIFETIME_REMAINING == 0
//Should make LockInitialLifetime() return a const 1.0f later
#if defined(FSR2_BIND_SRV_LOCK_STATUS)
FfxFloat32x2 LoadLockStatus(FfxUInt32x2 iPxPos)
{
    return r_lock_status[iPxPos];
}
#endif

#if defined(FSR2_BIND_UAV_LOCK_STATUS)
void StoreLockStatus(FfxUInt32x2 iPxPos, FfxFloat32x2 fLockStatus)
{
    rw_lock_status[iPxPos] = fLockStatus;
}
#endif

#if defined(FSR2_BIND_SRV_LOCK_INPUT_LUMA)
FFXM_MIN16_F LoadLockInputLuma(FfxUInt32x2 iPxPos)
{
    return r_lock_input_luma[iPxPos];
}
/*
   col00 (-1,1) *------* col10 (0,-1)
                |      |
                |      |
   col01 (-1,0) *------* col11 (0,0)
*/
void GatherLockInputLumaRQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col00,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col10,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col01,
    FFXM_PARAMETER_INOUT FFXM_MIN16_F col11)
{
    FFXM_MIN16_F4 rrrr = r_lock_input_luma.GatherRed(s_PointClamp, fUV);
    col01 = FFXM_MIN16_F(rrrr.x);
    col11 = FFXM_MIN16_F(rrrr.y);
    col10 = FFXM_MIN16_F(rrrr.z);
    col00 = FFXM_MIN16_F(rrrr.w);
}
#endif

#if defined(FSR2_BIND_SRV_NEW_LOCKS)
FfxFloat32 LoadNewLocks(FfxUInt32x2 iPxPos)
{
    return r_new_locks[iPxPos];
}
#endif

#if defined(FSR2_BIND_UAV_NEW_LOCKS)
FFXM_MIN16_F LoadRwNewLocks(FfxUInt32x2 iPxPos)
{
    return rw_new_locks[iPxPos];
}
#endif

#if defined(FSR2_BIND_UAV_NEW_LOCKS)
void StoreNewLocks(FfxUInt32x2 iPxPos, FfxFloat32 newLock)
{
    rw_new_locks[iPxPos] = newLock;
}
#endif

#if defined(FSR2_BIND_SRV_PREPARED_INPUT_COLOR)
FfxFloat32 SampleDepthClip(FfxFloat32x2 fUV)
{
    return r_prepared_input_color.SampleLevel(s_LinearClamp, fUV, 0).w;
}
#endif

#if defined(FSR2_BIND_SRV_LOCK_STATUS)
FFXM_MIN16_F2 SampleLockStatus(FfxFloat32x2 fUV)
{
    FFXM_MIN16_F2 fLockStatus = r_lock_status.SampleLevel(s_LinearClamp, fUV, 0);
    return fLockStatus;
}
#endif

#if defined(FSR2_BIND_SRV_RECONSTRUCTED_PREV_NEAREST_DEPTH)
FfxFloat32 LoadReconstructedPrevDepth(FfxUInt32x2 iPxPos)
{
    return asfloat(r_reconstructed_previous_nearest_depth[iPxPos]);
}
/*
   d00 (-1,1) *------* d10 (0,-1)
              |      |
              |      |
   d01 (-1,0) *------* d11 (0,0)
*/
void GatherReconstructedPreviousDepthRQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FfxFloat32 d00,
    FFXM_PARAMETER_INOUT FfxFloat32 d10,
    FFXM_PARAMETER_INOUT FfxFloat32 d01,
    FFXM_PARAMETER_INOUT FfxFloat32 d11)
{
    FfxUInt32x4 rrrr = r_reconstructed_previous_nearest_depth.GatherRed(s_PointClamp, fUV);
    d01 = FfxFloat32(asfloat(rrrr.x));
    d11 = FfxFloat32(asfloat(rrrr.y));
    d10 = FfxFloat32(asfloat(rrrr.z));
    d00 = FfxFloat32(asfloat(rrrr.w));
}
#endif

#if defined(FSR2_BIND_UAV_RECONSTRUCTED_PREV_NEAREST_DEPTH)
void StoreReconstructedDepth(FfxUInt32x2 iPxSample, FfxFloat32 fDepth)
{
    FfxUInt32 uDepth = asuint(fDepth);

    #if FFXM_FSR2_OPTION_INVERTED_DEPTH
        InterlockedMax(rw_reconstructed_previous_nearest_depth[iPxSample], uDepth);
    #else
        InterlockedMin(rw_reconstructed_previous_nearest_depth[iPxSample], uDepth); // min for standard, max for inverted depth
    #endif
}
#endif

#if defined(FSR2_BIND_SRV_DILATED_MOTION_VECTORS)
FFXM_MIN16_F2 LoadDilatedMotionVector(FfxUInt32x2 iPxInput)
{
    return r_dilated_motion_vectors[iPxInput].xy;
}
#endif

#if defined(FSR2_BIND_SRV_PREVIOUS_DILATED_MOTION_VECTORS)
FFXM_MIN16_F2 LoadPreviousDilatedMotionVector(FfxUInt32x2 iPxInput)
{
    return r_previous_dilated_motion_vectors[iPxInput].xy;
}

FFXM_MIN16_F2 SamplePreviousDilatedMotionVector(FfxFloat32x2 uv)
{
    return r_previous_dilated_motion_vectors.SampleLevel(s_LinearClamp, uv, 0).xy;
}
#endif

#if defined(FSR2_BIND_SRV_DILATED_DEPTH)
FfxFloat32 LoadDilatedDepth(FfxUInt32x2 iPxInput)
{
    return r_dilatedDepth[iPxInput];
}
/*
   dd00 (-1,1)  *------* dd10 (0,-1)
                |      |
                |      |
   dd01 (-1,0)  *------* dd11 (0,0)
*/
void GatherDilatedDepthRQuad(FfxFloat32x2 fUV,
    FFXM_PARAMETER_INOUT FfxFloat32 dd00,
    FFXM_PARAMETER_INOUT FfxFloat32 dd10,
    FFXM_PARAMETER_INOUT FfxFloat32 dd01,
    FFXM_PARAMETER_INOUT FfxFloat32 dd11)
{
    FfxFloat32x4 rrrr = r_dilatedDepth.GatherRed(s_PointClamp, fUV);
    dd01 = FfxFloat32(rrrr.x);
    dd11 = FfxFloat32(rrrr.y);
    dd10 = FfxFloat32(rrrr.z);
    dd00 = FfxFloat32(rrrr.w);
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_EXPOSURE)
FfxFloat32 Exposure()
{
    FfxFloat32 exposure = r_input_exposure[FfxUInt32x2(0, 0)].x;

    if (exposure == 0.0f) {
        exposure = 1.0f;
    }

    return exposure;
}
#endif

#if defined(FSR2_BIND_SRV_AUTO_EXPOSURE)
FfxFloat32 AutoExposure()
{
    FfxFloat32 exposure = r_auto_exposure[FfxUInt32x2(0, 0)].x;

    if (exposure == 0.0f) {
        exposure = 1.0f;
    }

    return exposure;
}
#endif

FfxFloat32 SampleLanczos2Weight(FfxFloat32 x)
{
#if defined(FSR2_BIND_SRV_LANCZOS_LUT)
    return r_lanczos_lut.SampleLevel(s_LinearClamp, FfxFloat32x2(x / 2, 0.5f), 0);
#else
    return 0.f;
#endif
}

#if defined(FSR2_BIND_SRV_UPSCALE_MAXIMUM_BIAS_LUT)
FfxFloat32 SampleUpsampleMaximumBias(FfxFloat32x2 uv)
{
    // Stored as a SNORM, so make sure to multiply by 2 to retrieve the actual expected range.
    return FfxFloat32(2.0) * r_upsample_maximum_bias_lut.SampleLevel(s_LinearClamp, abs(uv) * 2.0, 0);
}
#endif

#if defined(FSR2_BIND_SRV_TEMPORAL_REACTIVE)
FfxFloat32 SampleTemporalReactive(FfxFloat32x2 fUV)
{
    return r_internal_temporal_reactive.SampleLevel(s_LinearClamp, fUV, 0);
}
#endif

#if defined(FSR2_BIND_SRV_DILATED_REACTIVE_MASKS)
FFXM_MIN16_F2 SampleDilatedReactiveMasks(FfxFloat32x2 fUV)
{
    return r_dilated_reactive_masks.SampleLevel(s_LinearClamp, fUV, 0);
}
#endif

#if defined(FSR2_BIND_SRV_DILATED_REACTIVE_MASKS)
FFXM_MIN16_F2 LoadDilatedReactiveMasks(FFXM_PARAMETER_IN FfxUInt32x2 iPxPos)
{
    return r_dilated_reactive_masks[iPxPos];
}
#endif

#if defined(FSR2_BIND_SRV_INPUT_OPAQUE_ONLY)
FfxFloat32x3 LoadOpaqueOnly(FFXM_PARAMETER_IN FFXM_MIN16_I2 iPxPos)
{
    return r_input_opaque_only[iPxPos].xyz;
}
#endif

FfxFloat32x2 SPD_LoadExposureBuffer()
{
#if defined FSR2_BIND_UAV_AUTO_EXPOSURE
    return rw_auto_exposure[FfxInt32x2(0, 0)];
#else
    return FfxFloat32x2(0.f, 0.f);
#endif // #if defined FSR2_BIND_UAV_AUTO_EXPOSURE
}

void SPD_SetExposureBuffer(FfxFloat32x2 value)
{
#if defined FSR2_BIND_UAV_AUTO_EXPOSURE
    #if FFXM_SHADER_PLATFORM_GLES_3_2
    rw_auto_exposure[FfxInt32x2(0, 0)] = FfxInt32x4(value, 0.0f, 0.0f);
#else
    rw_auto_exposure[FfxInt32x2(0, 0)] = value;
#endif
#endif // #if defined FSR2_BIND_UAV_AUTO_EXPOSURE
}

FfxFloat32x4 SPD_LoadMipmap5(FfxInt32x2 iPxPos)
{
#if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
    return FfxFloat32x4(rw_img_mip_5[iPxPos], 0, 0, 0);
#else
    return FfxFloat32x4(0.f, 0.f, 0.f, 0.f);
#endif // #if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
}

void SPD_SetMipmap(FfxInt32x2 iPxPos, FfxUInt32 slice, FfxFloat32 value)
{
    switch (slice)
    {
    case FFXM_FSR2_SHADING_CHANGE_MIP_LEVEL:
#if defined FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE
        rw_img_mip_shading_change[iPxPos] = value;
#endif // #if defined FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE
        break;
    case 5:
#if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
        rw_img_mip_5[iPxPos] = value;
#endif // #if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
        break;
    default:

        // avoid flattened side effect
#if defined(FSR2_BIND_UAV_EXPOSURE_MIP_LUMA_CHANGE)
        rw_img_mip_shading_change[iPxPos] = rw_img_mip_shading_change[iPxPos];
#elif defined(FSR2_BIND_UAV_EXPOSURE_MIP_5)
        rw_img_mip_5[iPxPos] = rw_img_mip_5[iPxPos];
#endif // #if defined FSR2_BIND_UAV_EXPOSURE_MIP_5
        break;
    }
}

void SPD_IncreaseAtomicCounter(inout FfxUInt32 spdCounter)
{
#if defined FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC
    InterlockedAdd(rw_spd_global_atomic[FfxInt32x2(0, 0)], 1, spdCounter);
#endif // #if defined FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC
}

void SPD_ResetAtomicCounter()
{
#if defined FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC
    rw_spd_global_atomic[FfxInt32x2(0, 0)] = 0;
#endif // #if defined FSR2_BIND_UAV_SPD_GLOBAL_ATOMIC
}

#endif // #if defined(FFXM_GPU)
