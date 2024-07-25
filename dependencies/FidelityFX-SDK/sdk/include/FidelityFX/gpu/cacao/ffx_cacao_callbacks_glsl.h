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

#include "ffx_cacao_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(CACAO_BIND_CB_CACAO)
    struct FFX_CACAO_Constants
    {
	    FfxFloat32x2                 DepthUnpackConsts;
	    FfxFloat32x2                 CameraTanHalfFOV;

	    FfxFloat32x2                 NDCToViewMul;
	    FfxFloat32x2                 NDCToViewAdd;

	    FfxFloat32x2                 DepthBufferUVToViewMul;
	    FfxFloat32x2                 DepthBufferUVToViewAdd;

	    FfxFloat32                   EffectRadius;                           // world (viewspace) maximum size of the shadow
	    FfxFloat32                   EffectShadowStrength;                   // global strength of the effect (0 - 5)
	    FfxFloat32                   EffectShadowPow;
	    FfxFloat32                   EffectShadowClamp;

	    FfxFloat32                   EffectFadeOutMul;                       // effect fade out from distance (ex. 25)
	    FfxFloat32                   EffectFadeOutAdd;                       // effect fade out to distance   (ex. 100)
	    FfxFloat32                   EffectHorizonAngleThreshold;            // limit errors on slopes and caused by insufficient geometry tessellation (0.05 to 0.5)
	    FfxFloat32                   EffectSamplingRadiusNearLimitRec;          // if viewspace pixel closer than this, don't enlarge shadow sampling radius anymore (makes no sense to grow beyond some distance, not enough samples to cover everything, so just limit the shadow growth; could be SSAOSettingsFadeOutFrom * 0.1 or less)

	    FfxFloat32                   DepthPrecisionOffsetMod;
	    FfxFloat32                   NegRecEffectRadius;                     // -1.0 / EffectRadius
	    FfxFloat32                   LoadCounterAvgDiv;                      // 1.0 / ( halfDepthMip[SSAO_DEPTH_MIP_LEVELS-1].sizeX * halfDepthMip[SSAO_DEPTH_MIP_LEVELS-1].sizeY )
	    FfxFloat32                   AdaptiveSampleCountLimit;

	    FfxFloat32                   InvSharpness;
	    FfxInt32                     BlurNumPasses;
	    FfxFloat32                   BilateralSigmaSquared;
	    FfxFloat32                   BilateralSimilarityDistanceSigma;

	    FfxFloat32x4                 PatternRotScaleMatrices[4][5];

	    FfxFloat32                   NormalsUnpackMul;
	    FfxFloat32                   NormalsUnpackAdd;
	    FfxFloat32                   DetailAOStrength;
	    FfxFloat32                   Dummy0;

	    FfxFloat32x2                 SSAOBufferDimensions;
	    FfxFloat32x2                 SSAOBufferInverseDimensions;

	    FfxFloat32x2                 DepthBufferDimensions;
	    FfxFloat32x2                 DepthBufferInverseDimensions;

	    FfxInt32x2                   DepthBufferOffset;
	    FfxFloat32x4                 PerPassFullResUVOffset[4];

	    FfxFloat32x2                 OutputBufferDimensions;
	    FfxFloat32x2                 OutputBufferInverseDimensions;

	    FfxFloat32x2                 ImportanceMapDimensions;
	    FfxFloat32x2                 ImportanceMapInverseDimensions;

	    FfxFloat32x2                 DeinterleavedDepthBufferDimensions;
	    FfxFloat32x2                 DeinterleavedDepthBufferInverseDimensions;

	    FfxFloat32x2                 DeinterleavedDepthBufferOffset;
	    FfxFloat32x2                 DeinterleavedDepthBufferNormalisedOffset;

	    mat4                    NormalsWorldToViewspaceMatrix;
    };

    layout (set=0, binding = CACAO_BIND_CB_CACAO) uniform SSAOConstantsBuffer_t
    {
        FFX_CACAO_Constants        g_FFX_CACAO_Consts;
    };

    #define FFX_CACAO_CONSTANT_BUFFER_1_SIZE 172  // Number of 32-bit values. This must be kept in sync with max( cbRCAS , cbSPD) size.
#endif

FfxFloat32x2                 DepthUnpackConsts(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthUnpackConsts;
#else
    return 0;
#endif
}
FfxFloat32x2                 CameraTanHalfFOV(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.CameraTanHalfFOV;
#else
    return 0;
#endif
}

FfxFloat32x2                 NDCToViewMul(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.NDCToViewMul;
#else
    return 0;
#endif
}
FfxFloat32x2                 NDCToViewAdd(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.NDCToViewAdd;
#else
    return 0;
#endif
}

FfxFloat32x2                 DepthBufferUVToViewMul(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthBufferUVToViewMul;
#else
    return 0;
#endif
}
FfxFloat32x2                 DepthBufferUVToViewAdd(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthBufferUVToViewAdd;
#else
    return 0;
#endif
}

FfxFloat32                   EffectRadius(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectRadius;
#else
    return 0;
#endif
}
FfxFloat32                   EffectShadowStrength(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectShadowStrength;
#else
    return 0;
#endif
}
FfxFloat32                   EffectShadowPow(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectShadowPow;
#else
    return 0;
#endif
}
FfxFloat32                   EffectShadowClamp(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectShadowClamp;
#else
    return 0;
#endif
}

FfxFloat32                   EffectFadeOutMul(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectFadeOutMul;
#else
    return 0;
#endif
}
FfxFloat32                   EffectFadeOutAdd(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectFadeOutAdd;
#else
    return 0;
#endif
}
FfxFloat32                   EffectHorizonAngleThreshold(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectHorizonAngleThreshold;
#else
    return 0;
#endif
}
FfxFloat32                   EffectSamplingRadiusNearLimitRec(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.EffectSamplingRadiusNearLimitRec;
#else
    return 0;
#endif
}

FfxFloat32                   DepthPrecisionOffsetMod(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthPrecisionOffsetMod;
#else
    return 0;
#endif
}
FfxFloat32                   NegRecEffectRadius(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.NegRecEffectRadius;
#else
    return 0;
#endif
}
FfxFloat32                   LoadCounterAvgDiv(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.LoadCounterAvgDiv;
#else
    return 0;
#endif
}
FfxFloat32                   AdaptiveSampleCountLimit(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.AdaptiveSampleCountLimit;
#else
    return 0;
#endif
}

FfxFloat32                   InvSharpness(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.InvSharpness;
#else
    return 0;
#endif
}
FfxInt32                     BlurNumPasses(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.BlurNumPasses;
#else
    return 0;
#endif
}
FfxFloat32                   BilateralSigmaSquared(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.BilateralSigmaSquared;
#else
    return 0;
#endif
}
FfxFloat32                   BilateralSimilarityDistanceSigma(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.BilateralSimilarityDistanceSigma;
#else
    return 0;
#endif
}

FfxFloat32x4                 PatternRotScaleMatrices(uint i, uint j){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.PatternRotScaleMatrices[i][j];
#else
    return 0;
#endif
}

FfxFloat32                   NormalsUnpackMul(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.NormalsUnpackMul;
#else
    return 0;
#endif
}
FfxFloat32                   NormalsUnpackAdd(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.NormalsUnpackAdd;
#else
    return 0;
#endif
}
FfxFloat32                   DetailAOStrength(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DetailAOStrength;
#else
    return 0;
#endif
}
FfxFloat32                   Dummy0(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.Dummy0;
#else
    return 0;
#endif
}

FfxFloat32x2                 SSAOBufferDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.SSAOBufferDimensions;
#else
    return 0;
#endif
}
FfxFloat32x2                 SSAOBufferInverseDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.SSAOBufferInverseDimensions;
#else
    return 0;
#endif
}

FfxFloat32x2                 DepthBufferDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthBufferDimensions;
#else
    return 0;
#endif
}
FfxFloat32x2                 DepthBufferInverseDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthBufferInverseDimensions;
#else
    return 0;
#endif
}

FfxInt32x2                   DepthBufferOffset(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DepthBufferOffset;
#else
    return 0;
#endif
}
FfxFloat32x4                 PerPassFullResUVOffset(uint i){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.PerPassFullResUVOffset[i];
#else
    return 0;
#endif
}

FfxFloat32x2                 OutputBufferDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.OutputBufferDimensions;
#else
    return 0;
#endif
}
FfxFloat32x2                 OutputBufferInverseDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.OutputBufferInverseDimensions;
#else
    return 0;
#endif
}

FfxFloat32x2                 ImportanceMapDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.ImportanceMapDimensions;
#else
    return 0;
#endif
}
FfxFloat32x2                 ImportanceMapInverseDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.ImportanceMapInverseDimensions;
#else
    return 0;
#endif
}

FfxFloat32x2                 DeinterleavedDepthBufferDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DeinterleavedDepthBufferDimensions;
#else
    return 0;
#endif
}
FfxFloat32x2                 DeinterleavedDepthBufferInverseDimensions(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DeinterleavedDepthBufferInverseDimensions;
#else
    return 0;
#endif
}

FfxFloat32x2                 DeinterleavedDepthBufferOffset(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DeinterleavedDepthBufferOffset;
#else
    return 0;
#endif
}
FfxFloat32x2                 DeinterleavedDepthBufferNormalisedOffset(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.DeinterleavedDepthBufferNormalisedOffset;
#else
    return 0;
#endif
}

mat4                    NormalsWorldToViewspaceMatrix(){
#if defined(CACAO_BIND_CB_CACAO)
    return g_FFX_CACAO_Consts.NormalsWorldToViewspaceMatrix;
#else
    return 0;
#endif
}

layout (set=0, binding = 1000) uniform sampler g_PointClampSampler;
layout (set=0, binding = 1001) uniform sampler g_PointMirrorSampler;
layout (set=0, binding = 1002) uniform sampler g_LinearClampSampler;
layout (set=0, binding = 1003) uniform sampler g_ViewspaceDepthTapSampler;
layout (set=0, binding = 1004) uniform sampler g_RealPointClampSampler;


#if defined CACAO_BIND_SRV_DEPTH_IN
    layout (set=0, binding = CACAO_BIND_SRV_DEPTH_IN) uniform texture2D g_DepthIn;
#endif
#if defined CACAO_BIND_SRV_NORMAL_IN
    layout (set=0, binding = CACAO_BIND_SRV_NORMAL_IN) uniform texture2D g_NormalIn;
#endif
#if defined CACAO_BIND_SRV_LOAD_COUNTER
    layout (set=0, binding = CACAO_BIND_SRV_LOAD_COUNTER) uniform utexture1D g_LoadCounter; 
#endif
#if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS
    layout (set=0, binding = CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS) uniform texture2DArray g_DeinterleavedDepth;
#endif
#if defined CACAO_BIND_SRV_DEINTERLEAVED_NORMALS
    layout (set=0, binding = CACAO_BIND_SRV_DEINTERLEAVED_NORMALS) uniform texture2DArray g_DeinterleavedNormals;
#endif
#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
    layout (set=0, binding = CACAO_BIND_SRV_SSAO_BUFFER_PING) uniform texture2DArray g_SsaoBufferPing;
#endif
#if defined CACAO_BIND_SRV_SSAO_BUFFER_PONG
    layout (set=0, binding = CACAO_BIND_SRV_SSAO_BUFFER_PONG) uniform texture2DArray g_SsaoBufferPong;
#endif
#if defined CACAO_BIND_SRV_IMPORTANCE_MAP
    layout (set=0, binding = CACAO_BIND_SRV_IMPORTANCE_MAP) uniform texture2D g_ImportanceMap;
#endif
#if defined CACAO_BIND_SRV_IMPORTANCE_MAP_PONG
    layout (set=0, binding = CACAO_BIND_SRV_IMPORTANCE_MAP_PONG) uniform texture2D g_ImportanceMapPong;
#endif

#if defined CACAO_BIND_UAV_LOAD_COUNTER
    layout (set=0, binding = CACAO_BIND_UAV_LOAD_COUNTER, r32ui) uniform uimage1D g_RwLoadCounter;
#endif
#if defined CACAO_BIND_UAV_DEINTERLEAVED_DEPTHS
    layout (set=0, binding = CACAO_BIND_UAV_DEINTERLEAVED_DEPTHS, r32f) uniform image2DArray g_RwDeinterleavedDepth;
#endif
#if defined CACAO_BIND_UAV_DEINTERLEAVED_NORMALS
    layout (set=0, binding = CACAO_BIND_UAV_DEINTERLEAVED_NORMALS, rgba32f) uniform image2DArray g_RwDeinterleavedNormals;
#endif
#if defined CACAO_BIND_UAV_SSAO_BUFFER_PING
    layout (set=0, binding = CACAO_BIND_UAV_SSAO_BUFFER_PING, rg32f) uniform image2DArray g_RwSsaoBufferPing;
#endif
#if defined CACAO_BIND_UAV_SSAO_BUFFER_PONG
    layout (set=0, binding = CACAO_BIND_UAV_SSAO_BUFFER_PONG, rg32f) uniform image2DArray g_RwSsaoBufferPong;
#endif
#if defined CACAO_BIND_UAV_IMPORTANCE_MAP
    layout (set = 0, binding = CACAO_BIND_UAV_IMPORTANCE_MAP, r32f) uniform image2D g_RwImportanceMap;
#endif
#if defined CACAO_BIND_UAV_IMPORTANCE_MAP_PONG
    layout (set = 0, binding = CACAO_BIND_UAV_IMPORTANCE_MAP_PONG, r32f) uniform image2D g_RwImportanceMapPong;
#endif
#if defined CACAO_BIND_UAV_OUTPUT
    layout (set = 0, binding = CACAO_BIND_UAV_OUTPUT, rgba16f) uniform image2D g_RwOutput;
#endif
#if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS
    layout (set = 0, binding = CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS, r32f) uniform image2DArray g_RwDepthMips[4];
#endif

// =============================================================================
// Clear Load Counter
#if defined CACAO_BIND_UAV_LOAD_COUNTER
void FFX_CACAO_ClearLoadCounter_SetLoadCounter(FfxUInt32 val)
{
    imageStore(g_RwLoadCounter, 0, FfxUInt32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_LOAD_COUNTER

// =============================================================================
// Edge Sensitive Blur
#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
FfxFloat32x2 FFX_CACAO_EdgeSensitiveBlur_SampleInput(FfxFloat32x2 uv, FfxUInt32 layerId)
{
    return textureLod(sampler2DArray(g_SsaoBufferPing, g_PointMirrorSampler), FfxFloat32x3(uv, FfxFloat32(layerId)), 0.0f).xy;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PING

#if defined CACAO_BIND_UAV_SSAO_BUFFER_PONG
void FFX_CACAO_EdgeSensitiveBlur_StoreOutput(FfxUInt32x2 coord, FfxFloat32x2 value, FfxUInt32 layerId)
{
    imageStore(g_RwSsaoBufferPong, FfxInt32x3(coord, layerId), FfxFloat32x4(value, 0, 0));
}
#endif // #if defined CACAO_BIND_UAV_SSAO_BUFFER_PONG

// =============================================================================
// SSAO Generation
#if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS
FfxFloat32 FFX_CACAO_SSAOGeneration_SampleViewspaceDepthMip(FfxFloat32x2 uv, FfxFloat32 mip, FfxUInt32 layerId)
{
    return textureLod(sampler2DArray(g_DeinterleavedDepth, g_ViewspaceDepthTapSampler), FfxFloat32x3(uv, FfxFloat32(layerId)), mip).x;
}
#endif // #if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS

#if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS
FfxFloat32x4 FFX_CACAO_SSAOGeneration_GatherViewspaceDepthOffset(FfxFloat32x2 uv, FfxInt32x2 offset, FfxUInt32 layerId)
{
    return textureGatherOffset(sampler2DArray(g_DeinterleavedDepth, g_PointMirrorSampler), FfxFloat32x3(uv, FfxFloat32(layerId)), offset, 0);
}
#endif // #if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS

#if defined CACAO_BIND_SRV_LOAD_COUNTER
FfxUInt32 FFX_CACAO_SSAOGeneration_GetLoadCounter()
{
    return FfxUInt32(texelFetch(g_LoadCounter, 0, 0).x);
}
#endif // #if defined CACAO_BIND_SRV_LOAD_COUNTER

#if defined CACAO_BIND_SRV_IMPORTANCE_MAP
FfxFloat32 FFX_CACAO_SSAOGeneration_SampleImportance(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_ImportanceMap, g_LinearClampSampler), uv, 0.0f).x;
}
#endif // #if defined CACAO_BIND_SRV_IMPORTANCE_MAP

#if defined CACAO_BIND_SRV_SSAO_BUFFER_PONG
FfxFloat32x2 FFX_CACAO_SSAOGeneration_LoadBasePassSSAOPass(FfxUInt32x2 coord, FfxUInt32 pass)
{
    return texelFetch(g_SsaoBufferPong, FfxInt32x3(coord, pass), 0).xy;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PONG

#if defined CACAO_BIND_SRV_DEINTERLEAVED_NORMALS
FfxFloat32x3 FFX_CACAO_SSAOGeneration_GetNormalPass(FfxUInt32x2 coord, FfxUInt32 pass)
{
    return texelFetch(g_DeinterleavedNormals, FfxInt32x3(coord, pass), 0).xyz;
}
#endif // #if defined CACAO_BIND_SRV_DEINTERLEAVED_NORMALS

#if defined CACAO_BIND_UAV_SSAO_BUFFER_PING
void FFX_CACAO_SSAOGeneration_StoreOutput(FfxUInt32x2 coord, FfxFloat32x2 val, FfxUInt32 layerId)
{
    imageStore(g_RwSsaoBufferPing, FfxInt32x3(coord, layerId), FfxFloat32x4(val, 0, 0));
}
#endif // #if defined CACAO_BIND_UAV_SSAO_BUFFER_PING

// ============================================================================
// Apply
#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
FfxFloat32 FFX_CACAO_Apply_SampleSSAOUVPass(FfxFloat32x2 uv, FfxUInt32 pass)
{
    return textureLod(sampler2DArray(g_SsaoBufferPing, g_LinearClampSampler), FfxFloat32x3(uv, pass), 0.0f).x;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PING

#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
FfxFloat32x2 FFX_CACAO_Apply_LoadSSAOPass(FfxUInt32x2 coord, FfxUInt32 pass)
{
    return texelFetch(g_SsaoBufferPing, FfxInt32x3(coord, pass), 0).xy;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PING

#if defined CACAO_BIND_UAV_OUTPUT
void FFX_CACAO_Apply_StoreOutput(FfxUInt32x2 coord, FfxFloat32 val)
{
    FfxFloat32x4 prev = imageLoad(g_RwOutput, FfxInt32x2(coord));
    imageStore(g_RwOutput, FfxInt32x2(coord), FfxFloat32x4(val, prev.g, prev.b, prev.a));
}
#endif // #if defined CACAO_BIND_UAV_OUTPUT

// =============================================================================
// Prepare
#if defined CACAO_BIND_SRV_DEPTH_IN
FfxFloat32x4 FFX_CACAO_Prepare_SampleDepthOffsets(FfxFloat32x2 uv)
{
    FfxFloat32x4 samples;
    samples.x = textureLodOffset(sampler2D(g_DepthIn, g_PointClampSampler), uv, 0.0f, FfxInt32x2(0, 2)).x;
    samples.y = textureLodOffset(sampler2D(g_DepthIn, g_PointClampSampler), uv, 0.0f, FfxInt32x2(2, 2)).x;
    samples.z = textureLodOffset(sampler2D(g_DepthIn, g_PointClampSampler), uv, 0.0f, FfxInt32x2(2, 0)).x;
    samples.w = textureLodOffset(sampler2D(g_DepthIn, g_PointClampSampler), uv, 0.0f, FfxInt32x2(0, 0)).x;
    return samples;
}
#endif // #if defined CACAO_BIND_SRV_DEPTH_IN

#if defined CACAO_BIND_SRV_DEPTH_IN
FfxFloat32x4 FFX_CACAO_Prepare_GatherDepth(FfxFloat32x2 uv)
{
    return textureGather(sampler2D(g_DepthIn, g_PointClampSampler), uv, 0);
}
#endif // #if defined CACAO_BIND_SRV_DEPTH_IN

#if defined CACAO_BIND_SRV_DEPTH_IN
FfxFloat32 FFX_CACAO_Prepare_LoadDepth(FfxUInt32x2 coord)
{
    return texelFetch(g_DepthIn, FfxInt32x2(coord), 0).x;
}
#endif // #if defined CACAO_BIND_SRV_DEPTH_IN

#if defined CACAO_BIND_SRV_DEPTH_IN
FfxFloat32 FFX_CACAO_Prepare_LoadDepthOffset(FfxUInt32x2 coord, FfxInt32x2 offset)
{
    return texelFetch(g_DepthIn, FfxInt32x2(coord) + offset, 0).x; //CACAO_TODO
}
#endif // #if defined CACAO_BIND_SRV_DEPTH_IN

#if defined CACAO_BIND_SRV_DEPTH_IN
FfxFloat32x4 FFX_CACAO_Prepare_GatherDepthOffset(FfxFloat32x2 uv, FfxInt32x2 offset)
{
    return textureGatherOffset(sampler2D(g_DepthIn, g_PointClampSampler), uv, offset, 0);
}
#endif // #if defined CACAO_BIND_SRV_DEPTH_IN

#if defined CACAO_BIND_SRV_NORMAL_IN
FfxFloat32x3 FFX_CACAO_Prepare_LoadNormal(FfxUInt32x2 coord)
{
    FfxFloat32x3 normal = texelFetch(g_NormalIn, FfxInt32x2(coord), 0).xyz;
    normal = normal * NormalsUnpackMul().xxx + NormalsUnpackAdd().xxx;
    normal = FFX_TRANSFORM_VECTOR(normal, FfxFloat32x3x3(NormalsWorldToViewspaceMatrix())).xyz;
    return normal;
}
#endif // #if defined CACAO_BIND_SRV_NORMAL_IN

#if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS
void FFX_CACAO_Prepare_StoreDepthMip0(FfxUInt32x2 coord, FfxUInt32 index, FfxFloat32 val)
{
    imageStore(g_RwDepthMips[0], FfxInt32x3(coord, index), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS

#if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS
void FFX_CACAO_Prepare_StoreDepthMip1(FfxUInt32x2 coord, FfxUInt32 index, FfxFloat32 val)
{
    imageStore(g_RwDepthMips[1], FfxInt32x3(coord, index), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS

#if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS
void FFX_CACAO_Prepare_StoreDepthMip2(FfxUInt32x2 coord, FfxUInt32 index, FfxFloat32 val)
{
    imageStore(g_RwDepthMips[2], FfxInt32x3(coord, index), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS

#if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS
void FFX_CACAO_Prepare_StoreDepthMip3(FfxUInt32x2 coord, FfxUInt32 index, FfxFloat32 val)
{
    imageStore(g_RwDepthMips[3], FfxInt32x3(coord, index), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_DEPTH_DOWNSAMPLED_MIPS

#if defined CACAO_BIND_UAV_DEINTERLEAVED_DEPTHS
void FFX_CACAO_Prepare_StoreDepth(FfxUInt32x2 coord, FfxUInt32 index, FfxFloat32 val)
{
    imageStore(g_RwDeinterleavedDepth, FfxInt32x3(coord, index), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_DEINTERLEAVED_DEPTHS

#if defined CACAO_BIND_UAV_DEINTERLEAVED_DEPTHS
void FFX_CACAO_Prepare_StoreNormal(FfxUInt32x2 coord, FfxUInt32 index, FfxFloat32x3 normal)
{
    imageStore(g_RwDeinterleavedNormals, FfxInt32x3(coord, index), FfxFloat32x4(normal, 1.0f));
}
#endif // #if defined CACAO_BIND_UAV_DEINTERLEAVED_DEPTHS

// =============================================================================
// Importance Map
#if defined CACAO_BIND_SRV_SSAO_BUFFER_PONG
FfxFloat32x4 FFX_CACAO_Importance_GatherSSAO(FfxFloat32x2 uv, FfxUInt32 index)
{
    return textureGather(sampler2DArray(g_SsaoBufferPong, g_PointClampSampler), FfxFloat32x3(uv, index), 0);
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PONG

#if defined CACAO_BIND_UAV_IMPORTANCE_MAP
void FFX_CACAO_Importance_StoreImportance(FfxUInt32x2 coord, FfxFloat32 val)
{
    imageStore(g_RwImportanceMap, FfxInt32x2(coord), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_IMPORTANCE_MAP

#if defined CACAO_BIND_SRV_IMPORTANCE_MAP
FfxFloat32 FFX_CACAO_Importance_SampleImportanceA(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_ImportanceMap, g_LinearClampSampler), uv, 0.0f).x;
}
#endif // #if defined CACAO_BIND_SRV_IMPORTANCE_MAP

#if defined CACAO_BIND_UAV_IMPORTANCE_MAP_PONG
void FFX_CACAO_Importance_StoreImportanceA(FfxUInt32x2 coord, FfxFloat32 val)
{
    imageStore(g_RwImportanceMapPong, FfxInt32x2(coord), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_IMPORTANCE_MAP_PONG

#if defined CACAO_BIND_SRV_IMPORTANCE_MAP_PONG
FfxFloat32 FFX_CACAO_Importance_SampleImportanceB(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_ImportanceMapPong, g_LinearClampSampler), uv, 0.0f).x;
}
#endif // #if defined CACAO_BIND_SRV_IMPORTANCE_MAP_PONG

#if defined CACAO_BIND_UAV_IMPORTANCE_MAP
void FFX_CACAO_Importance_StoreImportanceB(FfxUInt32x2 coord, FfxFloat32 val)
{
    imageStore(g_RwImportanceMap, FfxInt32x2(coord), FfxFloat32x4(val, val, val, val));
}
#endif // #if defined CACAO_BIND_UAV_IMPORTANCE_MAP

#if defined CACAO_BIND_UAV_LOAD_COUNTER
void FFX_CACAO_Importance_LoadCounterInterlockedAdd(FfxUInt32 val)
{
    imageAtomicAdd(g_RwLoadCounter, 0, val);
}
#endif // #if defined CACAO_BIND_UAV_LOAD_COUNTER

// =============================================================================
// Bilateral Upscale

// These resources ping/pong which is handled by schedule dispatch
#if defined CACAO_BIND_UAV_OUTPUT
void FFX_CACAO_BilateralUpscale_StoreOutput(FfxUInt32x2 coord, FfxInt32x2 offset, FfxFloat32 val)
{
    FfxFloat32x4 prev = imageLoad(g_RwOutput, FfxInt32x2(coord) + offset);
    imageStore(g_RwOutput, FfxInt32x2(coord) + offset, FfxFloat32x4(val, prev.g, prev.b, prev.a));
}
#endif // #if defined CACAO_BIND_UAV_OUTPUT

#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
FfxFloat32 FFX_CACAO_BilateralUpscale_SampleSSAOLinear(FfxFloat32x2 uv, FfxUInt32 index)
{
    return textureLod(sampler2DArray(g_SsaoBufferPing, g_LinearClampSampler), FfxFloat32x3(uv, index), 0).r;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PING

#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
FfxFloat32 FFX_CACAO_BilateralUpscale_SampleSSAOPoint(FfxFloat32x2 uv, FfxUInt32 index)
{
    return textureLod(sampler2DArray(g_SsaoBufferPing, g_PointClampSampler), FfxFloat32x3(uv, index), 0).r;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PING

#if defined CACAO_BIND_SRV_SSAO_BUFFER_PING
FfxFloat32x2 FFX_CACAO_BilateralUpscale_LoadSSAO(FfxUInt32x2 coord, FfxUInt32 index)
{
    return texelFetch(g_SsaoBufferPing, FfxInt32x3(coord, index), 0).rg;
}
#endif // #if defined CACAO_BIND_SRV_SSAO_BUFFER_PING

#if defined CACAO_BIND_SRV_DEPTH_IN
FfxFloat32x4 FFX_CACAO_BilateralUpscale_LoadDepths(FfxUInt32x2 coord)
{
    FfxFloat32x4 depths;
    depths.x = texelFetchOffset(g_DepthIn, FfxInt32x2(coord), 0, FfxInt32x2(0, 0)).x;
    depths.y = texelFetchOffset(g_DepthIn, FfxInt32x2(coord), 0, FfxInt32x2(1, 0)).x;
    depths.z = texelFetchOffset(g_DepthIn, FfxInt32x2(coord), 0, FfxInt32x2(0, 1)).x;
    depths.w = texelFetchOffset(g_DepthIn, FfxInt32x2(coord), 0, FfxInt32x2(1, 1)).x;
    return depths;
}
#endif // #if defined CACAO_BIND_SRV_DEPTH_IN

#if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS
FfxFloat32 FFX_CACAO_BilateralUpscale_LoadDownscaledDepth(FfxUInt32x2 coord, FfxUInt32 index)
{
    return texelFetch(g_DeinterleavedDepth, FfxInt32x3(coord, index), 0).x;
}
#endif // #if defined CACAO_BIND_SRV_DEINTERLEAVED_DEPTHS

#endif // #if defined(FFX_GPU)
