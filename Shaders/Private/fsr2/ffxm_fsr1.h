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

/// @defgroup FfxGPUFsr1 FidelityFX FSR1
/// FidelityFX Super Resolution 1 GPU documentation
///
/// @ingroup FfxGPUEffects

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                                      FSR - [RCAS] ROBUST CONTRAST ADAPTIVE SHARPENING
//
//------------------------------------------------------------------------------------------------------------------------------
// CAS uses a simplified mechanism to convert local contrast into a variable amount of sharpness.
// RCAS uses a more exact mechanism, solving for the maximum local sharpness possible before clipping.
// RCAS also has a built in process to limit sharpening of what it detects as possible noise.
// RCAS sharper does not support scaling, as it should be applied after EASU scaling.
// Pass EASU output straight into RCAS, no color conversions necessary.
//------------------------------------------------------------------------------------------------------------------------------
// RCAS is based on the following logic.
// RCAS uses a 5 tap filter in a cross pattern (same as CAS),
//    w                n
//  w 1 w  for taps  w m e
//    w                s
// Where 'w' is the negative lobe weight.
//  output = (w*(n+e+w+s)+m)/(4*w+1)
// RCAS solves for 'w' by seeing where the signal might clip out of the {0 to 1} input range,
//  0 == (w*(n+e+w+s)+m)/(4*w+1) -> w = -m/(n+e+w+s)
//  1 == (w*(n+e+w+s)+m)/(4*w+1) -> w = (1-m)/(n+e+w+s-4*1)
// Then chooses the 'w' which results in no clipping, limits 'w', and multiplies by the 'sharp' amount.
// This solution above has issues with MSAA input as the steps along the gradient cause edge detection issues.
// So RCAS uses 4x the maximum and 4x the minimum (depending on equation)in place of the individual taps.
// As well as switching from 'm' to either the minimum or maximum (depending on side), to help in energy conservation.
// This stabilizes RCAS.
// RCAS does a simple highpass which is normalized against the local contrast then shaped,
//       0.25
//  0.25  -1  0.25
//       0.25
// This is used as a noise detection filter, to reduce the effect of RCAS on grain, and focus on real edges.
//
//  GLSL example for the required callbacks :
//
//  FfxFloat16x4 FsrRcasLoadH(FfxInt16x2 p){return FfxFloat16x4(imageLoad(imgSrc,FfxInt32x2(p)));}
//  void FsrRcasInputH(inout FfxFloat16 r,inout FfxFloat16 g,inout FfxFloat16 b)
//  {
//    //do any simple input color conversions here or leave empty if none needed
//  }
//
//  FsrRcasCon need to be called from the CPU or GPU to set up constants.
//  Including a GPU example here, the 'con' value would be stored out to a constant buffer.
//
//  FfxUInt32x4 con;
//  FsrRcasCon(con,
//   0.0); // The scale is {0.0 := maximum sharpness, to N>0, where N is the number of stops (halving) of the reduction of sharpness}.
// ---------------
// RCAS sharpening supports a CAS-like pass-through alpha via,
//  #define FSR_RCAS_PASSTHROUGH_ALPHA 1
// RCAS also supports a define to enable a more expensive path to avoid some sharpening of noise.
// Would suggest it is better to apply film grain after RCAS sharpening (and after scaling) instead of using this define,
//  #define FSR_RCAS_DENOISE 1
//==============================================================================================================================
// This is set at the limit of providing unnatural results for sharpening.
#define FSR_RCAS_LIMIT (0.25-(1.0/16.0))
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                      CONSTANT SETUP
//==============================================================================================================================
// Call to setup required constant values (works on CPU or GPU).
 FFXM_STATIC void FsrRcasCon(FfxUInt32x4 con,
                            // The scale is {0.0 := maximum, to N>0, where N is the number of stops (halving) of the reduction of sharpness}.
                            FfxFloat32 sharpness)
 {
     // Transform from stops to linear value.
     sharpness = exp2(-sharpness);
     FfxFloat32x2 hSharp  = {sharpness, sharpness};
     con[0] = ffxAsUInt32(sharpness);
     con[1] = packHalf2x16(hSharp);
     con[2] = 0;
     con[3] = 0;
 }
 ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                   NON-PACKED 32-BIT VERSION
//==============================================================================================================================
#if defined(FFXM_GPU)&&defined(FSR_RCAS_F)
 // Input callback prototypes that need to be implemented by calling shader
 FfxFloat32x4 FsrRcasLoadF(FfxInt32x2 p);
 void FsrRcasInputF(inout FfxFloat32 r,inout FfxFloat32 g,inout FfxFloat32 b);
//------------------------------------------------------------------------------------------------------------------------------
 void FsrRcasF(out FfxFloat32 pixR,  // Output values, non-vector so port between RcasFilter() and RcasFilterH() is easy.
               out FfxFloat32 pixG,
               out FfxFloat32 pixB,
#ifdef FSR_RCAS_PASSTHROUGH_ALPHA
               out FfxFloat32 pixA,
#endif
               FfxUInt32x2 ip,  // Integer pixel position in output.
               FfxUInt32x4 con)
 {  // Constant generated by RcasSetup().
     // Algorithm uses minimal 3x3 pixel neighborhood.
     //    b
     //  d e f
     //    h
     FfxInt32x2   sp = FfxInt32x2(ip);
     FfxFloat32x3 b  = FsrRcasLoadF(sp + FfxInt32x2(0, -1)).rgb;
     FfxFloat32x3 d  = FsrRcasLoadF(sp + FfxInt32x2(-1, 0)).rgb;
#ifdef FSR_RCAS_PASSTHROUGH_ALPHA
     FfxFloat32x4 ee = FsrRcasLoadF(sp);
     FfxFloat32x3 e  = ee.rgb;
     pixA            = ee.a;
#else
     FfxFloat32x3 e = FsrRcasLoadF(sp).rgb;
#endif
     FfxFloat32x3 f = FsrRcasLoadF(sp + FfxInt32x2(1, 0)).rgb;
     FfxFloat32x3 h = FsrRcasLoadF(sp + FfxInt32x2(0, 1)).rgb;
     // Rename (32-bit) or regroup (16-bit).
     FfxFloat32 bR = b.r;
     FfxFloat32 bG = b.g;
     FfxFloat32 bB = b.b;
     FfxFloat32 dR = d.r;
     FfxFloat32 dG = d.g;
     FfxFloat32 dB = d.b;
     FfxFloat32 eR = e.r;
     FfxFloat32 eG = e.g;
     FfxFloat32 eB = e.b;
     FfxFloat32 fR = f.r;
     FfxFloat32 fG = f.g;
     FfxFloat32 fB = f.b;
     FfxFloat32 hR = h.r;
     FfxFloat32 hG = h.g;
     FfxFloat32 hB = h.b;
     // Run optional input transform.
     FsrRcasInputF(bR, bG, bB);
     FsrRcasInputF(dR, dG, dB);
     FsrRcasInputF(eR, eG, eB);
     FsrRcasInputF(fR, fG, fB);
     FsrRcasInputF(hR, hG, hB);
     // Luma times 2.
     FfxFloat32 bL = bB * FfxFloat32(0.5) + (bR * FfxFloat32(0.5) + bG);
     FfxFloat32 dL = dB * FfxFloat32(0.5) + (dR * FfxFloat32(0.5) + dG);
     FfxFloat32 eL = eB * FfxFloat32(0.5) + (eR * FfxFloat32(0.5) + eG);
     FfxFloat32 fL = fB * FfxFloat32(0.5) + (fR * FfxFloat32(0.5) + fG);
     FfxFloat32 hL = hB * FfxFloat32(0.5) + (hR * FfxFloat32(0.5) + hG);
     // Noise detection.
     FfxFloat32 nz = FfxFloat32(0.25) * bL + FfxFloat32(0.25) * dL + FfxFloat32(0.25) * fL + FfxFloat32(0.25) * hL - eL;
     nz            = ffxSaturate(abs(nz) * ffxApproximateReciprocalMedium(ffxMax3(ffxMax3(bL, dL, eL), fL, hL) - ffxMin3(ffxMin3(bL, dL, eL), fL, hL)));
     nz            = FfxFloat32(-0.5) * nz + FfxFloat32(1.0);
     // Min and max of ring.
     FfxFloat32 mn4R = ffxMin(ffxMin3(bR, dR, fR), hR);
     FfxFloat32 mn4G = ffxMin(ffxMin3(bG, dG, fG), hG);
     FfxFloat32 mn4B = ffxMin(ffxMin3(bB, dB, fB), hB);
     FfxFloat32 mx4R = max(ffxMax3(bR, dR, fR), hR);
     FfxFloat32 mx4G = max(ffxMax3(bG, dG, fG), hG);
     FfxFloat32 mx4B = max(ffxMax3(bB, dB, fB), hB);
     // Immediate constants for peak range.
     FfxFloat32x2 peakC = FfxFloat32x2(1.0, -1.0 * 4.0);
     // Limiters, these need to be high precision RCPs.
     FfxFloat32 hitMinR = mn4R * rcp(FfxFloat32(4.0) * mx4R);
     FfxFloat32 hitMinG = mn4G * rcp(FfxFloat32(4.0) * mx4G);
     FfxFloat32 hitMinB = mn4B * rcp(FfxFloat32(4.0) * mx4B);
     FfxFloat32 hitMaxR = (peakC.x - mx4R) * rcp(FfxFloat32(4.0) * mn4R + peakC.y);
     FfxFloat32 hitMaxG = (peakC.x - mx4G) * rcp(FfxFloat32(4.0) * mn4G + peakC.y);
     FfxFloat32 hitMaxB = (peakC.x - mx4B) * rcp(FfxFloat32(4.0) * mn4B + peakC.y);
     FfxFloat32 lobeR   = max(-hitMinR, hitMaxR);
     FfxFloat32 lobeG   = max(-hitMinG, hitMaxG);
     FfxFloat32 lobeB   = max(-hitMinB, hitMaxB);
     FfxFloat32 lobe    = max(FfxFloat32(-FSR_RCAS_LIMIT), ffxMin(ffxMax3(lobeR, lobeG, lobeB), FfxFloat32(0.0))) * ffxAsFloat
     (con.x);
 // Apply noise removal.
#ifdef FSR_RCAS_DENOISE
     lobe *= nz;
#endif
     // Resolve, which needs the medium precision rcp approximation to avoid visible tonality changes.
     FfxFloat32 rcpL = ffxApproximateReciprocalMedium(FfxFloat32(4.0) * lobe + FfxFloat32(1.0));
     pixR            = (lobe * bR + lobe * dR + lobe * hR + lobe * fR + eR) * rcpL;
     pixG            = (lobe * bG + lobe * dG + lobe * hG + lobe * fG + eG) * rcpL;
     pixB            = (lobe * bB + lobe * dB + lobe * hB + lobe * fB + eB) * rcpL;
     return;
 }
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                  NON-PACKED 16-BIT VERSION
//==============================================================================================================================
#if defined(FFXM_GPU) && FFXM_HALF == 1 && defined(FSR_RCAS_H)
 // Input callback prototypes that need to be implemented by calling shader
 FfxFloat16x4 FsrRcasLoadH(FfxInt16x2 p);
 void FsrRcasInputH(inout FfxFloat16 r,inout FfxFloat16 g,inout FfxFloat16 b);
//------------------------------------------------------------------------------------------------------------------------------
 void FsrRcasH(
 out FfxFloat16 pixR, // Output values, non-vector so port between RcasFilter() and RcasFilterH() is easy.
 out FfxFloat16 pixG,
 out FfxFloat16 pixB,
 #ifdef FSR_RCAS_PASSTHROUGH_ALPHA
  out FfxFloat16 pixA,
 #endif
 FfxUInt32x2 ip, // Integer pixel position in output.
 FfxUInt32x4 con){ // Constant generated by RcasSetup().
  // Sharpening algorithm uses minimal 3x3 pixel neighborhood.
  //    b
  //  d e f
  //    h
  FfxInt16x2 sp=FfxInt16x2(ip);
  FfxFloat16x3 b=FsrRcasLoadH(sp+FfxInt16x2( 0,-1)).rgb;
  FfxFloat16x3 d=FsrRcasLoadH(sp+FfxInt16x2(-1, 0)).rgb;
  #ifdef FSR_RCAS_PASSTHROUGH_ALPHA
   FfxFloat16x4 ee=FsrRcasLoadH(sp);
   FfxFloat16x3 e=ee.rgb;pixA=ee.a;
  #else
   FfxFloat16x3 e=FsrRcasLoadH(sp).rgb;
  #endif
  FfxFloat16x3 f=FsrRcasLoadH(sp+FfxInt16x2( 1, 0)).rgb;
  FfxFloat16x3 h=FsrRcasLoadH(sp+FfxInt16x2( 0, 1)).rgb;
  // Rename (32-bit) or regroup (16-bit).
  FfxFloat16 bR=b.r;
  FfxFloat16 bG=b.g;
  FfxFloat16 bB=b.b;
  FfxFloat16 dR=d.r;
  FfxFloat16 dG=d.g;
  FfxFloat16 dB=d.b;
  FfxFloat16 eR=e.r;
  FfxFloat16 eG=e.g;
  FfxFloat16 eB=e.b;
  FfxFloat16 fR=f.r;
  FfxFloat16 fG=f.g;
  FfxFloat16 fB=f.b;
  FfxFloat16 hR=h.r;
  FfxFloat16 hG=h.g;
  FfxFloat16 hB=h.b;
  // Run optional input transform.
  FsrRcasInputH(bR,bG,bB);
  FsrRcasInputH(dR,dG,dB);
  FsrRcasInputH(eR,eG,eB);
  FsrRcasInputH(fR,fG,fB);
  FsrRcasInputH(hR,hG,hB);
  // Luma times 2.
  FfxFloat16 bL=bB*FFXM_BROADCAST_FLOAT16(0.5)+(bR*FFXM_BROADCAST_FLOAT16(0.5)+bG);
  FfxFloat16 dL=dB*FFXM_BROADCAST_FLOAT16(0.5)+(dR*FFXM_BROADCAST_FLOAT16(0.5)+dG);
  FfxFloat16 eL=eB*FFXM_BROADCAST_FLOAT16(0.5)+(eR*FFXM_BROADCAST_FLOAT16(0.5)+eG);
  FfxFloat16 fL=fB*FFXM_BROADCAST_FLOAT16(0.5)+(fR*FFXM_BROADCAST_FLOAT16(0.5)+fG);
  FfxFloat16 hL=hB*FFXM_BROADCAST_FLOAT16(0.5)+(hR*FFXM_BROADCAST_FLOAT16(0.5)+hG);
  // Noise detection.
  FfxFloat16 nz=FFXM_BROADCAST_FLOAT16(0.25)*bL+FFXM_BROADCAST_FLOAT16(0.25)*dL+FFXM_BROADCAST_FLOAT16(0.25)*fL+FFXM_BROADCAST_FLOAT16(0.25)*hL-eL;
  nz=ffxSaturate(abs(nz)*ffxApproximateReciprocalMediumHalf(ffxMax3Half(ffxMax3Half(bL,dL,eL),fL,hL)-ffxMin3Half(ffxMin3Half(bL,dL,eL),fL,hL)));
  nz=FFXM_BROADCAST_FLOAT16(-0.5)*nz+FFXM_BROADCAST_FLOAT16(1.0);
  // Min and max of ring.
  FfxFloat16 mn4R=min(ffxMin3Half(bR,dR,fR),hR);
  FfxFloat16 mn4G=min(ffxMin3Half(bG,dG,fG),hG);
  FfxFloat16 mn4B=min(ffxMin3Half(bB,dB,fB),hB);
  FfxFloat16 mx4R=max(ffxMax3Half(bR,dR,fR),hR);
  FfxFloat16 mx4G=max(ffxMax3Half(bG,dG,fG),hG);
  FfxFloat16 mx4B=max(ffxMax3Half(bB,dB,fB),hB);
  // Immediate constants for peak range.
  FfxFloat16x2 peakC=FfxFloat16x2(1.0,-1.0*4.0);
  // Limiters, these need to be high precision RCPs.
  FfxFloat16 hitMinR=mn4R*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16(4.0)*mx4R);
  FfxFloat16 hitMinG=mn4G*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16(4.0)*mx4G);
  FfxFloat16 hitMinB=mn4B*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16(4.0)*mx4B);
  FfxFloat16 hitMaxR=(peakC.x-mx4R)*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16(4.0)*mn4R+peakC.y);
  FfxFloat16 hitMaxG=(peakC.x-mx4G)*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16(4.0)*mn4G+peakC.y);
  FfxFloat16 hitMaxB=(peakC.x-mx4B)*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16(4.0)*mn4B+peakC.y);
  FfxFloat16 lobeR=max(-hitMinR,hitMaxR);
  FfxFloat16 lobeG=max(-hitMinG,hitMaxG);
  FfxFloat16 lobeB=max(-hitMinB,hitMaxB);
  FfxFloat16 lobe=max(FFXM_BROADCAST_FLOAT16(-FSR_RCAS_LIMIT),min(ffxMax3Half(lobeR,lobeG,lobeB),FFXM_BROADCAST_FLOAT16(0.0)))*FFXM_UINT32_TO_FLOAT16X2(con.y).x;
  // Apply noise removal.
  #ifdef FSR_RCAS_DENOISE
   lobe*=nz;
  #endif
  // Resolve, which needs the medium precision rcp approximation to avoid visible tonality changes.
  FfxFloat16 rcpL=ffxApproximateReciprocalMediumHalf(FFXM_BROADCAST_FLOAT16(4.0)*lobe+FFXM_BROADCAST_FLOAT16(1.0));
  pixR=(lobe*bR+lobe*dR+lobe*hR+lobe*fR+eR)*rcpL;
  pixG=(lobe*bG+lobe*dG+lobe*hG+lobe*fG+eG)*rcpL;
  pixB=(lobe*bB+lobe*dB+lobe*hB+lobe*fB+eB)*rcpL;
}
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                     PACKED 16-BIT VERSION
//==============================================================================================================================
#if defined(FFXM_GPU)&& FFXM_HALF == 1 && defined(FSR_RCAS_HX2)
 // Input callback prototypes that need to be implemented by the calling shader
 FfxFloat16x4 FsrRcasLoadHx2(FfxInt16x2 p);
 void FsrRcasInputHx2(inout FfxFloat16x2 r,inout FfxFloat16x2 g,inout FfxFloat16x2 b);
//------------------------------------------------------------------------------------------------------------------------------
 // Can be used to convert from packed Structures of Arrays to Arrays of Structures for store.
 void FsrRcasDepackHx2(out FfxFloat16x4 pix0,out FfxFloat16x4 pix1,FfxFloat16x2 pixR,FfxFloat16x2 pixG,FfxFloat16x2 pixB){
  #ifdef FFXM_HLSL
   // Invoke a slower path for DX only, since it won't allow uninitialized values.
   pix0.a=pix1.a=0.0;
  #endif
  pix0.rgb=FfxFloat16x3(pixR.x,pixG.x,pixB.x);
  pix1.rgb=FfxFloat16x3(pixR.y,pixG.y,pixB.y);}
//------------------------------------------------------------------------------------------------------------------------------
 void FsrRcasHx2(
 // Output values are for 2 8x8 tiles in a 16x8 region.
 //  pix<R,G,B>.x =  left 8x8 tile
 //  pix<R,G,B>.y = right 8x8 tile
 // This enables later processing to easily be packed as well.
 out FfxFloat16x2 pixR,
 out FfxFloat16x2 pixG,
 out FfxFloat16x2 pixB,
 #ifdef FSR_RCAS_PASSTHROUGH_ALPHA
  out FfxFloat16x2 pixA,
 #endif
 FfxUInt32x2 ip, // Integer pixel position in output.
 FfxUInt32x4 con){ // Constant generated by RcasSetup().
  // No scaling algorithm uses minimal 3x3 pixel neighborhood.
  FfxInt16x2 sp0=FfxInt16x2(ip);
  FfxFloat16x3 b0=FsrRcasLoadHx2(sp0+FfxInt16x2( 0,-1)).rgb;
  FfxFloat16x3 d0=FsrRcasLoadHx2(sp0+FfxInt16x2(-1, 0)).rgb;
  #ifdef FSR_RCAS_PASSTHROUGH_ALPHA
   FfxFloat16x4 ee0=FsrRcasLoadHx2(sp0);
   FfxFloat16x3 e0=ee0.rgb;pixA.r=ee0.a;
  #else
   FfxFloat16x3 e0=FsrRcasLoadHx2(sp0).rgb;
  #endif
  FfxFloat16x3 f0=FsrRcasLoadHx2(sp0+FfxInt16x2( 1, 0)).rgb;
  FfxFloat16x3 h0=FsrRcasLoadHx2(sp0+FfxInt16x2( 0, 1)).rgb;
  FfxInt16x2 sp1=sp0+FfxInt16x2(8,0);
  FfxFloat16x3 b1=FsrRcasLoadHx2(sp1+FfxInt16x2( 0,-1)).rgb;
  FfxFloat16x3 d1=FsrRcasLoadHx2(sp1+FfxInt16x2(-1, 0)).rgb;
  #ifdef FSR_RCAS_PASSTHROUGH_ALPHA
   FfxFloat16x4 ee1=FsrRcasLoadHx2(sp1);
   FfxFloat16x3 e1=ee1.rgb;pixA.g=ee1.a;
  #else
   FfxFloat16x3 e1=FsrRcasLoadHx2(sp1).rgb;
  #endif
  FfxFloat16x3 f1=FsrRcasLoadHx2(sp1+FfxInt16x2( 1, 0)).rgb;
  FfxFloat16x3 h1=FsrRcasLoadHx2(sp1+FfxInt16x2( 0, 1)).rgb;
  // Arrays of Structures to Structures of Arrays conversion.
  FfxFloat16x2 bR=FfxFloat16x2(b0.r,b1.r);
  FfxFloat16x2 bG=FfxFloat16x2(b0.g,b1.g);
  FfxFloat16x2 bB=FfxFloat16x2(b0.b,b1.b);
  FfxFloat16x2 dR=FfxFloat16x2(d0.r,d1.r);
  FfxFloat16x2 dG=FfxFloat16x2(d0.g,d1.g);
  FfxFloat16x2 dB=FfxFloat16x2(d0.b,d1.b);
  FfxFloat16x2 eR=FfxFloat16x2(e0.r,e1.r);
  FfxFloat16x2 eG=FfxFloat16x2(e0.g,e1.g);
  FfxFloat16x2 eB=FfxFloat16x2(e0.b,e1.b);
  FfxFloat16x2 fR=FfxFloat16x2(f0.r,f1.r);
  FfxFloat16x2 fG=FfxFloat16x2(f0.g,f1.g);
  FfxFloat16x2 fB=FfxFloat16x2(f0.b,f1.b);
  FfxFloat16x2 hR=FfxFloat16x2(h0.r,h1.r);
  FfxFloat16x2 hG=FfxFloat16x2(h0.g,h1.g);
  FfxFloat16x2 hB=FfxFloat16x2(h0.b,h1.b);
  // Run optional input transform.
  FsrRcasInputHx2(bR,bG,bB);
  FsrRcasInputHx2(dR,dG,dB);
  FsrRcasInputHx2(eR,eG,eB);
  FsrRcasInputHx2(fR,fG,fB);
  FsrRcasInputHx2(hR,hG,hB);
  // Luma times 2.
  FfxFloat16x2 bL=bB*FFXM_BROADCAST_FLOAT16X2(0.5)+(bR*FFXM_BROADCAST_FLOAT16X2(0.5)+bG);
  FfxFloat16x2 dL=dB*FFXM_BROADCAST_FLOAT16X2(0.5)+(dR*FFXM_BROADCAST_FLOAT16X2(0.5)+dG);
  FfxFloat16x2 eL=eB*FFXM_BROADCAST_FLOAT16X2(0.5)+(eR*FFXM_BROADCAST_FLOAT16X2(0.5)+eG);
  FfxFloat16x2 fL=fB*FFXM_BROADCAST_FLOAT16X2(0.5)+(fR*FFXM_BROADCAST_FLOAT16X2(0.5)+fG);
  FfxFloat16x2 hL=hB*FFXM_BROADCAST_FLOAT16X2(0.5)+(hR*FFXM_BROADCAST_FLOAT16X2(0.5)+hG);
  // Noise detection.
  FfxFloat16x2 nz=FFXM_BROADCAST_FLOAT16X2(0.25)*bL+FFXM_BROADCAST_FLOAT16X2(0.25)*dL+FFXM_BROADCAST_FLOAT16X2(0.25)*fL+FFXM_BROADCAST_FLOAT16X2(0.25)*hL-eL;
  nz=ffxSaturate(abs(nz)*ffxApproximateReciprocalMediumHalf(ffxMax3Half(ffxMax3Half(bL,dL,eL),fL,hL)-ffxMin3Half(ffxMin3Half(bL,dL,eL),fL,hL)));
  nz=FFXM_BROADCAST_FLOAT16X2(-0.5)*nz+FFXM_BROADCAST_FLOAT16X2(1.0);
  // Min and max of ring.
  FfxFloat16x2 mn4R=min(ffxMin3Half(bR,dR,fR),hR);
  FfxFloat16x2 mn4G=min(ffxMin3Half(bG,dG,fG),hG);
  FfxFloat16x2 mn4B=min(ffxMin3Half(bB,dB,fB),hB);
  FfxFloat16x2 mx4R=max(ffxMax3Half(bR,dR,fR),hR);
  FfxFloat16x2 mx4G=max(ffxMax3Half(bG,dG,fG),hG);
  FfxFloat16x2 mx4B=max(ffxMax3Half(bB,dB,fB),hB);
  // Immediate constants for peak range.
  FfxFloat16x2 peakC=FfxFloat16x2(1.0,-1.0*4.0);
  // Limiters, these need to be high precision RCPs.
  FfxFloat16x2 hitMinR=mn4R*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*mx4R);
  FfxFloat16x2 hitMinG=mn4G*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*mx4G);
  FfxFloat16x2 hitMinB=mn4B*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*mx4B);
  FfxFloat16x2 hitMaxR=(peakC.x-mx4R)*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*mn4R+peakC.y);
  FfxFloat16x2 hitMaxG=(peakC.x-mx4G)*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*mn4G+peakC.y);
  FfxFloat16x2 hitMaxB=(peakC.x-mx4B)*ffxReciprocalHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*mn4B+peakC.y);
  FfxFloat16x2 lobeR=max(-hitMinR,hitMaxR);
  FfxFloat16x2 lobeG=max(-hitMinG,hitMaxG);
  FfxFloat16x2 lobeB=max(-hitMinB,hitMaxB);
  FfxFloat16x2 lobe=max(FFXM_BROADCAST_FLOAT16X2(-FSR_RCAS_LIMIT),min(ffxMax3Half(lobeR,lobeG,lobeB),FFXM_BROADCAST_FLOAT16X2(0.0)))*FFXM_BROADCAST_FLOAT16X2(FFXM_UINT32_TO_FLOAT16X2(con.y).x);
  // Apply noise removal.
  #ifdef FSR_RCAS_DENOISE
   lobe*=nz;
  #endif
  // Resolve, which needs the medium precision rcp approximation to avoid visible tonality changes.
  FfxFloat16x2 rcpL=ffxApproximateReciprocalMediumHalf(FFXM_BROADCAST_FLOAT16X2(4.0)*lobe+FFXM_BROADCAST_FLOAT16X2(1.0));
  pixR=(lobe*bR+lobe*dR+lobe*hR+lobe*fR+eR)*rcpL;
  pixG=(lobe*bG+lobe*dG+lobe*hG+lobe*fG+eG)*rcpL;
  pixB=(lobe*bB+lobe*dB+lobe*hB+lobe*fB+eB)*rcpL;}
#endif
