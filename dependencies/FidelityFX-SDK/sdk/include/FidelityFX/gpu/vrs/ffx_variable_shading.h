// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

/// @defgroup FfxGPUVrs FidelityFX VRS
/// FidelityFX Variable Shading GPU documentation
///
/// @ingroup FfxGPUEffects

#if defined(FFX_CPP)
#define FFX_CPU
#include <FidelityFX/gpu/ffx_core.h>

FFX_STATIC void ffxVariableShadingGetDispatchInfo(
    const FfxDimensions2D resolution, const FfxUInt32 tileSize, const bool useAditionalShadingRates, FfxUInt32& numThreadGroupsX, FfxUInt32& numThreadGroupsY)
{
    FfxUInt32 vrsImageWidth  = FFX_DIVIDE_ROUNDING_UP(resolution.width, tileSize);
    FfxUInt32 vrsImageHeight = FFX_DIVIDE_ROUNDING_UP(resolution.height, tileSize);

    if (useAditionalShadingRates)
    {
        // coarse tiles are potentially 4x4, so each thread computes 4x4 pixels
        // as a result an 8x8 threadgroup computes 32x32 pixels
        numThreadGroupsX = FFX_DIVIDE_ROUNDING_UP(vrsImageWidth * tileSize, 32);
        numThreadGroupsY = FFX_DIVIDE_ROUNDING_UP(vrsImageHeight * tileSize, 32);
    }
    else
    {
        // coarse tiles are potentially 2x2, so each thread computes 2x2 pixels
        if (tileSize == 8)
        {
            //each threadgroup computes 4 VRS tiles
            numThreadGroupsX = FFX_DIVIDE_ROUNDING_UP(vrsImageWidth, 2);
            numThreadGroupsY = FFX_DIVIDE_ROUNDING_UP(vrsImageHeight, 2);
        }
        else
        {
            //each threadgroup computes one VRS tile
            numThreadGroupsX = vrsImageWidth;
            numThreadGroupsY = vrsImageHeight;
        }
    }
}
#elif defined(FFX_GPU)

// Forward declaration of functions that need to be implemented by shader code using this technique
FfxFloat32   ReadLuminance(FfxInt32x2 pos);
FfxFloat32x2 ReadMotionVec2D(FfxInt32x2 pos);
void         WriteVrsImage(FfxInt32x2 pos, FfxUInt32 value);

FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE1D_1X = 0x0;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE1D_2X = 0x1;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE1D_4X = 0x2;
#define FFX_VARIABLESHADING_MAKE_SHADING_RATE(x,y) ((x << 2) | (y))

FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_1X1 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_1X); // 0;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_1X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_2X); // 0x1;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_2X1 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_1X); // 0x4;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_2X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_2X); // 0x5;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_2X4 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_4X); // 0x6;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_4X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_4X, FFX_VARIABLESHADING_RATE1D_2X); // 0x9;
FFX_STATIC const FfxUInt32 FFX_VARIABLESHADING_RATE_4X4 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_4X, FFX_VARIABLESHADING_RATE1D_4X); // 0xa;

#if !defined FFX_VARIABLESHADING_ADDITIONALSHADINGRATES
#if FFX_VARIABLESHADING_TILESIZE == 8
FFX_STATIC const FfxUInt32 FFX_VariableShading_ThreadCount1D = 8;
FFX_STATIC const FfxUInt32 FFX_VariableShading_NumBlocks1D = 2;
#elif FFX_VARIABLESHADING_TILESIZE == 16
FFX_STATIC const FfxUInt32 FFX_VariableShading_ThreadCount1D = 8;
FFX_STATIC const FfxUInt32 FFX_VariableShading_NumBlocks1D = 1;
#else // FFX_VARIABLESHADING_TILESIZE == 32
FFX_STATIC const FfxUInt32 FFX_VariableShading_ThreadCount1D = 16;
FFX_STATIC const FfxUInt32 FFX_VariableShading_NumBlocks1D = 1;
#endif
FFX_STATIC const FfxUInt32 FFX_VariableShading_SampleCount1D = FFX_VariableShading_ThreadCount1D + 2;

FFX_GROUPSHARED FfxUInt32 FFX_VariableShading_LdsGroupReduce;

FFX_STATIC const FfxUInt32 FFX_VariableShading_ThreadCount = FFX_VariableShading_ThreadCount1D * FFX_VariableShading_ThreadCount1D;
FFX_STATIC const FfxUInt32 FFX_VariableShading_SampleCount = FFX_VariableShading_SampleCount1D * FFX_VariableShading_SampleCount1D;
FFX_STATIC const FfxUInt32 FFX_VariableShading_NumBlocks = FFX_VariableShading_NumBlocks1D * FFX_VariableShading_NumBlocks1D;

FFX_GROUPSHARED FfxFloat32x3 FFX_VariableShading_LdsVariance[FFX_VariableShading_SampleCount];
FFX_GROUPSHARED FfxFloat32 FFX_VariableShading_LdsMin[FFX_VariableShading_SampleCount];
FFX_GROUPSHARED FfxFloat32 FFX_VariableShading_LdsMax[FFX_VariableShading_SampleCount];

#else //if defined FFX_VARIABLESHADING_ADDITIONALSHADINGRATES
FFX_STATIC const FfxUInt32 FFX_VariableShading_ThreadCount1D = 8;
FFX_STATIC const FfxUInt32 FFX_VariableShading_NumBlocks1D = 32 / FFX_VARIABLESHADING_TILESIZE;
FFX_STATIC const FfxUInt32 FFX_VariableShading_TilesPerGroup = FFX_VariableShading_NumBlocks1D * FFX_VariableShading_NumBlocks1D;
FFX_STATIC const FfxUInt32 FFX_VariableShading_SampleCount1D = FFX_VariableShading_ThreadCount1D + 2;

FFX_GROUPSHARED FfxUInt32 FFX_VariableShading_LdsGroupReduce[FFX_VariableShading_TilesPerGroup];

FFX_STATIC const FfxUInt32 FFX_VariableShading_ThreadCount = FFX_VariableShading_ThreadCount1D * FFX_VariableShading_ThreadCount1D;
FFX_STATIC const FfxUInt32 FFX_VariableShading_SampleCount = FFX_VariableShading_SampleCount1D * FFX_VariableShading_SampleCount1D;
FFX_STATIC const FfxUInt32 FFX_VariableShading_NumBlocks = FFX_VariableShading_NumBlocks1D * FFX_VariableShading_NumBlocks1D;

// load and compute variance for 1x2, 2x1, 2x2, 2x4, 4x2, 4x4 for 8x8 coarse pixels
FFX_GROUPSHARED FfxUInt32 FFX_VariableShading_LdsShadingRate[FFX_VariableShading_SampleCount];
#endif

// Read luminance value from previous frame's color buffer.
FfxFloat32 VrsGetLuminance(FfxInt32x2 pos)
{
    FfxFloat32x2 v = ReadMotionVec2D(pos);
    pos            = pos - FfxInt32x2(round(v));
    // clamp to screen
    if (pos.x < 0) pos.x = 0;
    if (pos.y < 0) pos.y = 0;
    if (pos.x >= Resolution().x)
        pos.x = Resolution().x - 1;
    if (pos.y >= Resolution().y)
        pos.y = Resolution().y - 1;

    return ReadLuminance(pos);
}

// Get flattened LDS offset.
FfxInt32 VrsFlattenLdsOffset(FfxInt32x2 coord)
{
    coord += 1;
    return coord.y * FfxInt32(FFX_VariableShading_SampleCount1D) + coord.x;
}

#if !defined FFX_VARIABLESHADING_ADDITIONALSHADINGRATES

/// Generate and write shading rates to VRS image.
///
/// @param [in] Gid                      Index for which thread group the compute shader is executing in.
/// @param [in] Gtid                     Thread index within a thread group the compute shader is executing in.
/// @param [in] Gidx                     Flattened index of compute shader thread.
///
/// @ingroup FfxGPUVrs
void VrsGenerateVrsImage(FfxUInt32x3 Gid, FfxUInt32x3 Gtid, FfxUInt32 Gidx)
{
    FfxInt32x2 tileOffset = FfxInt32x2(Gid.xy * FFX_VariableShading_ThreadCount1D * 2);
    FfxInt32x2 baseOffset = tileOffset + FfxInt32x2(-2, -2);
    FfxUInt32 index = Gidx;

#if FFX_VARIABLESHADING_TILESIZE > 8
    if (index == 0)
    {
        FFX_VariableShading_LdsGroupReduce = FFX_VARIABLESHADING_RATE_2X2;
    }
#endif

    // sample source texture (using motion vectors)
    while (index < FFX_VariableShading_SampleCount)
    {
        FfxInt32x2 index2D = 2 * FfxInt32x2(index % FFX_VariableShading_SampleCount1D, index / FFX_VariableShading_SampleCount1D);
        FfxFloat32x4 lum;
        lum.x = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(0, 0));
        lum.y = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(1, 0));
        lum.z = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(0, 1));
        lum.w = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(1, 1));

        // compute the 2x1, 1x2 and 2x2 variance inside the 2x2 coarse pixel region
        FfxFloat32x3 delta;
        delta.x = ffxMax(abs(lum.x - lum.y), abs(lum.z - lum.w));
        delta.y = ffxMax(abs(lum.x - lum.z), abs(lum.y - lum.w));
        FfxFloat32x2 minmax = FfxFloat32x2(ffxMin(ffxMin(ffxMin(lum.x, lum.y), lum.z), lum.w), ffxMax(ffxMax(ffxMax(lum.x, lum.y), lum.z), lum.w));
        delta.z = minmax.y - minmax.x;

        // reduce variance value for fast moving pixels
        FfxFloat32 v = length(ReadMotionVec2D(baseOffset + index2D));
        v *= MotionFactor();
        delta -= v;
        minmax.y -= v;

        // store variance as well as min/max luminance
        FFX_VariableShading_LdsVariance[index] = delta;
        FFX_VariableShading_LdsMin[index] = minmax.x;
        FFX_VariableShading_LdsMax[index] = minmax.y;

        index += FFX_VariableShading_ThreadCount;
    }
#if defined(FFX_HLSL)
    GroupMemoryBarrierWithGroupSync();
#elif defined(FFX_GLSL)
    barrier();
#endif

    // upper left coordinate in LDS
    FfxInt32x2 threadUV = FfxInt32x2(Gtid.xy);

    // look at neighbouring coarse pixels, to combat burn in effect due to frame dependence
    FfxFloat32x3 delta = FFX_VariableShading_LdsVariance[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 0))];

    // read the minimum luminance for neighbouring coarse pixels
    FfxFloat32 minNeighbour = FFX_VariableShading_LdsMin[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, -1))];
    minNeighbour = ffxMin(minNeighbour, FFX_VariableShading_LdsMin[VrsFlattenLdsOffset(threadUV + FfxInt32x2(-1, 0))]);
    minNeighbour = ffxMin(minNeighbour, FFX_VariableShading_LdsMin[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 1))]);
    minNeighbour = ffxMin(minNeighbour, FFX_VariableShading_LdsMin[VrsFlattenLdsOffset(threadUV + FfxInt32x2(1, 0))]);
    FfxFloat32 dMin = ffxMax(0.f, FFX_VariableShading_LdsMin[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 0))] - minNeighbour);

    // read the maximum luminance for neighbouring coarse pixels
    FfxFloat32 maxNeighbour = FFX_VariableShading_LdsMax[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, -1))];
    maxNeighbour = ffxMax(maxNeighbour, FFX_VariableShading_LdsMax[VrsFlattenLdsOffset(threadUV + FfxInt32x2(-1, 0))]);
    maxNeighbour = ffxMax(maxNeighbour, FFX_VariableShading_LdsMax[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 1))]);
    maxNeighbour = ffxMax(maxNeighbour, FFX_VariableShading_LdsMax[VrsFlattenLdsOffset(threadUV + FfxInt32x2(1, 0))]);
    FfxFloat32 dMax = ffxMax(0.f, maxNeighbour - FFX_VariableShading_LdsMax[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 0))]);

    // assume higher luminance based on min & max values gathered from neighbouring pixels
    delta = ffxMax(FfxFloat32x3(0.f, 0.f, 0.f), delta + dMin + dMax);

    // Reduction: find maximum variance within VRS tile
#if FFX_VARIABLESHADING_TILESIZE > 8
    // with tilesize=16 we compute 1 tile in one 8x8 threadgroup, in wave32 mode we'll need LDS to compute the per tile max
    // similar for tilesize=32: 1 tile is computed in a 16x16 threadgroup, so we definitely need LDS
#if defined(FFX_HLSL)
    delta = WaveActiveMax(delta);
#elif defined(FFX_GLSL)
    delta = subgroupMax(delta);
#endif

#if defined(FFX_HLSL)
    if (WaveIsFirstLane())
#elif defined(FFX_GLSL)
    if (0 == gl_SubgroupInvocationID)
#endif
    {
        FfxUInt32 shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_1X);

        if (delta.z < VarianceCutoff())
        {
            shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_2X);
        }
        else
        {
            if (delta.x > delta.y)
            {
                shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, (delta.y > VarianceCutoff()) ? FFX_VARIABLESHADING_RATE1D_1X : FFX_VARIABLESHADING_RATE1D_2X);
            }
            else
            {
                shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE((delta.x > VarianceCutoff()) ? FFX_VARIABLESHADING_RATE1D_1X : FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_1X);
            }
        }
#if defined(FFX_HLSL)
        InterlockedAnd(FFX_VariableShading_LdsGroupReduce, shadingRate);
#elif defined(FFX_GLSL)
        atomicAnd(FFX_VariableShading_LdsGroupReduce, shadingRate);
#endif
    }
#if defined(FFX_HLSL)
    GroupMemoryBarrierWithGroupSync();
#elif defined(FFX_GLSL)
    barrier();
#endif

    if (Gidx == 0)
    {
        // Store
        WriteVrsImage(FfxInt32x2(Gid.xy), FFX_VariableShading_LdsGroupReduce);
    }
#else
    // with tilesize=8 we compute 2x2 tiles in one 8x8 threadgroup
    // even in wave32 mode wave FfxInt32rinsics are sufficient
    FfxFloat32x4 diffX = FfxFloat32x4(0, 0, 0, 0);
    FfxFloat32x4 diffY = FfxFloat32x4(0, 0, 0, 0);
    FfxFloat32x4 diffZ = FfxFloat32x4(0, 0, 0, 0);
    FfxUInt32 idx = (Gtid.y & (FFX_VariableShading_NumBlocks1D - 1)) * FFX_VariableShading_NumBlocks1D + (Gtid.x & (FFX_VariableShading_NumBlocks1D - 1));
    diffX[idx] = delta.x;
    diffY[idx] = delta.y;
    diffZ[idx] = delta.z;
#if defined(FFX_HLSL)
    diffX = WaveActiveMax(diffX);
    diffY = WaveActiveMax(diffY);
    diffZ = WaveActiveMax(diffZ);
#elif defined(FFX_GLSL)
    diffX = subgroupMax(diffX);
    diffY = subgroupMax(diffY);
    diffZ = subgroupMax(diffZ);
#endif

    // write out shading rates to VRS image
    if (Gidx < FFX_VariableShading_NumBlocks)
    {
        FfxFloat32 varH = diffX[Gidx];
        FfxFloat32 varV = diffY[Gidx];
        FfxFloat32 var = diffZ[Gidx];;
        FfxUInt32 shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_1X);

        if (var < VarianceCutoff())
        {
            shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_2X);
        }
        else
        {
            if (varH > varV)
            {
                shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, (varV > VarianceCutoff()) ? FFX_VARIABLESHADING_RATE1D_1X : FFX_VARIABLESHADING_RATE1D_2X);
            }
            else
            {
                shadingRate = FFX_VARIABLESHADING_MAKE_SHADING_RATE((varH > VarianceCutoff()) ? FFX_VARIABLESHADING_RATE1D_1X : FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_1X);
            }
        }
        // Store
          WriteVrsImage(
            FfxInt32x2(Gid.xy * FFX_VariableShading_NumBlocks1D + FfxUInt32x2(Gidx / FFX_VariableShading_NumBlocks1D, Gidx % FFX_VariableShading_NumBlocks1D)), shadingRate);
    }
#endif
}

#else // if defined FFX_VARIABLESHADING_ADDITIONALSHADINGRATES

/// Generate and write shading rates to VRS image.
///
/// @param [in] Gid                      Index for which thread group the compute shader is executing in.
/// @param [in] Gtid                     Thread index within a thread group the compute shader is executing in.
/// @param [in] Gidx                     Flattened index of compute shader thread.
///
/// @ingroup FfxGPUVrs
void VrsGenerateVrsImage(FfxUInt32x3 Gid, FfxUInt32x3 Gtid, FfxUInt32 Gidx)
{
    FfxInt32x2 tileOffset = FfxInt32x2(Gid.xy * FFX_VariableShading_ThreadCount1D * 4);
    FfxInt32x2 baseOffset = tileOffset;
    FfxUInt32 index      = Gidx;

    while (index < FFX_VariableShading_SampleCount)
    {
        FfxInt32x2 index2D = 4 * FfxInt32x2(index % FFX_VariableShading_SampleCount1D, index / FFX_VariableShading_SampleCount1D);

        // reduce shading rate for fast moving pixels
        FfxFloat32 v = length(ReadMotionVec2D(baseOffset + index2D));
        v *= MotionFactor();

        // compute variance for one 4x4 region
        FfxFloat32 var2x1 = 0;
        FfxFloat32 var1x2 = 0;
        FfxFloat32 var2x2 = 0;
        FfxFloat32x2 minmax4x2[2] = { FfxFloat32x2(VarianceCutoff(), 0.f), FfxFloat32x2(VarianceCutoff(), 0.f) };
        FfxFloat32x2 minmax2x4[2] = { FfxFloat32x2(VarianceCutoff(), 0.f), FfxFloat32x2(VarianceCutoff(), 0.f) };
        FfxFloat32x2 minmax4x4 = FfxFloat32x2(VarianceCutoff(), 0.f);

        // computes variance for 2x2 tiles
        // also we need min/max for 2x4, 4x2 & 4x4 
        for (FfxUInt32 y = 0; y < 2; y += 1)
        {
            FfxFloat32 tmpVar4x2 = 0;
            for (FfxUInt32 x = 0; x < 2; x += 1)
            {
                FfxInt32x2 index2D = 4 * FfxInt32x2(index % FFX_VariableShading_SampleCount1D, index / FFX_VariableShading_SampleCount1D) + FfxInt32x2(2 * x, 2 * y);
                FfxFloat32x4 lum;
                lum.x = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(0, 0));
                lum.y = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(1, 0));
                lum.z = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(0, 1));
                lum.w = VrsGetLuminance(baseOffset + index2D + FfxInt32x2(1, 1));

                FfxFloat32x2 minmax = FfxFloat32x2(ffxMin(ffxMin(lum.x, lum.y), ffxMin(lum.z, lum.w)), ffxMax(ffxMax(lum.x, lum.y), ffxMax(lum.z, lum.w)));
                FfxFloat32x3 delta;
                delta.x = ffxMax(abs(lum.x - lum.y), abs(lum.z - lum.w));
                delta.y = ffxMax(abs(lum.x - lum.y), abs(lum.z - lum.w));
                delta.z = minmax.y - minmax.x;

                // reduce shading rate for fast moving pixels
                delta = ffxMax(FfxFloat32x3(0.f, 0.f, 0.f), delta - v);

                var2x1 = ffxMax(var2x1, delta.x);
                var1x2 = ffxMax(var1x2, delta.y);
                var2x2 = ffxMax(var2x2, delta.z);

                minmax4x2[y].x = ffxMin(minmax4x2[y].x, minmax.x);
                minmax4x2[y].y = ffxMax(minmax4x2[y].y, minmax.y);

                minmax2x4[x].x = ffxMin(minmax2x4[x].x, minmax.x);
                minmax2x4[x].y = ffxMax(minmax2x4[x].y, minmax.y);

                minmax4x4.x = ffxMin(minmax4x4.x, minmax.x);
                minmax4x4.y = ffxMax(minmax4x4.y, minmax.y);
            }
        }

        FfxFloat32 var4x2 = ffxMax(0.f, ffxMax(minmax4x2[0].y - minmax4x2[0].x, minmax4x2[1].y - minmax4x2[1].x) - v);
        FfxFloat32 var2x4 = ffxMax(0.f, ffxMax(minmax2x4[0].y - minmax2x4[0].x, minmax2x4[1].y - minmax2x4[1].x) - v);
        FfxFloat32 var4x4 = ffxMax(0.f, minmax4x4.y - minmax4x4.x - v);

        FfxUInt32 shadingRate = FFX_VARIABLESHADING_RATE_1X1;
        if (var4x4 < VarianceCutoff()) shadingRate = FFX_VARIABLESHADING_RATE_4X4;
        else if (var4x2 < VarianceCutoff()) shadingRate = FFX_VARIABLESHADING_RATE_4X2;
        else if (var2x4 < VarianceCutoff()) shadingRate = FFX_VARIABLESHADING_RATE_2X4;
        else if (var2x2 < VarianceCutoff()) shadingRate = FFX_VARIABLESHADING_RATE_2X2;
        else if (var2x1 < VarianceCutoff()) shadingRate = FFX_VARIABLESHADING_RATE_2X1;
        else if (var1x2 < VarianceCutoff()) shadingRate = FFX_VARIABLESHADING_RATE_1X2;

        FFX_VariableShading_LdsShadingRate[index] = shadingRate;

        index += FFX_VariableShading_ThreadCount;
    }

    if (Gidx < FFX_VariableShading_TilesPerGroup)
    {
        FFX_VariableShading_LdsGroupReduce[Gidx] = 0;
    }
#if defined(FFX_HLSL)
    GroupMemoryBarrierWithGroupSync();
#elif defined(FFX_GLSL)
    barrier();
#endif

    FfxInt32 i = 0;
    FfxInt32x2 threadUV = FfxInt32x2(Gtid.xy);

    FfxUInt32 shadingRate[FFX_VariableShading_TilesPerGroup];
    for (i = 0; i < FFX_VariableShading_TilesPerGroup; ++i)
    {
        shadingRate[i] = FFX_VARIABLESHADING_RATE_4X4;
    }
    FfxUInt32 idx    = (Gtid.y & (FFX_VariableShading_NumBlocks1D - 1)) * FFX_VariableShading_NumBlocks1D + (Gtid.x & (FFX_VariableShading_NumBlocks1D - 1));
    shadingRate[idx] = FFX_VariableShading_LdsShadingRate[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 0))];
    shadingRate[idx] = ffxMin(shadingRate[idx], FFX_VariableShading_LdsShadingRate[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, -1))]);
    shadingRate[idx] = ffxMin(shadingRate[idx], FFX_VariableShading_LdsShadingRate[VrsFlattenLdsOffset(threadUV + FfxInt32x2(-1, 0))]);
    shadingRate[idx] = ffxMin(shadingRate[idx], FFX_VariableShading_LdsShadingRate[VrsFlattenLdsOffset(threadUV + FfxInt32x2(1, 0))]);
    shadingRate[idx] = ffxMin(shadingRate[idx], FFX_VariableShading_LdsShadingRate[VrsFlattenLdsOffset(threadUV + FfxInt32x2(0, 1))]);

    // wave-reduce
    for (i = 0; i < FFX_VariableShading_TilesPerGroup; ++i)
    {
#if defined(FFX_HLSL)
        shadingRate[i] = WaveActiveMin(shadingRate[i]);
#elif defined(FFX_GLSL)
        shadingRate[i] = subgroupMin(shadingRate[i]);
#endif
    }

    // threadgroup-reduce
#if FFX_VARIABLESHADING_TILESIZE<16
#if defined(FFX_HLSL)
    if (WaveIsFirstLane())
#elif defined(FFX_GLSL)
    if (0 == gl_SubgroupInvocationID)
#endif
    {
        for (i = 0; i < FFX_VariableShading_TilesPerGroup; ++i)
        {
#if defined(FFX_HLSL)
            InterlockedAnd(FFX_VariableShading_LdsGroupReduce[i], shadingRate[i]);
#elif defined(FFX_GLSL)
            atomicAnd(FFX_VariableShading_LdsGroupReduce[i], shadingRate[i]);
#endif
        }
    }
#if defined(FFX_HLSL)
    GroupMemoryBarrierWithGroupSync();
#elif defined(FFX_GLSL)
    barrier();
#endif

    // write out final rates
    if (Gidx < FFX_VariableShading_TilesPerGroup)
    {
        WriteVrsImage(
            FfxInt32x2(Gid.xy * FFX_VariableShading_NumBlocks1D + FfxUInt32x2(Gidx / FFX_VariableShading_NumBlocks1D, Gidx % FFX_VariableShading_NumBlocks1D)),
            FFX_VariableShading_LdsGroupReduce[Gidx]);
    }
#else
    // write out final rates
    if (Gidx < FFX_VariableShading_TilesPerGroup)
    {
        WriteVrsImage(
            FfxInt32x2(Gid.xy * FFX_VariableShading_NumBlocks1D + FfxUInt32x2(Gidx / FFX_VariableShading_NumBlocks1D, Gidx % FFX_VariableShading_NumBlocks1D)),
            shadingRate[Gidx]);
    }
#endif


}
#endif // FFX_VARIABLESHADING_ADDITIONALSHADINGRATES
#endif // FFX_CPP|FFX_GPU
