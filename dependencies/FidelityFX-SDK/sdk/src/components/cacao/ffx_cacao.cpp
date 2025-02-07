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

#include <cstring>   // memcpy
#include <cmath>     // cos, sin
#include <stdexcept>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#endif

#include <FidelityFX/host/ffx_cacao.h>
#include <FidelityFX/gpu/ffx_core.h>
#include <ffx_object_management.h>

#include "ffx_cacao_private.h"

#define FFX_CACAO_ASSERT(exp)                assert(exp)
#define FFX_CACAO_ARRAY_SIZE(xs)             (sizeof(xs) / sizeof(xs[0]))
#define FFX_CACAO_COS(x)                     cosf(x)
#define FFX_CACAO_SIN(x)                     sinf(x)
#define FFX_CACAO_MIN(x, y)                  (((x) < (y)) ? (x) : (y))
#define FFX_CACAO_MAX(x, y)                  (((x) > (y)) ? (x) : (y))
#define FFX_CACAO_CLAMP(value, lower, upper) FFX_CACAO_MIN(FFX_CACAO_MAX(value, lower), upper)

#define MATRIX_ROW_MAJOR_ORDER 1
#define MAX_BLUR_PASSES 8

static const FfxFloat32x4x4 FFX_CACAO_IDENTITY_MATRIX = {
    
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
    
};


inline static uint32_t dispatchSize(uint32_t tileSize, uint32_t totalSize)
{
    return FFX_DIVIDE_ROUNDING_UP(totalSize, tileSize);
}

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t index;
    wchar_t  name[64];
} ResourceBinding;

static const ResourceBinding constantBufferBindingTable[] = {
    {FFX_CACAO_CONSTANTBUFFER_IDENTIFIER_CACAO, L"SSAOConstantsBuffer"},
};

static const ResourceBinding srvTextureBindingTable[] = {
    {FFX_CACAO_RESOURCE_IDENTIFIER_DEPTH_IN, L"g_DepthIn"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_NORMAL_IN, L"g_NormalIn"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_LOAD_COUNTER_BUFFER, L"g_LoadCounter"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_DEPTHS, L"g_DeinterleavedDepth"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_NORMALS, L"g_DeinterleavedNormals"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PING, L"g_SsaoBufferPing"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PONG, L"g_SsaoBufferPong"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP, L"g_ImportanceMap"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP_PONG, L"g_ImportanceMapPong"},
};

static const ResourceBinding uavTextureBindingTable[] = {
    {FFX_CACAO_RESOURCE_IDENTIFIER_LOAD_COUNTER_BUFFER, L"g_RwLoadCounter"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_DEPTHS, L"g_RwDeinterleavedDepth"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_NORMALS, L"g_RwDeinterleavedNormals"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PING, L"g_RwSsaoBufferPing"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PONG, L"g_RwSsaoBufferPong"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP, L"g_RwImportanceMap"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP_PONG, L"g_RwImportanceMapPong"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_OUTPUT, L"g_RwOutput"},
    {FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_DEPTH_MIPMAP_0, L"g_RwDepthMips"},
};

void ffxCacaoUpdateBufferSizeInfo(const uint32_t width, const uint32_t height, const bool useDownsampledSsao, FfxCacaoBufferSizeInfo* bsi)
{
    const uint32_t halfWidth     = (width + 1) / 2;
    const uint32_t halfHeight    = (height + 1) / 2;
    const uint32_t quarterWidth  = (halfWidth + 1) / 2;
    const uint32_t quarterHeight = (halfHeight + 1) / 2;
    const uint32_t eighthWidth   = (quarterWidth + 1) / 2;
    const uint32_t eighthHeight  = (quarterHeight + 1) / 2;

    const uint32_t depthBufferWidth         = width;
    const uint32_t depthBufferHeight        = height;
    const uint32_t depthBufferHalfWidth     = halfWidth;
    const uint32_t depthBufferHalfHeight    = halfHeight;
    const uint32_t depthBufferQuarterWidth  = quarterWidth;
    const uint32_t depthBufferQuarterHeight = quarterHeight;

    const uint32_t depthBufferXOffset        = 0;
    const uint32_t depthBufferYOffset        = 0;
    const uint32_t depthBufferHalfXOffset    = 0;
    const uint32_t depthBufferHalfYOffset    = 0;
    const uint32_t depthBufferQuarterXOffset = 0;
    const uint32_t depthBufferQuarterYOffset = 0;

    bsi->inputOutputBufferWidth  = width;
    bsi->inputOutputBufferHeight = height;
    bsi->depthBufferXOffset      = depthBufferXOffset;
    bsi->depthBufferYOffset      = depthBufferYOffset;
    bsi->depthBufferWidth        = depthBufferWidth;
    bsi->depthBufferHeight       = depthBufferHeight;

    if (useDownsampledSsao)
    {
        bsi->ssaoBufferWidth                 = quarterWidth;
        bsi->ssaoBufferHeight                = quarterHeight;
        bsi->deinterleavedDepthBufferXOffset = depthBufferQuarterXOffset;
        bsi->deinterleavedDepthBufferYOffset = depthBufferQuarterYOffset;
        bsi->deinterleavedDepthBufferWidth   = depthBufferQuarterWidth;
        bsi->deinterleavedDepthBufferHeight  = depthBufferQuarterHeight;
        bsi->importanceMapWidth              = eighthWidth;
        bsi->importanceMapHeight             = eighthHeight;
        bsi->downsampledSsaoBufferWidth      = halfWidth;
        bsi->downsampledSsaoBufferHeight     = halfHeight;
    }
    else
    {
        bsi->ssaoBufferWidth                 = halfWidth;
        bsi->ssaoBufferHeight                = halfHeight;
        bsi->deinterleavedDepthBufferXOffset = depthBufferHalfXOffset;
        bsi->deinterleavedDepthBufferYOffset = depthBufferHalfYOffset;
        bsi->deinterleavedDepthBufferWidth   = depthBufferHalfWidth;
        bsi->deinterleavedDepthBufferHeight  = depthBufferHalfHeight;
        bsi->importanceMapWidth              = quarterWidth;
        bsi->importanceMapHeight             = quarterHeight;
        bsi->downsampledSsaoBufferWidth      = 1;
        bsi->downsampledSsaoBufferHeight     = 1;
    }
}

void ffxCacaoUpdateConstants(FfxCacaoConstants*            consts,
                             const FfxCacaoSettings*       settings,
                             const FfxCacaoBufferSizeInfo* bufferSizeInfo,
                             const FfxFloat32x4x4*         proj,
                             const FfxFloat32x4x4*         normalsToView,
                             const float                   NormalUnPackMul,
                             const float                   NormalUnPackAdd)
{
    consts->BilateralSigmaSquared            = settings->bilateralSigmaSquared;
    consts->BilateralSimilarityDistanceSigma = settings->bilateralSimilarityDistanceSigma;

    if (settings->generateNormals)
    {
        memcpy(consts->NormalsWorldToViewspaceMatrix, FFX_CACAO_IDENTITY_MATRIX, sizeof(consts->NormalsWorldToViewspaceMatrix));
    }
    else
    {
        memcpy(consts->NormalsWorldToViewspaceMatrix, *normalsToView, sizeof(consts->NormalsWorldToViewspaceMatrix));
    }
    // used to get average load per pixel; 9.0 is there to compensate for only doing every 9th InterlockedAdd in PSPostprocessImportanceMapB for performance reasons
    consts->LoadCounterAvgDiv = 9.0f / (float)(bufferSizeInfo->importanceMapWidth * bufferSizeInfo->importanceMapHeight * 255.0);

    const float depthLinearizeMul = (MATRIX_ROW_MAJOR_ORDER) ? (-(*proj)[14])
                                                       : (-(*proj)[11]);  // float depthLinearizeMul = ( clipFar * clipNear ) / ( clipFar - clipNear );
    float depthLinearizeAdd = (MATRIX_ROW_MAJOR_ORDER) ? ((*proj)[10]) 
                                                       : ((*proj)[10]);  // float depthLinearizeAdd = clipFar / ( clipFar - clipNear );
    // correct the handedness issue. need to make sure this below is correct, but I think it is.
    if (depthLinearizeMul * depthLinearizeAdd < 0)
        depthLinearizeAdd = -depthLinearizeAdd;
    consts->DepthUnpackConsts[0] = depthLinearizeMul;
    consts->DepthUnpackConsts[1] = depthLinearizeAdd;

    const float tanHalfFOVY           = 1.0f / (*proj)[5];  // = tanf( drawContext.Camera.GetYFOV( ) * 0.5f );
    const float tanHalfFOVX           = 1.0F / (*proj)[0];  // = tanHalfFOVY * drawContext.Camera.GetAspect( );
    consts->CameraTanHalfFOV[0] = tanHalfFOVX;
    consts->CameraTanHalfFOV[1] = tanHalfFOVY;

    consts->NDCToViewMul[0] = consts->CameraTanHalfFOV[0] * 2.0f;
    consts->NDCToViewMul[1] = consts->CameraTanHalfFOV[1] * -2.0f;
    consts->NDCToViewAdd[0] = consts->CameraTanHalfFOV[0] * -1.0f;
    consts->NDCToViewAdd[1] = consts->CameraTanHalfFOV[1] * 1.0f;

    const float ratio  = ((float)bufferSizeInfo->inputOutputBufferWidth) / ((float)bufferSizeInfo->depthBufferWidth);
    const float border = (1.0f - ratio) / 2.0f;
    for (int i = 0; i < FFX_CACAO_ARRAY_SIZE(consts->DepthBufferUVToViewMul); ++i)
    {
        consts->DepthBufferUVToViewMul[i] = consts->NDCToViewMul[i] / ratio;
        consts->DepthBufferUVToViewAdd[i] = consts->NDCToViewAdd[i] - consts->NDCToViewMul[i] * border / ratio;
    }

    consts->EffectRadius                = FFX_CACAO_CLAMP(settings->radius, 0.0f, 100000.0f);
    consts->EffectShadowStrength        = FFX_CACAO_CLAMP(settings->shadowMultiplier * 4.3f, 0.0f, 10.0f);
    consts->EffectShadowPow             = FFX_CACAO_CLAMP(settings->shadowPower, 0.0f, 10.0f);
    consts->EffectShadowClamp           = FFX_CACAO_CLAMP(settings->shadowClamp, 0.0f, 1.0f);
    consts->EffectFadeOutMul            = -1.0f / (settings->fadeOutTo - settings->fadeOutFrom);
    consts->EffectFadeOutAdd            = settings->fadeOutFrom / (settings->fadeOutTo - settings->fadeOutFrom) + 1.0f;
    consts->EffectHorizonAngleThreshold = FFX_CACAO_CLAMP(settings->horizonAngleThreshold, 0.0f, 1.0f);

    // 1.2 seems to be around the best trade off - 1.0 means on-screen radius will stop/slow growing when the camera is at 1.0 distance, so, depending on FOV, basically filling up most of the screen
    // This setting is viewspace-dependent and not screen size dependent intentionally, so that when you change FOV the effect stays (relatively) similar.
    float effectSamplingRadiusNearLimit = (settings->radius * 1.2f);

    // if the depth precision is switched to 32bit float, this can be set to something closer to 1 (0.9999 is fine)
    consts->DepthPrecisionOffsetMod = 0.9992f;

    // Special settings for lowest quality level - just nerf the effect a tiny bit
    if (settings->qualityLevel <= FFX_CACAO_QUALITY_LOW)
    {
        //consts.EffectShadowStrength     *= 0.9f;
        effectSamplingRadiusNearLimit *= 1.50f;

        if (settings->qualityLevel == FFX_CACAO_QUALITY_LOWEST)
            consts->EffectRadius *= 0.8f;
    }

    effectSamplingRadiusNearLimit /= tanHalfFOVY;  // to keep the effect same regardless of FOV

    consts->EffectSamplingRadiusNearLimitRec = 1.0f / effectSamplingRadiusNearLimit;

    consts->AdaptiveSampleCountLimit = settings->adaptiveQualityLimit;

    consts->NegRecEffectRadius = -1.0f / consts->EffectRadius;

    consts->InvSharpness = FFX_CACAO_CLAMP(1.0f - settings->sharpness, 0.0f, 1.0f);

    consts->DetailAOStrength = settings->detailShadowStrength;

    // set buffer size constants.
    consts->SSAOBufferDimensions[0]        = (float)bufferSizeInfo->ssaoBufferWidth;
    consts->SSAOBufferDimensions[1]        = (float)bufferSizeInfo->ssaoBufferHeight;
    consts->SSAOBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->ssaoBufferWidth);
    consts->SSAOBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->ssaoBufferHeight);

    consts->DepthBufferDimensions[0]        = (float)bufferSizeInfo->depthBufferWidth;
    consts->DepthBufferDimensions[1]        = (float)bufferSizeInfo->depthBufferHeight;
    consts->DepthBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->depthBufferWidth);
    consts->DepthBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->depthBufferHeight);

    consts->DepthBufferOffset[0] = bufferSizeInfo->depthBufferXOffset;
    consts->DepthBufferOffset[1] = bufferSizeInfo->depthBufferYOffset;

    consts->InputOutputBufferDimensions[0]        = (float)bufferSizeInfo->inputOutputBufferWidth;
    consts->InputOutputBufferDimensions[1]        = (float)bufferSizeInfo->inputOutputBufferHeight;
    consts->InputOutputBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->inputOutputBufferWidth);
    consts->InputOutputBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->inputOutputBufferHeight);

    consts->ImportanceMapDimensions[0]        = (float)bufferSizeInfo->importanceMapWidth;
    consts->ImportanceMapDimensions[1]        = (float)bufferSizeInfo->importanceMapHeight;
    consts->ImportanceMapInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->importanceMapWidth);
    consts->ImportanceMapInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->importanceMapHeight);

    consts->DeinterleavedDepthBufferDimensions[0]        = (float)bufferSizeInfo->deinterleavedDepthBufferWidth;
    consts->DeinterleavedDepthBufferDimensions[1]        = (float)bufferSizeInfo->deinterleavedDepthBufferHeight;
    consts->DeinterleavedDepthBufferInverseDimensions[0] = 1.0f / ((float)bufferSizeInfo->deinterleavedDepthBufferWidth);
    consts->DeinterleavedDepthBufferInverseDimensions[1] = 1.0f / ((float)bufferSizeInfo->deinterleavedDepthBufferHeight);

    consts->DeinterleavedDepthBufferOffset[0] = (float)bufferSizeInfo->deinterleavedDepthBufferXOffset;
    consts->DeinterleavedDepthBufferOffset[1] = (float)bufferSizeInfo->deinterleavedDepthBufferYOffset;
    consts->DeinterleavedDepthBufferNormalisedOffset[0] =
        ((float)bufferSizeInfo->deinterleavedDepthBufferXOffset) / ((float)bufferSizeInfo->deinterleavedDepthBufferWidth);
    consts->DeinterleavedDepthBufferNormalisedOffset[1] =
        ((float)bufferSizeInfo->deinterleavedDepthBufferYOffset) / ((float)bufferSizeInfo->deinterleavedDepthBufferHeight);

    consts->NormalsUnpackMul = NormalUnPackMul;
    consts->NormalsUnpackAdd = NormalUnPackAdd;

    for (int passId = 0; passId < FFX_CACAO_ARRAY_SIZE(consts->PerPassFullResUVOffset) / 4; ++passId)
    {
        consts->PerPassFullResUVOffset[4 * passId + 0] = ((float)(passId % 2)) / (float)bufferSizeInfo->ssaoBufferWidth;
        consts->PerPassFullResUVOffset[4 * passId + 1] = ((float)(passId / 2)) / (float)bufferSizeInfo->ssaoBufferHeight;
        consts->PerPassFullResUVOffset[4 * passId + 2] = 0;
        consts->PerPassFullResUVOffset[4 * passId + 3] = 0;
    }

    consts->BlurNumPasses = settings->qualityLevel == FFX_CACAO_QUALITY_LOWEST ? 2 : 4;

    const float additionalAngleOffset =
        settings
            ->temporalSupersamplingAngleOffset;  // if using temporal supersampling approach (like "Progressive Rendering Using Multi-frame Sampling" from GPU Pro 7, etc.)
    const float additionalRadiusScale =
        settings
            ->temporalSupersamplingRadiusOffset;  // if using temporal supersampling approach (like "Progressive Rendering Using Multi-frame Sampling" from GPU Pro 7, etc.)
            
    const int spmap[5]{0, 1, 4, 3, 2};
    const float PI = 3.1415926535897932384626433832795f;
    const int subPassCount = FFX_CACAO_ARRAY_SIZE(consts->PatternRotScaleMatrices[0]);
    for (int passId = 0; passId < FFX_CACAO_ARRAY_SIZE(consts->PatternRotScaleMatrices); ++passId)
    {
        for (int subPass = 0; subPass < subPassCount; subPass++)
        {
            const int a = passId;

            const int b = spmap[subPass];

            float ca, sa;
            const float angle0 = ((float)a + (float)b / (float)subPassCount) * (PI) * 0.5f;

            ca = FFX_CACAO_COS(angle0);
            sa = FFX_CACAO_SIN(angle0);

            const float scale = 1.0f + (a - 1.5f + (b - (subPassCount - 1.0f) * 0.5f) / (float)subPassCount) * 0.07f;

            consts->PatternRotScaleMatrices[passId][subPass][0] = scale * ca;
            consts->PatternRotScaleMatrices[passId][subPass][1] = scale * -sa;
            consts->PatternRotScaleMatrices[passId][subPass][2] = -scale * sa;
            consts->PatternRotScaleMatrices[passId][subPass][3] = -scale * ca;
        }
    }
}

// =================================================================================
// Interface
// =================================================================================

static FfxErrorCode patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    for (uint32_t srvIndex = 0; srvIndex < inoutPipeline->srvTextureCount; ++srvIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_CACAO_ARRAY_SIZE(srvTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvTextureBindingTable[mapIndex].name, inoutPipeline->srvTextureBindings[srvIndex].name))
                break;
        }
        if (mapIndex == FFX_CACAO_ARRAY_SIZE(srvTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->srvTextureBindings[srvIndex].resourceIdentifier = srvTextureBindingTable[mapIndex].index;
    }

    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavTextureCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_CACAO_ARRAY_SIZE(uavTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavTextureBindingTable[mapIndex].name, inoutPipeline->uavTextureBindings[uavIndex].name))
                break;
        }
        if (mapIndex == FFX_CACAO_ARRAY_SIZE(uavTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavTextureBindings[uavIndex].resourceIdentifier = uavTextureBindingTable[mapIndex].index;
    }

    for (uint32_t cbIndex = 0; cbIndex < inoutPipeline->constCount; ++cbIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < FFX_CACAO_ARRAY_SIZE(constantBufferBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(constantBufferBindingTable[mapIndex].name, inoutPipeline->constantBufferBindings[cbIndex].name))
                break;
        }
        if (mapIndex == FFX_CACAO_ARRAY_SIZE(constantBufferBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->constantBufferBindings[cbIndex].resourceIdentifier = constantBufferBindingTable[mapIndex].index;
    }

    return FFX_OK;
}

static uint32_t getPipelinePermutationFlags(const uint32_t contextFlags, const bool fp16, const bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? CACAO_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (contextFlags & FFX_CACAO_ENABLE_APPLY_SMART) ? CACAO_SHADER_PERMUTATION_APPLY_SMART : 0;
    flags |= (fp16) ? CACAO_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    return flags;
}

static FfxErrorCode createCacaoPipeline(FfxCacaoContext_Private* context, const FfxPipelineDescription &pipelineDescription, FfxPipelineState *pipelineState, const FfxPass pass,  const bool fp16, const bool canForceWave64, const bool applySmart = false){
    
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(
        &context->contextDescription.backendInterface,
        FFX_EFFECT_CACAO,
        pass,
        getPipelinePermutationFlags(pipelineDescription.contextFlags | (applySmart ? FFX_CACAO_ENABLE_APPLY_SMART : 0), fp16, canForceWave64),
        &pipelineDescription,
        context->effectContextId,
        pipelineState));

    patchResourceBindings(pipelineState);

    return FFX_OK;
}

static FfxErrorCode createPipelineStates(FfxCacaoContext_Private* context)
{
    FFX_ASSERT(context);

    // Static Samplers
    const size_t          samplerCount = 5;
    const FfxSamplerDescription samplers[samplerCount] = {
        //Sampler 0
        { 
            FFX_FILTER_TYPE_MINMAGMIP_POINT,  //filter
            FFX_ADDRESS_MODE_CLAMP,           //addressModeU
            FFX_ADDRESS_MODE_CLAMP,           //addressModeV
            FFX_ADDRESS_MODE_CLAMP,           //addressModeW
            FFX_BIND_COMPUTE_SHADER_STAGE,    //stage
        },
        //Sampler 1
        {
            FFX_FILTER_TYPE_MINMAGMIP_POINT,  //filter
            FFX_ADDRESS_MODE_MIRROR,          //addressModeU
            FFX_ADDRESS_MODE_MIRROR,          //addressModeV
            FFX_ADDRESS_MODE_MIRROR,          //addressModeW
            FFX_BIND_COMPUTE_SHADER_STAGE,    //stage
        },
        //Sampler 2
        {
            FFX_FILTER_TYPE_MINMAGMIP_LINEAR,  //filter
            FFX_ADDRESS_MODE_CLAMP,            //addressModeU
            FFX_ADDRESS_MODE_CLAMP,            //addressModeV
            FFX_ADDRESS_MODE_CLAMP,            //addressModeW
            FFX_BIND_COMPUTE_SHADER_STAGE,     //stage
        },
        //Sampler 3
        {
            FFX_FILTER_TYPE_MINMAGMIP_POINT,  //filter
            FFX_ADDRESS_MODE_CLAMP,           //addressModeU
            FFX_ADDRESS_MODE_CLAMP,           //addressModeV
            FFX_ADDRESS_MODE_CLAMP,           //addressModeW
            FFX_BIND_COMPUTE_SHADER_STAGE,    //stage
        },
        //Sampler 4
        {
            FFX_FILTER_TYPE_MINMAGMIP_POINT,  //filter
            FFX_ADDRESS_MODE_BORDER,          //addressModeU
            FFX_ADDRESS_MODE_BORDER,          //addressModeV
            FFX_ADDRESS_MODE_BORDER,          //addressModeW
            FFX_BIND_COMPUTE_SHADER_STAGE     //stage
        }
    };

    FfxPipelineDescription pipelineDescription{};
    pipelineDescription.contextFlags = 0;
    pipelineDescription.samplerCount = samplerCount;
    pipelineDescription.samplers     = samplers;

    // Root constants
    pipelineDescription.rootConstantBufferCount = 1;
    const FfxRootConstantDescription rootConstantDesc = {sizeof(FfxCacaoConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE};
    pipelineDescription.rootConstants           = &rootConstantDesc;

    // Query device capabilities
    FfxDeviceCapabilities capabilities;
    context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &capabilities);

    // Setup a few options used to determine permutation flags
    const bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    const bool supportedFP16     = capabilities.fp16Supported;

    bool canForceWave64 = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
    {
        canForceWave64 = haveShaderModel66;
    }
    else
    {
        canForceWave64 = false;
    }

    // Set up pipeline descriptors (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"CACAO-CLEAR_LOAD_COUNTER");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineClearLoadCounter, FFX_CACAO_PASS_CLEAR_LOAD_COUNTER, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_DOWNSAMPLED_DEPTHS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareDownsampledDepths, FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_DEPTHS, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_NATIVE_DEPTHS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareNativeDepths, FFX_CACAO_PASS_PREPARE_NATIVE_DEPTHS, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_DOWNSAMPLED_DEPTHS_AND_MIPS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareDownsampledDepthsAndMips, FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_DEPTHS_AND_MIPS, false, canForceWave64);

    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareNativeDepthsAndMips, FFX_CACAO_PASS_PREPARE_NATIVE_DEPTHS_AND_MIPS, false, canForceWave64);
    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_NATIVE_DEPTHS_AND_MIPS");

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_DOWNSAMPLED_DEPTHS_HALF");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareDownsampledDepthsHalf, FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_DEPTHS_HALF, supportedFP16, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_NATIVE_DEPTHS_HALF");
    createCacaoPipeline(context, pipelineDescription,  &context->pipelinePrepareNativeDepthsHalf, FFX_CACAO_PASS_PREPARE_NATIVE_DEPTHS_HALF, supportedFP16, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_DOWNSAMPLED_NORMALS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareDownsampledNormals, FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_NORMALS, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_NATIVE_NORMALS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareNativeNormals, FFX_CACAO_PASS_PREPARE_NATIVE_NORMALS, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_DOWNSAMPLED_NORMALS_FROM_INPUT_NORMALS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareDownsampledNormalsFromInputNormals, FFX_CACAO_PASS_PREPARE_DOWNSAMPLED_NORMALS_FROM_INPUT_NORMALS, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-PREPARE_NATIVE_NORMALS_FROM_INPUT_NORMALS");
    createCacaoPipeline(context, pipelineDescription, &context->pipelinePrepareNativeNormalsFromInputNormals, FFX_CACAO_PASS_PREPARE_NATIVE_NORMALS_FROM_INPUT_NORMALS, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-GENERATE_Q0");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineGenerateQ[0], FFX_CACAO_PASS_GENERATE_Q0, false, canForceWave64);
    
    wcscpy_s(pipelineDescription.name, L"CACAO-GENERATE_Q1");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineGenerateQ[1], FFX_CACAO_PASS_GENERATE_Q1, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-GENERATE_Q2");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineGenerateQ[2], FFX_CACAO_PASS_GENERATE_Q2, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-GENERATE_Q3");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineGenerateQ[3], FFX_CACAO_PASS_GENERATE_Q3, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-GENERATE_Q3_BASE");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineGenerateQ[4], FFX_CACAO_PASS_GENERATE_Q3_BASE, false, canForceWave64);
    
    wcscpy_s(pipelineDescription.name, L"CACAO-GENERATE_IMPORTANCE_MAP");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineGenerateImportanceMap, FFX_CACAO_PASS_GENERATE_IMPORTANCE_MAP, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-POST_PROCESS_IMPORTANCE_MAP_A");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineProcessImportanceMapA, FFX_CACAO_PASS_POST_PROCESS_IMPORTANCE_MAP_A, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-POST_PROCESS_IMPORTANCE_MAP_B");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineProcessImportanceMapB, FFX_CACAO_PASS_POST_PROCESS_IMPORTANCE_MAP_B, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_1");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[0], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_1, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_2");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[1], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_2, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_3");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[2], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_3, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_4");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[3], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_4, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_5");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[4], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_5, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_6");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[5], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_6, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_7");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[6], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_7, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-EDGE_SENSITIVE_BLUR_8");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineEdgeSensitiveBlur[7], FFX_CACAO_PASS_EDGE_SENSITIVE_BLUR_8, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-APPLY_NON_SMART_HALF");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineApplyNonSmartHalf, FFX_CACAO_PASS_APPLY_NON_SMART_HALF, supportedFP16, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-APPLY_NON_SMART");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineApplyNonSmart, FFX_CACAO_PASS_APPLY_NON_SMART, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-APPLY");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineApply, FFX_CACAO_PASS_APPLY, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-UPSCALE_BILATERAL_5X5_HALF");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineUpscaleBilateral5x5Half, FFX_CACAO_PASS_UPSCALE_BILATERAL_5X5, supportedFP16, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-UPSCALE_BILATERAL_5X5_NON_SMART");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineUpscaleBilateral5x5NonSmart, FFX_CACAO_PASS_UPSCALE_BILATERAL_5X5, false, canForceWave64);

    wcscpy_s(pipelineDescription.name, L"CACAO-UPSCALE_BILATERAL_5X5_SMART");
    createCacaoPipeline(context, pipelineDescription, &context->pipelineUpscaleBilateral5x5Smart, FFX_CACAO_PASS_UPSCALE_BILATERAL_5X5, false, canForceWave64, true);

    return FFX_OK;
}

static FfxErrorCode cacaoCreate(FfxCacaoContext_Private* context, const FfxCacaoContextDescription* contextDescription)
{
    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    memset(context, 0, sizeof(FfxCacaoContext_Private));
    context->device = contextDescription->backendInterface.device;
    memcpy(&context->contextDescription, contextDescription, sizeof(FfxCacaoContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);
    
    context->constantBuffer.num32BitEntries = sizeof(FfxCacaoConstants) / sizeof(uint32_t);

    // Create the device
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_CACAO, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

#ifdef FFX_CACAO_ENABLE_PROFILING
    errorStatus = gpuTimerInit(&context->gpuTimer, device);
    if (errorStatus)
    {
        goto error_create_gpu_timer;
    }
#endif

    context->useDownsampledSsao   = contextDescription->useDownsampledSsao;

    FfxCacaoBufferSizeInfo* bsi = &context->bufferSizeInfo;
    ffxCacaoUpdateBufferSizeInfo(contextDescription->width, contextDescription->height, context->useDownsampledSsao, bsi);

    // =======================================
    // Init textures
    const FfxInternalResourceDescription internalSurfaceDesc[] = {

        {FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_DEPTHS,
         context->useDownsampledSsao ? L"CACAO_Deinterleaved_Depths_Downsampled" : L"CACAO_DeInterleaved_Depths",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R16_FLOAT,
         bsi->deinterleavedDepthBufferWidth,
         bsi->deinterleavedDepthBufferHeight,
         4,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_NORMALS,
         context->useDownsampledSsao ? L"CACAO_DeInterleaved_Normals_Downsampled" : L"CACAO_DeInterleaved_Normals",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8G8B8A8_SNORM,
         bsi->ssaoBufferWidth,
         bsi->ssaoBufferHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PING,
         context->useDownsampledSsao ? L"CACAO_Ssao_Buffer_Ping_Downsampled" : L"CACAO_Ssao_Buffer_Ping",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8G8_UNORM,
         bsi->ssaoBufferWidth,
         bsi->ssaoBufferHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PONG,
         context->useDownsampledSsao ? L"CACAO_Ssao_Buffer_Pong_Downsampled" : L"CACAO_Ssao_Buffer_Pong",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8G8_UNORM,
         bsi->ssaoBufferWidth,
         bsi->ssaoBufferHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP,
         context->useDownsampledSsao ? L"CACAO_Importance_Map_Downsampled" : L"CACAO_Importance_Map",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8_UNORM,
         bsi->importanceMapWidth,
         bsi->importanceMapHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},

        {FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP_PONG,
         context->useDownsampledSsao ? L"CACAO_Importance_Map_Pong_Downsampled" : L"CACAO_Importance_Map_Pong",
         FFX_RESOURCE_TYPE_TEXTURE2D,
         FFX_RESOURCE_USAGE_UAV,
         FFX_SURFACE_FORMAT_R8_UNORM,
         bsi->importanceMapWidth,
         bsi->importanceMapHeight,
         1,
         FFX_RESOURCE_FLAGS_ALIASABLE,
         {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}},
    };

    const uint32_t surfaceDepths[] = {
        4, //CACAO_DeInterleaved_Depths
        4, //CACAO_DeInterleaved_Normals
        4, //CACAO_Ssao_Buffer_Ping
        4, //CACAO_Ssao_Buffer_Pong
        1, //CACAO_Importance_Map
        1, //CACAO_Importance_Map_Pong
    };

    // clear the textures to NULL.
    memset(context->textures, 0, sizeof(context->textures));

    // Create load counter
    {
        const FfxInternalResourceDescription internalSurfaceDesc = {FFX_CACAO_RESOURCE_IDENTIFIER_LOAD_COUNTER_BUFFER,
                                                                    context->useDownsampledSsao ? L"CACAO::m_loadCounterDownsampled" : L"CACAO::m_loadCounter",
                                                                    FFX_RESOURCE_TYPE_TEXTURE1D,
                                                                    FFX_RESOURCE_USAGE_UAV,
                                                                    FFX_SURFACE_FORMAT_UNKNOWN,
                                                                    1 * sizeof(uint32_t),
                                                                    sizeof(uint32_t),
                                                                    1,
                                                                    FFX_RESOURCE_FLAGS_NONE,
                                                                    {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED}};

        const FfxResourceDescription resourceDescription = {FFX_RESOURCE_TYPE_TEXTURE1D, FFX_SURFACE_FORMAT_R32_UINT, 1, 1, 1, 1, FFX_RESOURCE_FLAGS_NONE, internalSurfaceDesc.usage};
        const FfxCreateResourceDescription createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                        resourceDescription,
                                                                        FFX_RESOURCE_STATE_UNORDERED_ACCESS,
                                                                        internalSurfaceDesc.name,
                                                                        internalSurfaceDesc.id,
                                                                        internalSurfaceDesc.initData};
        FFX_VALIDATE(contextDescription->backendInterface.fpCreateResource(&context->contextDescription.backendInterface,
                                                                           &createResourceDescription,
                                                                           context->effectContextId,
                                                                           &context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_LOAD_COUNTER_BUFFER]));
    }

    for (int32_t currentSurfaceIndex = 0; currentSurfaceIndex < FFX_ARRAY_ELEMENTS(internalSurfaceDesc); ++currentSurfaceIndex)
    {
        const FfxInternalResourceDescription* currentSurfaceDescription = &internalSurfaceDesc[currentSurfaceIndex];
        const FfxResourceType                 resourceType = currentSurfaceDescription->height > 1 ? FFX_RESOURCE_TYPE_TEXTURE2D : FFX_RESOURCE_TYPE_TEXTURE1D;
        const FfxResourceDescription          resourceDescription       = {resourceType,
                                                                           currentSurfaceDescription->format,
                                                                           currentSurfaceDescription->width,
                                                                           currentSurfaceDescription->height,
                                                                           surfaceDepths[currentSurfaceIndex],
                                                                           currentSurfaceDescription->mipCount,
                                                                           FFX_RESOURCE_FLAGS_NONE,
                                                                           currentSurfaceDescription->usage};
        const FfxResourceStates               initialState              = FFX_RESOURCE_STATE_UNORDERED_ACCESS;
        const FfxCreateResourceDescription    createResourceDescription = {FFX_HEAP_TYPE_DEFAULT,
                                                                           resourceDescription,
                                                                           initialState,
                                                                           currentSurfaceDescription->name,
                                                                           currentSurfaceDescription->id,
                                                                           currentSurfaceDescription->initData};

        FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface,
                                                                                   &createResourceDescription,
                                                                                   context->effectContextId,
                                                                                   &context->textures[currentSurfaceDescription->id]));
    }

    errorCode = createPipelineStates(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

static FfxErrorCode cacaoRelease(FfxCacaoContext_Private* context)
{
    FFX_ASSERT(context);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineClearLoadCounter, context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareDownsampledDepths, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareNativeDepths, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareDownsampledDepthsAndMips, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareNativeDepthsAndMips, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareDownsampledDepthsHalf, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareNativeDepthsHalf, context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareDownsampledNormals, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareNativeNormals, context->effectContextId);
    ffxSafeReleasePipeline(
        &context->contextDescription.backendInterface, &context->pipelinePrepareDownsampledNormalsFromInputNormals, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelinePrepareNativeNormalsFromInputNormals, context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineGenerateQ[0], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineGenerateQ[1], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineGenerateQ[2], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineGenerateQ[3], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineGenerateQ[4], context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineGenerateImportanceMap, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineProcessImportanceMapA, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineProcessImportanceMapB, context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[0], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[1], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[2], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[3], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[4], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[5], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[6], context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineEdgeSensitiveBlur[7], context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineApplyNonSmartHalf, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineApplyNonSmart, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineApply, context->effectContextId);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineUpscaleBilateral5x5Half, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineUpscaleBilateral5x5Smart, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineUpscaleBilateral5x5NonSmart, context->effectContextId);

    // Unregister resource
    context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_DEPTH_IN]  = {FFX_FSR2_RESOURCE_IDENTIFIER_NULL};
    context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_NORMAL_IN] = {FFX_FSR2_RESOURCE_IDENTIFIER_NULL};
    context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_OUTPUT]    = {FFX_FSR2_RESOURCE_IDENTIFIER_NULL};

    for (uint32_t i = 0; i < FFX_CACAO_RESOURCE_IDENTIFIER_COUNT; ++i)
    {
        ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->textures[i], context->effectContextId);
    }

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

static void scheduleDispatch(
    FfxCacaoContext_Private* context, const FfxPipelineState* pipeline, const uint32_t dispatchX, const uint32_t dispatchY, const uint32_t dispatchZ, const uint32_t flags = 0)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);
    const size_t size                      = sizeof(dispatchJob);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        const uint32_t      currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        FfxResourceInternal currentResource   = context->textures[currentResourceId];

        if ((flags & FFX_CACAO_SRV_SSAO_REMAP_TO_PONG) != 0 && currentResourceId == FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PING)
        {
            currentResource = context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PONG];
        }

        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name,
                 pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    uint32_t uavEntry = 0;  // Uav resource offset (accounts for uav arrays)
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        uint32_t       bindEntry         = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].arrayIndex;
        const uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name,
                 pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        if (currentResourceId == FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_DEPTH_MIPMAP_0)
        {
            const FfxResourceInternal currentResource = context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_DEPTHS];

            // Don't over-subscribe mips (default to mip 0 once we've exhausted min mip)
            const FfxResourceDescription resDesc = context->contextDescription.backendInterface.fpGetResourceDescription(&context->contextDescription.backendInterface, currentResource);
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip    = (bindEntry < resDesc.mipCount) ? bindEntry : 0;
        }
        else
        {
            FfxResourceInternal currentResource = context->textures[currentResourceId];
            if ((flags & FFX_CACAO_UAV_SSAO_REMAP_TO_PONG) != 0 && currentResourceId == FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PING)
            {
                currentResource = context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PONG];
            }

            dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
            dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = 0;
        }
    }

    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = dispatchZ;
    dispatchJob.computeJobDescriptor.pipeline      = *pipeline;

#ifdef FFX_DEBUG
    wcscpy_s(dispatchJob.computeJobDescriptor.cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    dispatchJob.computeJobDescriptor.cbs[0] = context->constantBuffer;

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

// TODO: REMOVE
#define USER_MARKER(arg)

static FfxErrorCode cacaoDispatch(FfxCacaoContext_Private* context,
                                  FfxCommandList           commandList,
                                  FfxResource              depthBuffer,
                                  FfxResource              normalBuffer,
                                  FfxResource              outputBuffer,
                                  const FfxFloat32x4x4*    proj,
                                  const FfxFloat32x4x4*    normalsToView,
                                  const float              normalUnPackMul,
                                  const float              normalUnPackAdd)
{
    FFX_ASSERT(context);

#ifdef FFX_CACAO_ENABLE_PROFILING
#define GET_TIMESTAMP(name) gpuTimerGetTimestamp(&context->gpuTimer, commandList, TIMESTAMP_##name)
#else
#define GET_TIMESTAMP(name)
#endif
    const FfxCacaoBufferSizeInfo* bsi = &context->bufferSizeInfo;

    USER_MARKER("FidelityFX CACAO");

#ifdef FFX_CACAO_ENABLE_PROFILING
    gpuTimerStartFrame(&context->gpuTimer);
#endif

    GET_TIMESTAMP(BEGIN);

    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &depthBuffer, context->effectContextId, &context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_DEPTH_IN]);
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &normalBuffer, context->effectContextId, &context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_NORMAL_IN]);
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &outputBuffer, context->effectContextId, &context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_OUTPUT]);

    // clear load counter
    {
        FfxGpuJobDescription clearJob      = {FFX_GPU_JOB_CLEAR_FLOAT};
        wcscpy_s(clearJob.jobLabel, L"Clear Load Counter");
        uint32_t             clearValues[] = {0, 0, 0, 0};
        memcpy(clearJob.clearJobDescriptor.color, clearValues, sizeof(uint32_t) * FFX_CACAO_ARRAY_SIZE(clearJob.clearJobDescriptor.color));
        clearJob.clearJobDescriptor.target = context->textures[FFX_CACAO_RESOURCE_IDENTIFIER_LOAD_COUNTER_BUFFER];
        context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &clearJob);
    }
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);
    // upload constant buffers
    ffxCacaoUpdateConstants(&context->constants, &context->settings, bsi, proj, normalsToView, normalUnPackMul, normalUnPackAdd);
    
    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface, &context->constants, sizeof(FfxCacaoConstants), &context->constantBuffer);
    
    // prepare depths, normals and mips
    {
        USER_MARKER("Prepare downsampled depths, normals and mips");

        switch (context->settings.qualityLevel)
        {
        case FFX_CACAO_QUALITY_LOWEST:
        {
            const uint32_t         dispatchWidth  = dispatchSize(FFX_CACAO_PREPARE_DEPTHS_HALF_WIDTH, bsi->deinterleavedDepthBufferWidth);
            const uint32_t         dispatchHeight = dispatchSize(FFX_CACAO_PREPARE_DEPTHS_HALF_HEIGHT, bsi->deinterleavedDepthBufferHeight);
            const FfxPipelineState prepareDepthsHalf =
                context->useDownsampledSsao ? context->pipelinePrepareDownsampledDepthsHalf : context->pipelinePrepareNativeDepthsHalf;
            scheduleDispatch(
                context, &prepareDepthsHalf, dispatchWidth, dispatchHeight, 1);
            break;
        }
        case FFX_CACAO_QUALITY_LOW:
        {
            const uint32_t         dispatchWidth  = dispatchSize(FFX_CACAO_PREPARE_DEPTHS_WIDTH, bsi->deinterleavedDepthBufferWidth);
            const uint32_t         dispatchHeight = dispatchSize(FFX_CACAO_PREPARE_DEPTHS_HEIGHT, bsi->deinterleavedDepthBufferHeight);
            const FfxPipelineState prepareDepths  = context->useDownsampledSsao ? context->pipelinePrepareDownsampledDepths : context->pipelinePrepareNativeDepths;
            scheduleDispatch(context,
                             &prepareDepths,
                             dispatchWidth,
                             dispatchHeight,
                             1);
            break;
        }
        default:
        {
            const uint32_t         dispatchWidth  = dispatchSize(FFX_CACAO_PREPARE_DEPTHS_AND_MIPS_WIDTH, bsi->deinterleavedDepthBufferWidth);
            const uint32_t         dispatchHeight = dispatchSize(FFX_CACAO_PREPARE_DEPTHS_AND_MIPS_HEIGHT, bsi->deinterleavedDepthBufferHeight);
            const FfxPipelineState prepareDepthsAndMips =
                context->useDownsampledSsao ? context->pipelinePrepareDownsampledDepthsAndMips : context->pipelinePrepareNativeDepthsAndMips;
            scheduleDispatch(context,
                             &prepareDepthsAndMips,
                             dispatchWidth,
                             dispatchHeight,
                             1);
            break;
        }
        }

        if (context->settings.generateNormals)
        {
            const uint32_t         dispatchWidth  = dispatchSize(FFX_CACAO_PREPARE_NORMALS_WIDTH, bsi->ssaoBufferWidth);
            const uint32_t         dispatchHeight = dispatchSize(FFX_CACAO_PREPARE_NORMALS_HEIGHT, bsi->ssaoBufferHeight);
            const FfxPipelineState prepareNormals = context->useDownsampledSsao ? context->pipelinePrepareDownsampledNormals : context->pipelinePrepareNativeNormals;
            scheduleDispatch(context,
                             &prepareNormals,
                             dispatchWidth,
                             dispatchHeight,
                             1);
        }
        else
        {
            const uint32_t         dispatchWidth                  = dispatchSize(PREPARE_NORMALS_FROM_INPUT_NORMALS_WIDTH, bsi->ssaoBufferWidth);
            const uint32_t         dispatchHeight                 = dispatchSize(PREPARE_NORMALS_FROM_INPUT_NORMALS_HEIGHT, bsi->ssaoBufferHeight);
            const FfxPipelineState prepareNormalsFromInputNormals = context->useDownsampledSsao ? context->pipelinePrepareDownsampledNormalsFromInputNormals
                                                                                          : context->pipelinePrepareNativeNormalsFromInputNormals;
            scheduleDispatch(context,
                             &prepareNormalsFromInputNormals,
                             dispatchWidth,
                             dispatchHeight,
                             1);
        }

        GET_TIMESTAMP(PREPARE);
    }
    // base pass for highest quality setting
    if (context->settings.qualityLevel == FFX_CACAO_QUALITY_HIGHEST)
    {
        USER_MARKER("Generate High Quality Base Pass");

        // SSAO
        {
            USER_MARKER("SSAO");

            const uint32_t dispatchWidth  = dispatchSize(FFX_CACAO_GENERATE_WIDTH, bsi->ssaoBufferWidth);
            const uint32_t dispatchHeight = dispatchSize(FFX_CACAO_GENERATE_HEIGHT, bsi->ssaoBufferHeight);
            scheduleDispatch(context,
                             &context->pipelineGenerateQ[4],
                             dispatchWidth,
                             dispatchHeight,
                             4,
                             FFX_CACAO_UAV_SSAO_REMAP_TO_PONG);
            GET_TIMESTAMP(BASE_SSAO_PASS);
        }

        // generate importance map
        {
            USER_MARKER("Importance Map");

            const uint32_t dispatchWidth  = dispatchSize(IMPORTANCE_MAP_WIDTH, bsi->importanceMapWidth);
            const uint32_t dispatchHeight = dispatchSize(IMPORTANCE_MAP_HEIGHT, bsi->importanceMapHeight);

            scheduleDispatch(context,
                             &context->pipelineGenerateImportanceMap,
                             dispatchWidth,
                             dispatchHeight,
                             1);
            scheduleDispatch(context,
                             &context->pipelineProcessImportanceMapA,
                             dispatchWidth,
                             dispatchHeight,
                             1);
            scheduleDispatch(context,
                             &context->pipelineProcessImportanceMapB,
                             dispatchWidth,
                             dispatchHeight,
                             1);

            GET_TIMESTAMP(IMPORTANCE_MAP);
        }
    }
    const int blurPassCount = FFX_CACAO_CLAMP(context->settings.blurPassCount, 0, MAX_BLUR_PASSES);

    // main ssao generation
    {
        USER_MARKER("Generate SSAO");

        const uint32_t generate = FFX_CACAO_MAX(0, context->settings.qualityLevel - 1);
        uint32_t dispatchWidth, dispatchHeight, dispatchDepth;

        switch (context->settings.qualityLevel)
        {
        case FFX_CACAO_QUALITY_LOWEST:
        case FFX_CACAO_QUALITY_LOW:
        case FFX_CACAO_QUALITY_MEDIUM:
            dispatchWidth  = dispatchSize(FFX_CACAO_GENERATE_SPARSE_WIDTH, bsi->ssaoBufferWidth);
            dispatchWidth  = (dispatchWidth + 4) / 5;
            dispatchHeight = dispatchSize(FFX_CACAO_GENERATE_SPARSE_HEIGHT, bsi->ssaoBufferHeight);
            dispatchDepth  = 5;
            break;
        case FFX_CACAO_QUALITY_HIGH:
        case FFX_CACAO_QUALITY_HIGHEST:
            dispatchWidth  = dispatchSize(FFX_CACAO_GENERATE_WIDTH, bsi->ssaoBufferWidth);
            dispatchHeight = dispatchSize(FFX_CACAO_GENERATE_HEIGHT, bsi->ssaoBufferHeight);
            dispatchDepth  = 1;
            break;
        default:
            return FFX_ERROR_INVALID_ENUM;
        }

        dispatchDepth *= (context->settings.qualityLevel == FFX_CACAO_QUALITY_LOWEST) ? 2 : 4;  // 2 layers for lowest, 4 for all others

        scheduleDispatch(context,
                         &context->pipelineGenerateQ[generate],
                         dispatchWidth,
                         dispatchHeight,
                         dispatchDepth);

        GET_TIMESTAMP(GENERATE_SSAO);
    }
    // de-interleaved blur
    if (blurPassCount)
    {
        USER_MARKER("Deinterleaved blur");

        const uint32_t w                 = 4 * FFX_CACAO_BLUR_WIDTH - 2 * blurPassCount;
        const uint32_t h                 = 3 * FFX_CACAO_BLUR_HEIGHT - 2 * blurPassCount;
        const uint32_t blurPassIndex     = blurPassCount - 1;
        const uint32_t dispatchWidth     = dispatchSize(w, bsi->ssaoBufferWidth);
        const uint32_t dispatchHeight    = dispatchSize(h, bsi->ssaoBufferHeight);
        const uint32_t dispatchDepth     = context->settings.qualityLevel == FFX_CACAO_QUALITY_LOWEST ? 2 : 4;
        const uint32_t edgeSensitiveBlur = blurPassCount - 1;
        scheduleDispatch(context,
                         &context->pipelineEdgeSensitiveBlur[edgeSensitiveBlur],
                         dispatchWidth,
                         dispatchHeight,
                         dispatchDepth);

        GET_TIMESTAMP(EDGE_SENSITIVE_BLUR);
    }
    const uint32_t dispatchFlags = blurPassCount ? FFX_CACAO_SRV_SSAO_REMAP_TO_PONG : 0;
    if (context->useDownsampledSsao)
    {
        USER_MARKER("Upscale");

        FfxPipelineState upscaler;
        switch (context->settings.qualityLevel)
        {
        case FFX_CACAO_QUALITY_LOWEST:
            upscaler = context->pipelineUpscaleBilateral5x5Half;
            break;
        case FFX_CACAO_QUALITY_LOW:
        case FFX_CACAO_QUALITY_MEDIUM:
            upscaler = context->pipelineUpscaleBilateral5x5NonSmart;
            break;
        case FFX_CACAO_QUALITY_HIGH:
        case FFX_CACAO_QUALITY_HIGHEST:
            upscaler = context->pipelineUpscaleBilateral5x5Smart;
            break;
        }
        const uint32_t dispatchWidth  = dispatchSize(2 * FFX_CACAO_BILATERAL_UPSCALE_WIDTH, bsi->inputOutputBufferWidth);
        const uint32_t dispatchHeight = dispatchSize(2 * FFX_CACAO_BILATERAL_UPSCALE_HEIGHT, bsi->inputOutputBufferHeight);
        scheduleDispatch(context, &upscaler, dispatchWidth, dispatchHeight, 1, dispatchFlags);

        GET_TIMESTAMP(BILATERAL_UPSAMPLE);
    }
    else
    {
        USER_MARKER("Create Output");
        const uint32_t dispatchWidth  = dispatchSize(FFX_CACAO_APPLY_WIDTH, bsi->inputOutputBufferWidth);
        const uint32_t dispatchHeight = dispatchSize(FFX_CACAO_APPLY_HEIGHT, bsi->inputOutputBufferHeight);
        switch (context->settings.qualityLevel)
        {
        case FFX_CACAO_QUALITY_LOWEST:
            scheduleDispatch(context, &context->pipelineApplyNonSmartHalf, dispatchWidth, dispatchHeight, 1, dispatchFlags);
            break;
        case FFX_CACAO_QUALITY_LOW:
            scheduleDispatch(context, &context->pipelineApplyNonSmart, dispatchWidth, dispatchHeight, 1, dispatchFlags);
            break;
        default:
            scheduleDispatch(context, &context->pipelineApply, dispatchWidth, dispatchHeight, 1, dispatchFlags);
            break;
        }
        GET_TIMESTAMP(APPLY);
    }

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

#ifdef FFX_CACAO_ENABLE_PROFILING
    gpuTimerEndFrame(&context->gpuTimer, commandList);
#endif

    return FFX_OK;

#undef GET_TIMESTAMP
}

#ifdef FFX_CACAO_ENABLE_PROFILING
FFX_CACAO_Status FFX_CACAO_D3D12GetDetailedTimings(FFX_CACAO_D3D12Context* context, FFX_CACAO_DetailedTiming* timings)
{
    if (context == NULL || timings == NULL)
    {
        return FFX_CACAO_STATUS_INVALID_POINTER;
    }
    context = getAlignedD3D12ContextPointer(context);

    gpuTimerCollectTimings(&context->gpuTimer, timings);

    return FFX_CACAO_STATUS_OK;
}
#endif

FfxErrorCode ffxCacaoContextCreate(FfxCacaoContext* context, const FfxCacaoContextDescription* contextDescription)
{
    // zero context memory
    memset(context, 0, sizeof(FfxCacaoContext));

    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

    // validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // if a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxCacaoContext) >= sizeof(FfxCacaoContext_Private));

    // create the context.
    FfxCacaoContext_Private* contextPrivate = (FfxCacaoContext_Private*)(context);
    const FfxErrorCode       errorCode      = cacaoCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxCacaoContextDestroy(FfxCacaoContext* context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // destroy the context.
    FfxCacaoContext_Private* contextPrivate = (FfxCacaoContext_Private*)(context);
    const FfxErrorCode       errorCode      = cacaoRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxCacaoContextDispatch(FfxCacaoContext* context, const FfxCacaoDispatchDescription* dispatchParams)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchParams, FFX_ERROR_INVALID_POINTER);

    FfxCacaoContext_Private* contextPrivate = (FfxCacaoContext_Private*)(context);

    // dispatch the FSR2 passes.
    const FfxErrorCode errorCode = cacaoDispatch(contextPrivate,
                                                 dispatchParams->commandList,
                                                 dispatchParams->depthBuffer,
                                                 dispatchParams->normalBuffer,
                                                 dispatchParams->outputBuffer,
                                                 dispatchParams->proj,
                                                 dispatchParams->normalsToView,
                                                 dispatchParams->normalUnpackMul,
                                                 dispatchParams->normalUnpackAdd);
    return errorCode;
}

FfxErrorCode ffxCacaoUpdateSettings(FfxCacaoContext* context, const FfxCacaoSettings* settings, const bool useDownsampledSsao)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(settings, FFX_ERROR_INVALID_POINTER);
    
    FfxCacaoContext_Private* contextPrivate = (FfxCacaoContext_Private*)(context);
    contextPrivate->useDownsampledSsao      = useDownsampledSsao;
    memcpy(&contextPrivate->settings, settings, sizeof(*settings));

    return FFX_OK;
}

FFX_API FfxVersionNumber ffxCacaoGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_CACAO_VERSION_MAJOR, FFX_CACAO_VERSION_MINOR, FFX_CACAO_VERSION_PATCH);
}
