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

#include <string.h>  // for memset
#include <stdlib.h>  // for _countof
#include <cmath>     // for fabs, abs, sinf, sqrt, etc.


#include <FidelityFX/host/ffx_lpm.h>

#define FFX_CPU
#include <FidelityFX/gpu/ffx_core.h>

static float    fs2S;
static float    hdr10S;
static uint32_t ctl[24 * 4];

static void LpmSetupOut(uint32_t i, uint32_t* v)
{
    for (int j = 0; j < 4; ++j)
    {
        ctl[i * 4 + j] = v[j];
    }
}
#include <FidelityFX/gpu/lpm/ffx_lpm.h>
#include <ffx_object_management.h>

#include "ffx_lpm_private.h"

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t index;
    wchar_t  name[64];
} ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] = {
    {FFX_LPM_RESOURCE_IDENTIFIER_INPUT_COLOR, L"r_input_color"},
};

static const ResourceBinding uavTextureBindingTable[] = {
    {FFX_LPM_RESOURCE_IDENTIFIER_OUTPUT_COLOR, L"rw_output_color"},
};

static const ResourceBinding cbResourceBindingTable[] = {
    {FFX_LPM_CONSTANTBUFFER_IDENTIFIER_LPM, L"cbLPM"},
};

static FfxErrorCode patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    for (uint32_t srvIndex = 0; srvIndex < inoutPipeline->srvTextureCount; ++srvIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(srvTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvTextureBindingTable[mapIndex].name, inoutPipeline->srvTextureBindings[srvIndex].name))
                break;
        }
        if (mapIndex == _countof(srvTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->srvTextureBindings[srvIndex].resourceIdentifier = srvTextureBindingTable[mapIndex].index;
    }

    for (uint32_t uavIndex = 0; uavIndex < inoutPipeline->uavTextureCount; ++uavIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(uavTextureBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavTextureBindingTable[mapIndex].name, inoutPipeline->uavTextureBindings[uavIndex].name))
                break;
        }
        if (mapIndex == _countof(uavTextureBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->uavTextureBindings[uavIndex].resourceIdentifier = uavTextureBindingTable[mapIndex].index;
    }

    for (uint32_t cbIndex = 0; cbIndex < inoutPipeline->constCount; ++cbIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(cbResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(cbResourceBindingTable[mapIndex].name, inoutPipeline->constantBufferBindings[cbIndex].name))
                break;
        }
        if (mapIndex == _countof(cbResourceBindingTable))
            return FFX_ERROR_INVALID_ARGUMENT;

        inoutPipeline->constantBufferBindings[cbIndex].resourceIdentifier = cbResourceBindingTable[mapIndex].index;
    }

    return FFX_OK;
}

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, FfxLpmPass passId, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? LPM_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? LPM_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    return flags;
}

static FfxErrorCode createPipelineStates(FfxLpmContext_Private* context)
{
    FFX_ASSERT(context);
    FfxPipelineDescription pipelineDescription = {};
    pipelineDescription.contextFlags            = context->contextDescription.flags;

    // Samplers
    pipelineDescription.samplerCount = 1;
    FfxSamplerDescription samplerDesc = { FFX_FILTER_TYPE_MINMAGMIP_LINEAR, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_ADDRESS_MODE_CLAMP, FFX_BIND_COMPUTE_SHADER_STAGE };
    pipelineDescription.samplers = &samplerDesc;

    // Root constants
    pipelineDescription.rootConstantBufferCount = 1;
    FfxRootConstantDescription rootConstantDesc = { sizeof(LpmConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };
    pipelineDescription.rootConstants = &rootConstantDesc;

    // Query device capabilities
    FfxDeviceCapabilities capabilities;
    context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &capabilities);

    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    // Defaulting to false to avoid fp16 overflows for large HDR values.
    // Set to true, if certain content will not have issues with fp16 overflow for enabling optimization allowing for 2pix per thread.
    bool supportedFP16     = false;
    bool canForceWave64    = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
        canForceWave64 = haveShaderModel66;
    else
        canForceWave64 = false;

    // Work out what permutation to load.
    uint32_t contextFlags = context->contextDescription.flags;

    // Set up pipeline descriptors (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"LPM-FILTER");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(
        &context->contextDescription.backendInterface,
        FFX_EFFECT_LPM,
        FFX_LPM_PASS_FILTER,
        getPipelinePermutationFlags(contextFlags, FFX_LPM_PASS_FILTER, supportedFP16, canForceWave64),
        &pipelineDescription, context->effectContextId,
        &context->pipelineLPMFilter));

    // For each pipeline: re-route/fix-up IDs based on names
    patchResourceBindings(&context->pipelineLPMFilter);

    return FFX_OK;
}

static void scheduleDispatch(FfxLpmContext_Private* context, const FfxLpmDispatchDescription* params, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        const uint32_t            currentResourceId               = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource                 = context->srvResources[currentResourceId];
        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name,
                 pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name,
                 pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
        const FfxResourceInternal currentResource                     = context->uavResources[currentResourceId];
        dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].resource = currentResource;
        dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].mip = 0;
    }

    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = 1;
    dispatchJob.computeJobDescriptor.pipeline      = *pipeline;

#ifdef FFX_DEBUG
    wcscpy_s(dispatchJob.computeJobDescriptor.cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    dispatchJob.computeJobDescriptor.cbs[0] = context->constantBuffer;


    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &dispatchJob);
}

static FfxErrorCode lpmDispatch(FfxLpmContext_Private* context, const FfxLpmDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface, &params->inputColor, context->effectContextId, &context->srvResources[FFX_LPM_RESOURCE_IDENTIFIER_INPUT_COLOR]);

    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &params->outputColor, context->effectContextId, &context->uavResources[FFX_LPM_RESOURCE_IDENTIFIER_OUTPUT_COLOR]);

    // This value is the image region dimension that each thread group of the LPM shader operates on
    static const int threadGroupWorkRegionDim = 16;
    FfxResourceDescription desc = context->contextDescription.backendInterface.fpGetResourceDescription(
        &context->contextDescription.backendInterface, context->srvResources[FFX_LPM_RESOURCE_IDENTIFIER_INPUT_COLOR]);
    int dispatchX = FFX_DIVIDE_ROUNDING_UP(desc.width, threadGroupWorkRegionDim);
    int dispatchY = FFX_DIVIDE_ROUNDING_UP(desc.height, threadGroupWorkRegionDim);

    FfxFloat32x2 fs2R;
    FfxFloat32x2 fs2G;
    FfxFloat32x2 fs2B;
    FfxFloat32x2 fs2W;
    FfxFloat32x2 displayMinMaxLuminance;
    if (params->displayMode != FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_LDR)
    {
        // Only used in fs2 modes
        fs2R[0] = params->displayRedPrimary[0];
        fs2R[1] = params->displayRedPrimary[1];
        fs2G[0] = params->displayGreenPrimary[0];
        fs2G[1] = params->displayGreenPrimary[1];
        fs2B[0] = params->displayBluePrimary[0];
        fs2B[1] = params->displayBluePrimary[1];
        fs2W[0] = params->displayWhitePoint[0];
        fs2W[1] = params->displayWhitePoint[1];

        // Used in all HDR modes
        displayMinMaxLuminance[0] = params->displayMinLuminance;
        displayMinMaxLuminance[1] = params->displayMaxLuminance;
    }

    FfxFloat32x3 saturation;
    saturation[0] = params->saturation[0];
    saturation[1] = params->saturation[1];
    saturation[2] = params->saturation[2];

    FfxFloat32x3 crosstalk;
    crosstalk[0] = params->crosstalk[0];
    crosstalk[1] = params->crosstalk[1];
    crosstalk[2] = params->crosstalk[2];

    LpmConstants lpmConsts = {};

    lpmConsts.displayMode = static_cast<FfxUInt32>(params->displayMode);

    switch (params->colorSpace)
    {
        case FfxLpmColorSpace::FFX_LPM_ColorSpace_REC709:
        {
            switch (params->displayMode)
            {
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_LDR:
                {
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_709_709,
                                          LPM_COLORS_709_709,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_709_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_FSHDR_2084:
                {
                    hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_FS2RAWPQ_709,
                                          LPM_COLORS_FS2RAWPQ_709,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_FS2RAWPQ_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_FSHDR_SCRGB:
                {
                    fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_FS2SCRGB_709,
                                          LPM_COLORS_FS2SCRGB_709,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_FS2SCRGB_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_HDR10_2084:
                {
                    hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_HDR10RAW_709,
                                          LPM_COLORS_HDR10RAW_709,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_HDR10RAW_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_HDR10_SCRGB:
                {
                    hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_HDR10SCRGB_709,
                                          LPM_COLORS_HDR10SCRGB_709,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_HDR10SCRGB_709, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
            }
        }
        break;
        case FfxLpmColorSpace::FFX_LPM_ColorSpace_P3:
        {
            switch (params->displayMode)
            {
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_LDR:
                {
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_709_P3,
                                          LPM_COLORS_709_P3,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_709_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_FSHDR_2084:
                {
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_FS2RAWPQ_P3,
                                          LPM_COLORS_FS2RAWPQ_P3,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_FS2RAWPQ_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_FSHDR_SCRGB:
                {
                    fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_FS2SCRGB_P3,
                                          LPM_COLORS_FS2SCRGB_P3,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_FS2SCRGB_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_HDR10_2084:
                {
                    hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_HDR10RAW_P3,
                                          LPM_COLORS_HDR10RAW_P3,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_HDR10RAW_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_HDR10_SCRGB:
                {
                    hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_HDR10SCRGB_P3,
                                          LPM_COLORS_HDR10SCRGB_P3,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_HDR10SCRGB_P3, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
            }
        }
        break;
        case FfxLpmColorSpace::FFX_LPM_ColorSpace_REC2020:
        {
            switch (params->displayMode)
            {
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_LDR:
                {
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_709_2020,
                                          LPM_COLORS_709_2020,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_709_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_FSHDR_2084:
                {
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_FS2RAWPQ_2020,
                                          LPM_COLORS_FS2RAWPQ_2020,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_FS2RAWPQ_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_FSHDR_SCRGB:
                {
                    fs2S = LpmFs2ScrgbScalar(displayMinMaxLuminance[0], displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_FS2SCRGB_2020,
                                          LPM_COLORS_FS2SCRGB_2020,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_FS2SCRGB_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_HDR10_2084:
                {
                    hdr10S = LpmHdr10RawScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_HDR10RAW_2020,
                                          LPM_COLORS_HDR10RAW_2020,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_HDR10RAW_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
                case FfxLpmDisplayMode::FFX_LPM_DISPLAYMODE_HDR10_SCRGB:
                {
                    hdr10S = LpmHdr10ScrgbScalar(displayMinMaxLuminance[1]);
                    FfxCalculateLpmConsts(params->shoulder,
                                          LPM_CONFIG_HDR10SCRGB_2020,
                                          LPM_COLORS_HDR10SCRGB_2020,
                                          params->softGap,
                                          params->hdrMax,
                                          params->lpmExposure,
                                          params->contrast,
                                          params->shoulderContrast,
                                          saturation,
                                          crosstalk);
                    FfxPopulateLpmConsts(LPM_CONFIG_HDR10SCRGB_2020, lpmConsts.con, lpmConsts.soft, lpmConsts.con2, lpmConsts.clip, lpmConsts.scaleOnly);
                }
                break;
            }
        }
        break;
        default:
            break;
    }

    memcpy(lpmConsts.ctl, ctl, sizeof(ctl));

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface, 
                                                                               &lpmConsts, 
                                                                               sizeof(LpmConstants), 
                                                                               &context->constantBuffer);
    
    scheduleDispatch(context, params, &context->pipelineLPMFilter, dispatchX, dispatchY);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    return FFX_OK;
}

static FfxErrorCode lpmCreate(FfxLpmContext_Private* context, const FfxLpmContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxLpmContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxLpmContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);
    
    // Setup constant buffer sizes.
    context->constantBuffer.num32BitEntries = sizeof(LpmConstants) / sizeof(uint32_t);

    // Create the context.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_LPM, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(
        &context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    // And copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // Create shaders on initialize.
    errorCode = createPipelineStates(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

static FfxErrorCode lpmRelease(FfxLpmContext_Private* context)
{
    FFX_ASSERT(context);

    // Release all pipelines
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineLPMFilter, context->effectContextId);

    // Unregister resources not created internally
    context->srvResources[FFX_LPM_RESOURCE_IDENTIFIER_INPUT_COLOR]  = {FFX_LPM_RESOURCE_IDENTIFIER_NULL};
    context->uavResources[FFX_LPM_RESOURCE_IDENTIFIER_OUTPUT_COLOR] = {FFX_LPM_RESOURCE_IDENTIFIER_NULL};

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxLpmContextCreate(FfxLpmContext* context, const FfxLpmContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(FfxLpmContext));
    
    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // Ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxLpmContext) >= sizeof(FfxLpmContext_Private));

    // create the context.
    FfxLpmContext_Private* contextPrivate = (FfxLpmContext_Private*)(context);
    const FfxErrorCode     errorCode      = lpmCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxLpmContextDestroy(FfxLpmContext* context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxLpmContext_Private* contextPrivate = (FfxLpmContext_Private*)(context);
    const FfxErrorCode     errorCode      = lpmRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxLpmContextDispatch(FfxLpmContext* context, const FfxLpmDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);

    FfxLpmContext_Private* contextPrivate = (FfxLpmContext_Private*)(context);

    // dispatch the LPM passes.
    const FfxErrorCode errorCode = lpmDispatch(contextPrivate, dispatchDescription);
    return errorCode;
}

FFX_API FfxErrorCode FfxPopulateLpmConsts(bool      incon,
                                          bool      insoft,
                                          bool      incon2,
                                          bool      inclip,
                                          bool      inscaleOnly,
                                          uint32_t& outcon,
                                          uint32_t& outsoft,
                                          uint32_t& outcon2,
                                          uint32_t& outclip,
                                          uint32_t& outscaleOnly)
{
    outcon = incon;
    outsoft = insoft;
    outcon2 = incon2;
    outclip = inclip;
    outscaleOnly = inscaleOnly;

    return FFX_OK;
}

FFX_API FfxVersionNumber ffxLpmGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_LPM_VERSION_MAJOR, FFX_LPM_VERSION_MINOR, FFX_LPM_VERSION_PATCH);
}
