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

#include "ffx_classifier_resources.h"
#include "ffx_core.h"

#if defined(FFX_GPU)

const FfxFloat32x2 k_poissonDisc[] = {
    FfxFloat32x2(0.640736f, -0.355205f),  FfxFloat32x2(-0.725411f, -0.688316f), FfxFloat32x2(-0.185095f, 0.722648f),   FfxFloat32x2(0.770596f, 0.637324f),
    FfxFloat32x2(-0.921445f, 0.196997f),  FfxFloat32x2(0.076571f, -0.98822f),   FfxFloat32x2(-0.1348f, -0.0908536f),   FfxFloat32x2(0.320109f, 0.257241f),
    FfxFloat32x2(0.994021f, 0.109193f),   FfxFloat32x2(0.304934f, 0.952374f),   FfxFloat32x2(-0.698577f, 0.715535f),   FfxFloat32x2(0.548701f, -0.836019f),
    FfxFloat32x2(-0.443159f, 0.296121f),  FfxFloat32x2(0.15067f, -0.489731f),   FfxFloat32x2(-0.623829f, -0.208167f),  FfxFloat32x2(-0.294778f, -0.596545f),
    FfxFloat32x2(0.334086f, -0.128208f),  FfxFloat32x2(-0.0619831f, 0.311747f), FfxFloat32x2(0.166112f, 0.61626f),     FfxFloat32x2(-0.289127f, -0.957291f),
    FfxFloat32x2(-0.98748f, -0.157745f),  FfxFloat32x2(0.637501f, 0.0651571f),  FfxFloat32x2(0.971376f, -0.237545f),   FfxFloat32x2(-0.0170599f, 0.98059f),
    FfxFloat32x2(-0.442564f, 0.896737f),  FfxFloat32x2(0.48619f, 0.518723f),    FfxFloat32x2(-0.725272f, 0.419965f),   FfxFloat32x2(0.781417f, -0.624009f),
    FfxFloat32x2(-0.899227f, -0.437482f), FfxFloat32x2(0.769219f, 0.33372f),    FfxFloat32x2(-0.414411f, 0.00375378f), FfxFloat32x2(0.262856f, -0.759514f),
};

#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_CB_CLASSIFIER, std140) uniform cbClassifier_t
    {
        FfxFloat32x4 textureSize;
        FfxFloat32x3 lightDir;
        FfxFloat32 skyHeight;

        FfxFloat32x4  blockerOffset_cascadeSize_sunSizeLightSpace_pad;
        FfxUInt32x4  cascadeCount_tileTolerance_pad_pad;
        FfxFloat32x4 bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd;

        FfxFloat32x4 cascadeScale[4];
        FfxFloat32x4 cascadeOffset[4];

        // Matrices
        FfxFloat32Mat4 viewToWorld;
        FfxFloat32Mat4 lightView;
        FfxFloat32Mat4 inverseLightView;

        //#define FFX_CLASSIFIER_CONSTANT_BUFFER_1_SIZE 18 + 2 booleans +  32 +  48// Number of 32-bit values
    } cbClassifier;
#endif

FfxFloat32x4 TextureSize()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.textureSize;
#endif
    return FfxFloat32x4(0,0,0,0);
}

FfxFloat32x3 LightDir()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.lightDir;
#endif
    return FfxFloat32x3(0, 0, 0);
}

FfxFloat32 SkyHeight()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.skyHeight;
#endif
    return FfxFloat32(0);
}

FfxUInt32 CascadeCount()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.cascadeCount_tileTolerance_pad_pad[0];
#endif
    return 0;
}

FfxUInt32 TileTolerance()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.cascadeCount_tileTolerance_pad_pad[1];
#endif
    return 0;
}

FfxFloat32 BlockerOffset()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.blockerOffset_cascadeSize_sunSizeLightSpace_pad[0];
#endif
    return 0;
}

FfxFloat32 CascadeSize()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.blockerOffset_cascadeSize_sunSizeLightSpace_pad[1];
#endif
    return 0;
}

FfxFloat32 SunSizeLightSpace()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.blockerOffset_cascadeSize_sunSizeLightSpace_pad[2];
#endif
    return 0;
}

FfxBoolean RejectLitPixels()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return FfxBoolean(cbClassifier.bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd[0]);
#endif
    return false;
}

FfxBoolean UseCascadesForRayT()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return FfxBoolean(cbClassifier.bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd[1]);
#endif
    return false;
}

FfxFloat32 NormalsUnpackMul()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd[2];
#else
    return 0;
#endif
}

FfxFloat32 NormalsUnpackAdd()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd[3];
#else
    return 0;
#endif
}

FfxFloat32x4 CascadeScale(FfxUInt32 index)
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.cascadeScale[index];
#else
    return FfxFloat32x4(0,0,0,0);
#endif
}

FfxFloat32x4 CascadeOffset(FfxUInt32 index)
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.cascadeOffset[index];
#else
    return FfxFloat32x4(0,0,0,0);
#endif
}

FfxFloat32Mat4 ViewToWorld()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.viewToWorld;
#endif
    return FfxFloat32Mat4(0);
}

FfxFloat32Mat4 LightView()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.lightView;
#endif
    return FfxFloat32Mat4(0);
}

FfxFloat32Mat4 InverseLightView()
{
#if defined(FFX_CLASSIFIER_BIND_CB_CLASSIFIER)
    return cbClassifier.inverseLightView;
#endif
    return FfxFloat32Mat4(0);
}

// SRVs
#if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_DEPTH)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_SRV_INPUT_DEPTH)
    uniform texture2D r_input_depth;
#endif
#if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_NORMALS)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_SRV_INPUT_NORMALS)
    uniform texture2D r_input_normal;
#endif
#if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_SHADOW_MAPS)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_SRV_INPUT_SHADOW_MAPS)
    uniform texture2D r_input_shadowMap[4];
#endif

// UAVs
#if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES, std430)
    buffer rwsb_tiles_t
    {
        FfxUInt32x4 data[];
    } rwsb_tiles;
#endif
#if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES_COUNT)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES_COUNT, std430)
    coherent buffer rwb_tileCount_t
    {
        FfxUInt32 data[];
    } rwb_tileCount;
#endif
#if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_RAY_HIT)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_UAV_OUTPUT_RAY_HIT, r32ui)
    uniform uimage2D rwt2d_rayHitResults;
#endif
#if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_TEXTURE)
    layout (set = 0, binding = FFX_CLASSIFIER_BIND_UAV_OUTPUT_TEXTURE, rgba32f)
    uniform image2D rwt2d_output;
#endif

FfxFloat32 FfxClassifierSampleDepth(FfxUInt32x2 uiPxPos)
{
#if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_DEPTH)
    return texelFetch(r_input_depth, ivec2(uiPxPos), 0).r;
#else
    return 0.0;
#endif // defined(FFX_CLASSIFIER_BIND_SRV_INPUT_DEPTH)
}

#if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_NORMALS)
FfxFloat32x3 FfxClassifierSampleNormal(FfxUInt32x2 uiPxPos)
{
    FfxFloat32x3 normal = texelFetch(r_input_normal, ivec2(uiPxPos), 0).rgb;
    normal = normal * NormalsUnpackMul().xxx + NormalsUnpackAdd().xxx;
    return normalize(normal);
}
#endif // #if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_NORMALS)

#if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_SHADOW_MAPS)
FfxFloat32 FfxClassifierSampleShadowMap(FfxFloat32x2 sampleUV, FfxUInt32 cascadeIndex)
{
    nonuniformEXT FfxUInt32 nonUniformCascadeIndex = cascadeIndex;
    return texelFetch(r_input_shadowMap[nonUniformCascadeIndex], ivec2(sampleUV), 0).r;
}
#endif // #if defined(FFX_CLASSIFIER_BIND_SRV_INPUT_SHADOW_MAPS)

#if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_RAY_HIT)
void FfxClassifierStoreLightMask(FfxUInt32x2 index, FfxUInt32 lightMask)
{
    imageStore(rwt2d_rayHitResults, ivec2(index), FfxUInt32x4(~lightMask, 0,0,0));
}
#endif // #if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_RAY_HIT)

FfxUInt32 CountBits(const FfxUInt32 mask)
{
    return bitCount(mask);
}

#if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES) || defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES_COUNT)
void FfxClassifierStoreTile(FfxUInt32x4 uiTile)
{
    uint index = ~0;
    index = atomicAdd(rwb_tileCount.data[0], 1);
    rwsb_tiles.data[index] = uiTile;
}
#endif // #if defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES) || defined(FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES_COUNT)

#endif // #if defined(FFX_GPU)
