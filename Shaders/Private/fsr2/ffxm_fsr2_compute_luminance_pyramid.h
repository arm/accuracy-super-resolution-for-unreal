// Copyright  © 2023 Advanced Micro Devices, Inc.
// Copyright © 2024 Arm Limited.
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

FFXM_GROUPSHARED FfxUInt32 spdCounter;

void SpdIncreaseAtomicCounter(FfxUInt32 slice)
{
    SPD_IncreaseAtomicCounter(spdCounter);
}

FfxUInt32 SpdGetAtomicCounter()
{
    return spdCounter;
}

void SpdResetAtomicCounter(FfxUInt32 slice)
{
    SPD_ResetAtomicCounter();
}

#ifndef SPD_PACKED_ONLY
FFXM_GROUPSHARED FfxFloat32 spdIntermediateR[16][16];
FFXM_GROUPSHARED FfxFloat32 spdIntermediateG[16][16];
FFXM_GROUPSHARED FfxFloat32 spdIntermediateB[16][16];
FFXM_GROUPSHARED FfxFloat32 spdIntermediateA[16][16];

FfxFloat32x4 SpdLoadSourceImage(FfxFloat32x2 tex, FfxUInt32 slice)
{
    FfxFloat32x2 fUv = (tex + 0.5f + Jitter()) / RenderSize();
    fUv = ClampUv(fUv, RenderSize(), InputColorResourceDimensions());
    FfxFloat32x3 fRgb = SampleInputColor(fUv);

    fRgb /= PreExposure();

    //compute log luma
    const FfxFloat32 fLogLuma = log(ffxMax(FSR2_EPSILON, RGBToLuma(fRgb)));

    // Make sure out of screen pixels contribute no value to the end result
    const FfxFloat32 result = all(FFXM_LESS_THAN(tex, RenderSize())) ? fLogLuma : 0.0f;

    return FfxFloat32x4(result, 0, 0, 0);
}

FfxFloat32x4 SpdLoad(FfxInt32x2 tex, FfxUInt32 slice)
{
    return SPD_LoadMipmap5(tex);
}

void SpdStore(FfxInt32x2 pix, FfxFloat32x4 outValue, FfxUInt32 index, FfxUInt32 slice)
{
    if (index == LumaMipLevelToUse() || index == 5)
    {
        SPD_SetMipmap(pix, index, outValue.r);
    }

    if (index == MipCount() - 1) { //accumulate on 1x1 level

        if (all(FFXM_EQUAL(pix, FfxInt32x2(0, 0))))
        {
            FfxFloat32 prev = SPD_LoadExposureBuffer().y;
            FfxFloat32 result = outValue.r;

            if (prev < resetAutoExposureAverageSmoothing) // Compare Lavg, so small or negative values
            {
                FfxFloat32 rate = 1.0f;
                result = prev + (result - prev) * (1 - exp(-DeltaTime() * rate));
            }
            FfxFloat32x2 spdOutput = FfxFloat32x2(ComputeAutoExposureFromLavg(result), result);
            SPD_SetExposureBuffer(spdOutput);
        }
    }
}

FfxFloat32x4 SpdLoadIntermediate(FfxUInt32 x, FfxUInt32 y)
{
    return FfxFloat32x4(
        spdIntermediateR[x][y],
        spdIntermediateG[x][y],
        spdIntermediateB[x][y],
        spdIntermediateA[x][y]);
}
void SpdStoreIntermediate(FfxUInt32 x, FfxUInt32 y, FfxFloat32x4 value)
{
    spdIntermediateR[x][y] = value.x;
    spdIntermediateG[x][y] = value.y;
    spdIntermediateB[x][y] = value.z;
    spdIntermediateA[x][y] = value.w;
}
FfxFloat32x4 SpdReduce4(FfxFloat32x4 v0, FfxFloat32x4 v1, FfxFloat32x4 v2, FfxFloat32x4 v3)
{
    return (v0 + v1 + v2 + v3) * 0.25f;
}
#endif

// define fetch and store functions Packed
#if FFXM_HALF

FFXM_GROUPSHARED FfxFloat16x2 spdIntermediateRG[16][16];
FFXM_GROUPSHARED FfxFloat16x2 spdIntermediateBA[16][16];

FfxFloat16x4 SpdLoadSourceImageH(FfxFloat32x2 tex, FfxUInt32 slice)
{
    FfxFloat16x2 fUv = FfxFloat16x2((tex + 0.5f + Jitter()) / RenderSize());
    fUv = FfxFloat16x2(ClampUv(fUv, RenderSize(), InputColorResourceDimensions()));
    FfxFloat16x3 fRgb = FfxFloat16x3(SampleInputColor(fUv));

    fRgb /= FfxFloat16(PreExposure());

    //compute log luma
    const FfxFloat16 fLogLuma = FfxFloat16(log(ffxMax(FSR2_EPSILON, RGBToLuma(fRgb))));

    // Make sure out of screen pixels contribute no value to the end result
    const FfxFloat16 result = all(FFXM_LESS_THAN(tex, RenderSize())) ? fLogLuma : FfxFloat16(0.0f);

    return FfxFloat16x4(result, 0, 0, 0);
}

FfxFloat16x4 SpdLoadH(FfxInt32x2 p, FfxUInt32 slice)
{
    return FfxFloat16x4(SPD_LoadMipmap5(p));
}

void SpdStoreH(FfxInt32x2 pix, FfxFloat16x4 outValue, FfxUInt32 index, FfxUInt32 slice)
{
    if (index == LumaMipLevelToUse() || index == 5)
    {
        SPD_SetMipmap(pix, index, outValue.r);
    }

    if (index == MipCount() - 1) { //accumulate on 1x1 level

        if (all(FFXM_EQUAL(pix, FfxInt16x2(0, 0))))
        {
            FfxFloat16 result = outValue.r;

            // Remove the smooth exposure transision with GLES 3.2
#if !FFXM_SHADER_PLATFORM_GLES_3_2
            FfxFloat16 prev = FfxFloat16(SPD_LoadExposureBuffer().y);
            if (prev < resetAutoExposureAverageSmoothing) // Compare Lavg, so small or negative values
            {
                FfxFloat16 rate = FfxFloat16(1.0f);
                result = FfxFloat16(prev + (result - prev) * (1 - exp(-DeltaTime() * rate)));
            }
#endif
            FfxFloat16x2 spdOutput = FfxFloat16x2(ComputeAutoExposureFromLavg(result), result);
            SPD_SetExposureBuffer(spdOutput);
        }
    }
}

FfxFloat16x4 SpdLoadIntermediateH(FfxUInt32 x, FfxUInt32 y)
{
    return FfxFloat16x4(
        spdIntermediateRG[x][y].x,
        spdIntermediateRG[x][y].y,
        spdIntermediateBA[x][y].x,
        spdIntermediateBA[x][y].y);
}

void SpdStoreIntermediateH(FfxUInt32 x, FfxUInt32 y, FfxFloat16x4 value)
{
    spdIntermediateRG[x][y] = value.xy;
    spdIntermediateBA[x][y] = value.zw;
}

FfxFloat16x4 SpdReduce4H(FfxFloat16x4 v0, FfxFloat16x4 v1, FfxFloat16x4 v2, FfxFloat16x4 v3)
{
    return (v0 + v1 + v2 + v3) * FfxFloat16(0.25);
}
#endif

#include "ffxm_spd.h"

void ComputeAutoExposure(FfxUInt32x3 WorkGroupId, FfxUInt32 LocalThreadIndex)
{
#if FFXM_HALF
    SpdDownsampleH(
        FfxUInt32x2(WorkGroupId.xy),
        FfxUInt32(LocalThreadIndex),
        FfxUInt32(MipCount()),
        FfxUInt32(NumWorkGroups()),
        FfxUInt32(WorkGroupId.z),
        FfxUInt32x2(WorkGroupOffset()));
#else
    SpdDownsample(
        FfxUInt32x2(WorkGroupId.xy),
        FfxUInt32(LocalThreadIndex),
        FfxUInt32(MipCount()),
        FfxUInt32(NumWorkGroups()),
        FfxUInt32(WorkGroupId.z),
        FfxUInt32x2(WorkGroupOffset()));
#endif
}
