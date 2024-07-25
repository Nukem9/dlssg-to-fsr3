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

/// @defgroup FfxGPUClassifier FidelityFX Classifier
/// FidelityFX Classifier GPU documentation
///
/// @ingroup FfxGPUEffects

#include "ffx_classifier_common.h"

struct ClassifyResults
{
    FfxBoolean bIsActiveLane;
    FfxBoolean bIsInLight;
    FfxFloat32 minT;
    FfxFloat32 maxT;
};

ClassifyResults FfxClassify(const FfxUInt32x2 pixelCoord,
                            const FfxBoolean bUseNormal,
                            const FfxBoolean bUseCascadeBlocking)
{
    const FfxBoolean bIsInViewport = all(FFX_LESS_THAN(pixelCoord, TextureSize().xy));
    const FfxFloat32 depth = FfxClassifierSampleDepth(pixelCoord);

#if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
    FfxBoolean bIsActiveLane = bIsInViewport && (depth > 0.0f);
#else
    FfxBoolean bIsActiveLane = bIsInViewport && (depth < 1.0f);
#endif
    FfxBoolean bIsInLight = FFX_FALSE;
    FfxFloat32 minT       = FFX_POSITIVE_INFINITY_FLOAT;
    FfxFloat32 maxT = 0.f;

    if (bUseNormal && bIsActiveLane)
    {
        const FfxFloat32x3 normal = normalize(FfxClassifierSampleNormal(pixelCoord));
        const FfxBoolean   bIsNormalFacingLight = dot(normal, -LightDir()) > 0;

        bIsActiveLane = bIsActiveLane && bIsNormalFacingLight;
    }

    if (bUseCascadeBlocking && bIsActiveLane)
    {
        const FfxFloat32x2 uv = pixelCoord * TextureSize().zw;
        const FfxFloat32x4 homogeneous = FFX_MATRIX_MULTIPLY(ViewToWorld(), FfxFloat32x4(2.0f * FfxFloat32x2(uv.x, 1.0f - uv.y) - 1.0f, depth, 1));
        const FfxFloat32x3 worldPos = homogeneous.xyz / homogeneous.w;

        const FfxFloat32x3 lightViewSpacePos = FFX_MATRIX_MULTIPLY(LightView(), FfxFloat32x4(worldPos, 1)).xyz;

        FfxBoolean bIsInActiveCascade = FFX_FALSE;

        if (bUseCascadeBlocking)
        {
            const FfxFloat32 radius = SunSizeLightSpace() * lightViewSpacePos.z;

            FfxFloat32x3 shadowCoord = FfxFloat32x3(0, 0, 0);
            FfxUInt32 cascadeIndex = 0;
            for (FfxUInt32 i = 0; i < CascadeCount(); ++i)
            {
                shadowCoord = lightViewSpacePos * CascadeScale(i).xyz + CascadeOffset(i).xyz;
                if (all(FFX_GREATER_THAN(shadowCoord.xy, FfxFloat32x2(0, 0))) && all(FFX_LESS_THAN(shadowCoord.xy, FfxFloat32x2(1, 1))))
                {
                    cascadeIndex = i;
                    break;
                }
            }

            // grow search area by a pixel to make sure we search a wide enough area
            // also scale everything from UV to pixel coord for image loads.
            const FfxFloat32x2 radiusCoord = abs(FfxFloat32x2(radius, radius) * CascadeScale(cascadeIndex).xy) * FfxFloat32x2(CascadeSize(), CascadeSize()) + FfxFloat32x2(1,1);
            shadowCoord.xy *= CascadeSize();

        #if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
            const FfxFloat32 depthCmp = shadowCoord.z + BlockerOffset();
        #else
            const FfxFloat32 depthCmp    = shadowCoord.z - BlockerOffset();
        #endif

        #if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
            FfxFloat32 maxD        = 1;
            FfxFloat32 minD        = 0;
            FfxFloat32 closetDepth = 1;
        #else
            FfxFloat32 maxD        = 0;
            FfxFloat32 minD        = 1;
            FfxFloat32 closetDepth = 0;
        #endif


            // With small shadow maps we will be bound on filtering since the shadow map can end up completely in LO cache
            // using an image load is faster then a sample in RDNA but we will be losing the benefit of doing some of the ALU
            // in the filter and getting 4 pixels of data per tap.
            for (FfxUInt32 x = 0; x < k_poissonDiscSampleCountHigh; ++x)
            {
                const FfxFloat32x2 sampleUV   = shadowCoord.xy + k_poissonDisc[x] * radiusCoord + 0.5f;

                // UV bounds check
                if (!(all(FFX_GREATER_THAN_EQUAL(sampleUV.xy, FfxFloat32x2(0, 0))) &&
                      all(FFX_LESS_THAN(sampleUV.xy, FfxFloat32x2(CascadeSize(), CascadeSize())))))
                    continue;
                const FfxFloat32 pixelDepth = FfxClassifierSampleShadowMap(sampleUV, cascadeIndex);

                // using min and max to reduce number of cmps
            #if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
                maxD = min(maxD, pixelDepth);
                minD = max(minD, pixelDepth);

                // need to find closet point in front of the receiver
                if (pixelDepth > depthCmp)
                {
                    closetDepth = min(closetDepth, pixelDepth);
                }
            #else
                maxD = max(maxD, pixelDepth);
                minD = min(minD, pixelDepth);

                // need to find closet point in front of the receiver
                if (pixelDepth < depthCmp)
                {
                    closetDepth = max(closetDepth, pixelDepth);
                }
            #endif
            }

        #if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
            const FfxBoolean bIsInShadow = (maxD >= depthCmp);
            bIsInLight                   = RejectLitPixels() && (minD <= depthCmp);
        #else
            const FfxBoolean bIsInShadow = (maxD <= depthCmp);
            bIsInLight                   = RejectLitPixels() && (minD >= depthCmp);
        #endif
            bIsInActiveCascade = !bIsInShadow && !bIsInLight;

            if (bIsInActiveCascade && UseCascadesForRayT())
            {
        #if FFX_CLASSIFIER_OPTION_INVERTED_DEPTH
            const FfxFloat32 viewMinT = abs(min(shadowCoord.z + closetDepth + BlockerOffset(), 0) / CascadeScale(cascadeIndex).z);
            const FfxFloat32 viewMaxT = abs((shadowCoord.z + minD - BlockerOffset()) / CascadeScale(cascadeIndex).z);
        #else
            const FfxFloat32 viewMinT = abs(max(shadowCoord.z - closetDepth - BlockerOffset(), 0) / CascadeScale(cascadeIndex).z);
            const FfxFloat32 viewMaxT = abs((shadowCoord.z - minD + BlockerOffset()) / CascadeScale(cascadeIndex).z);
        #endif

            // if its known that the light view matrix is only a rotation or has uniform scale this can be optimized.
            minT = length(FFX_MATRIX_MULTIPLY(InverseLightView(), FfxFloat32x4(0, 0, viewMinT, 0)).xyz);
            maxT = length(FFX_MATRIX_MULTIPLY(InverseLightView(), FfxFloat32x4(0, radius, viewMaxT, 0)).xyz);

            }
        }

        bIsActiveLane = bIsActiveLane && bIsInActiveCascade;
    }

    const ClassifyResults results = { bIsActiveLane, bIsInLight, minT, maxT };

    return results;
}

/// Classifier pass entry point.
///
/// @param LocalThreadId The "flattened" index of a thread within a thread group (SV_GroupIndex).
/// @param WorkGroupId   Index of the thread group currently executed (SV_GroupID).
/// @ingroup FfxGPUClassifier
void FfxClassifyShadows(FfxUInt32 LocalThreadId, FfxUInt32x3 WorkGroupId)
{
    const FfxUInt32x2 localID = ffxRemapForWaveReduction(LocalThreadId);
    const FfxUInt32x2 pixelCoord = WorkGroupId.xy * k_tileSize + localID.xy;

#if FFX_CLASSIFIER_OPTION_CLASSIFIER_MODE == 0
    ClassifyResults results = FfxClassify(pixelCoord, FFX_TRUE, FFX_FALSE);
#endif
#if FFX_CLASSIFIER_OPTION_CLASSIFIER_MODE == 1
    ClassifyResults results = FfxClassify(pixelCoord, FFX_TRUE, FFX_TRUE);
#endif
    Tile currentTile = TileCreate(WorkGroupId.xy);
    const FfxUInt32 mask        = BoolToWaveMask(results.bIsActiveLane, localID);
    currentTile.mask = mask;

#if FFX_CLASSIFIER_OPTION_CLASSIFIER_MODE == 1
    if (UseCascadesForRayT())
    {
        // At lest one lane must be active for the tile to be written out, so the infinitly and zero will be emoved by the wave min and max.
        // Otherwise we will get minT to be infinite and maxT to be 0
        currentTile.minT = max(ffxWaveMin(results.minT), currentTile.minT);
        currentTile.maxT = min(ffxWaveMax(results.maxT), currentTile.maxT);
    }
#endif

    const FfxUInt32 lightMask = BoolToWaveMask(results.bIsInLight, localID);
    const FfxBoolean bDiscardTile = (CountBits(mask) <= TileTolerance());

    if (LocalThreadId == 0)
    {
        if (!bDiscardTile)
        {
            FfxClassifierStoreTile(TileToUint(currentTile));
        }

        FfxClassifierStoreLightMask(WorkGroupId.xy, lightMask);
    }
}
