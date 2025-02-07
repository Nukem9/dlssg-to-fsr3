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

#include <string>       // for wstring
#include <string.h>     // for memset
#include <stdlib.h>     // for _countof
#include <cmath>        // for fabs, abs, sinf, sqrt, etc.

#include <FidelityFX/host/ffx_blur.h>
#include <FidelityFX/gpu/ffx_core.h>
#include <FidelityFX/gpu/blur/ffx_blur.h>

#include <ffx_object_management.h>

#include "ffx_blur_private.h"

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t index;
    wchar_t  name[64];
} ResourceBinding;

static const ResourceBinding srvTextureBindingTable[] = {
    {FFX_BLUR_RESOURCE_IDENTIFIER_INPUT_SRC, L"r_input_src"},
};

static const ResourceBinding uavTextureBindingTable[] = {
    {FFX_BLUR_RESOURCE_IDENTIFIER_OUTPUT, L"rw_output"},
};

static const ResourceBinding cbResourceBindingTable[] = {
    {FFX_BLUR_CONSTANTBUFFER_IDENTIFIER_BLUR, L"cbBLUR"},
};

static wchar_t* getKernelSizeString(wchar_t* buffer, FfxBlurKernelSize kernelSize)
{
#pragma warning(push)
#pragma warning(disable : 4996) // Suppress the deprecation warning
    switch (kernelSize)
    {
    case FFX_BLUR_KERNEL_SIZE_3x3:
        wcsncpy(buffer, L"3x3", wcslen(L"3x3"));
        break;
    case FFX_BLUR_KERNEL_SIZE_5x5:
        wcsncpy(buffer, L"5x5", wcslen(L"5x5"));
        break;
    case FFX_BLUR_KERNEL_SIZE_7x7:
        wcsncpy(buffer, L"7x7", wcslen(L"7x7"));
        break;
    case FFX_BLUR_KERNEL_SIZE_9x9:
        wcsncpy(buffer, L"9x9", wcslen(L"9x9"));
        break;
    case FFX_BLUR_KERNEL_SIZE_11x11:
        wcsncpy(buffer, L"11x11", wcslen(L"11x11"));
        break;
    case FFX_BLUR_KERNEL_SIZE_13x13:
        wcsncpy(buffer, L"13x13", wcslen(L"13x13"));
        break;
    case FFX_BLUR_KERNEL_SIZE_15x15:
        wcsncpy(buffer, L"15x15", wcslen(L"15x15"));
        break;
    case FFX_BLUR_KERNEL_SIZE_17x17:
        wcsncpy(buffer, L"17x17", wcslen(L"17x17"));
        break;
    case FFX_BLUR_KERNEL_SIZE_19x19:
        wcsncpy(buffer, L"19x19", wcslen(L"19x19"));
        break;
    case FFX_BLUR_KERNEL_SIZE_21x21:
        wcsncpy(buffer, L"21x21", wcslen(L"21x21"));
        break;
    default:
        FFX_ASSERT_MESSAGE(false, "Unhandled kernel size in getKernelSizeString.");
        wcsncpy(buffer, L"?x?", wcslen(L"?x?"));
        break;
    }
#pragma warning(pop)

    return buffer;
}

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

static uint32_t getPipelinePermutationFlags(
    FfxBlurKernelPermutation kernelPermutation,
    FfxBlurKernelSize kernelSize,
    FfxBlurFloatPrecision desiredFloatPrecision, bool fp16Supported,
    bool canForceWave64)
{
    // work out what permutation to load.
    uint32_t flags = 0;

    switch (kernelPermutation)
    {
    case FFX_BLUR_KERNEL_PERMUTATION_0:
        flags |= BLUR_SHADER_PERMUTATION_KERNEL_0;
        break;
    case FFX_BLUR_KERNEL_PERMUTATION_1:
        flags |= BLUR_SHADER_PERMUTATION_KERNEL_1;
        break;
    case FFX_BLUR_KERNEL_PERMUTATION_2:
        flags |= BLUR_SHADER_PERMUTATION_KERNEL_2;
        break;
    }

    switch (kernelSize)
    {
    case FFX_BLUR_KERNEL_SIZE_3x3:
        flags |= BLUR_SHADER_PERMUTATION_3x3_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_5x5:
        flags |= BLUR_SHADER_PERMUTATION_5x5_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_7x7:
        flags |= BLUR_SHADER_PERMUTATION_7x7_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_9x9:
        flags |= BLUR_SHADER_PERMUTATION_9x9_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_11x11:
        flags |= BLUR_SHADER_PERMUTATION_11x11_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_13x13:
        flags |= BLUR_SHADER_PERMUTATION_13x13_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_15x15:
        flags |= BLUR_SHADER_PERMUTATION_15x15_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_17x17:
        flags |= BLUR_SHADER_PERMUTATION_17x17_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_19x19:
        flags |= BLUR_SHADER_PERMUTATION_19x19_KERNEL;
        break;
    case FFX_BLUR_KERNEL_SIZE_21x21:
        flags |= BLUR_SHADER_PERMUTATION_21x21_KERNEL;
        break;
    }

    if (desiredFloatPrecision == FFX_BLUR_FLOAT_PRECISION_16BIT
        && fp16Supported)
    {
        flags |= BLUR_SHADER_PERMUTATION_ALLOW_FP16;
    }

    if (canForceWave64)
    {
        flags |= BLUR_SHADER_PERMUTATION_FORCE_WAVE64;
    }

    return flags;
}

static uint32_t countNumberOfSetBits(uint32_t bits)
{
    uint32_t count = 0;
    while (bits > 0)
    {
        if (bits & 1)
            ++count;

        bits >>= 1;
    }

    return count;
}

#ifdef _DEBUG

#define FFX_ASSERT_OR_RETURN(condition, falseValue) FFX_ASSERT(condition)

#else

#define FFX_ASSERT_OR_RETURN(condition, falseValue) if (!(condition)) return falseValue;

#endif


static FfxErrorCode createPipelineStateObjects(FfxBlurContext_Private* context)
{
    FFX_ASSERT(context);

    FfxPipelineDescription pipelineDescription  = {};
    pipelineDescription.contextFlags = 0;

    // Samplers
    pipelineDescription.samplerCount = 0;
    pipelineDescription.samplers = nullptr;

    // Root constants
    pipelineDescription.rootConstantBufferCount = 1;
    FfxRootConstantDescription rootConstantDesc = { sizeof(BlurConstants) / sizeof(uint32_t), FFX_BIND_COMPUTE_SHADER_STAGE };
    pipelineDescription.rootConstants = &rootConstantDesc;

    FfxDeviceCapabilities& capabilities = context->deviceCapabilities;
    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    bool fp16Supported     = capabilities.fp16Supported;
    bool canForceWave64    = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
        canForceWave64 = haveShaderModel66;
    else
        canForceWave64 = false;

    uint32_t numberOfKernelPermutations = countNumberOfSetBits(context->contextDescription.kernelPermutations);

    FFX_ASSERT_OR_RETURN(numberOfKernelPermutations <= FFX_BLUR_KERNEL_PERMUTATION_COUNT && numberOfKernelPermutations != 0, FFX_ERROR_INVALID_ARGUMENT);

    uint32_t numberOfKernelSizes = countNumberOfSetBits(context->contextDescription.kernelSizes);

    FFX_ASSERT_OR_RETURN(numberOfKernelSizes <= FFX_BLUR_KERNEL_SIZE_COUNT && numberOfKernelSizes != 0, FFX_ERROR_INVALID_ARGUMENT);

    context->pBlurPipelines =
        (FfxPipelineState*)calloc(1u, numberOfKernelSizes * numberOfKernelPermutations * sizeof(FfxPipelineState));

    FFX_RETURN_ON_ERROR(context->pBlurPipelines, FFX_ERROR_OUT_OF_MEMORY);

    context->numKernelSizes = numberOfKernelSizes;

    uint32_t curPipelineIndex = 0;
    uint32_t curKernelPermutation = FFX_BLUR_KERNEL_PERMUTATION_0;
    for (uint32_t kernPermIndex = 0; kernPermIndex < FFX_BLUR_KERNEL_PERMUTATION_COUNT; ++kernPermIndex, curKernelPermutation <<= 1)
    {
        if (curKernelPermutation & context->contextDescription.kernelPermutations)
        {
            uint32_t curKernelSize = FFX_BLUR_KERNEL_SIZE_3x3;
            for (uint32_t psoIndex = 0; psoIndex < FFX_BLUR_KERNEL_SIZE_COUNT; ++psoIndex, curKernelSize <<= 1)
            {
                if (curKernelSize & context->contextDescription.kernelSizes)
                {
                    wcscpy_s(pipelineDescription.name, L"BLUR-BLUR_");

                    wchar_t kernelPermStr[32];
                    swprintf_s(kernelPermStr, L"PERM%d_", kernPermIndex);

                    wcscat_s(pipelineDescription.name, kernelPermStr);

                    wchar_t kernel[10]; // 3x3 through 21x21
                    getKernelSizeString(kernel, (FfxBlurKernelSize)curKernelSize);

                    wcscat_s(pipelineDescription.name, kernel);

                    FfxPipelineState* pBlurPipeline = &context->pBlurPipelines[curPipelineIndex];
                    // Set up pipeline descriptors (basically RootSignature and binding)
                    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(
                        &context->contextDescription.backendInterface,
                        FFX_EFFECT_BLUR,
                        FFX_BLUR_PASS_BLUR,
                        getPipelinePermutationFlags(
                            (FfxBlurKernelPermutation)curKernelPermutation,
                            (FfxBlurKernelSize)curKernelSize,
                            context->contextDescription.floatPrecision,
                            fp16Supported,
                            canForceWave64),
                        &pipelineDescription,
                        context->effectContextId,
                        pBlurPipeline));

                    // For each pipeline: re-route/fix-up IDs based on names
                    auto patchStatus = patchResourceBindings(pBlurPipeline);
                    if (patchStatus != FFX_OK)
                    {
                        free(context->pBlurPipelines);
                        context->pBlurPipelines = nullptr;
                        return patchStatus;
                    }

                    ++curPipelineIndex;
                }
            }
        }
    }

    return FFX_OK;
}

static FfxErrorCode blurCreate(FfxBlurContext_Private* context, const FfxBlurContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxBlurContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxBlurContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    context->blurConstants.num32BitEntries = sizeof(BlurConstants) / sizeof(uint32_t);

    // Create the context.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(
            &context->contextDescription.backendInterface, 
            FFX_EFFECT_BLUR,
            nullptr,
            &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(
        &context->contextDescription.backendInterface,
        &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Clear the SRV resources to NULL.
    memset(context->srvResources, 0, sizeof(context->srvResources));

    // And copy resources to uavResrouces list
    memcpy(context->uavResources, context->srvResources, sizeof(context->srvResources));

    // Create shaders on initialize.
    errorCode = createPipelineStateObjects(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    return FFX_OK;
}

FfxErrorCode ffxBlurContextCreate(FfxBlurContext* context, const FfxBlurContextDescription* contextDescription)
{
    // Zero context memory
    memset(context, 0, sizeof(*context));

    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        contextDescription,
        FFX_ERROR_INVALID_POINTER);

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer) {

        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // Ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(*context) >= sizeof(FfxBlurContext_Private));

    // create the context.
    FfxBlurContext_Private* contextPrivate = (FfxBlurContext_Private*)(context);
    const FfxErrorCode errorCode = blurCreate(contextPrivate, contextDescription);

    return errorCode;
}

static FfxErrorCode blurRelease(FfxBlurContext_Private* context)
{
    FFX_ASSERT(context);

    if (context->pBlurPipelines != nullptr)
    {
        // Release all pipelines
        uint32_t kernPermCount = countNumberOfSetBits(context->contextDescription.kernelPermutations);
        for (uint32_t curKernPermIndex = 0; curKernPermIndex < kernPermCount; ++curKernPermIndex)
        {
            for (uint32_t curPipelineIndex = 0; curPipelineIndex < context->numKernelSizes; ++curPipelineIndex)
            {
                FfxPipelineState* pBlurPipeline = &context->pBlurPipelines[(curKernPermIndex * context->numKernelSizes) + curPipelineIndex];
                ffxSafeReleasePipeline(&context->contextDescription.backendInterface, pBlurPipeline, context->effectContextId);
            }
        }

        free(context->pBlurPipelines);
        context->pBlurPipelines = nullptr;
    }

    // Unregister resources not created internally
    context->srvResources[FFX_BLUR_RESOURCE_IDENTIFIER_INPUT_SRC] = {FFX_BLUR_RESOURCE_IDENTIFIER_NULL};
    context->srvResources[FFX_BLUR_RESOURCE_IDENTIFIER_OUTPUT] = {FFX_BLUR_RESOURCE_IDENTIFIER_NULL};

    context->uavResources[FFX_BLUR_RESOURCE_IDENTIFIER_INPUT_SRC] = {FFX_BLUR_RESOURCE_IDENTIFIER_NULL};
    context->uavResources[FFX_BLUR_RESOURCE_IDENTIFIER_OUTPUT] = {FFX_BLUR_RESOURCE_IDENTIFIER_NULL};

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(
        &context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxBlurContextDestroy(FfxBlurContext* context)
{
    FFX_RETURN_ON_ERROR(
        context,
        FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxBlurContext_Private* contextPrivate = (FfxBlurContext_Private*)(context);
    const FfxErrorCode errorCode = blurRelease(contextPrivate);
    return errorCode;
}

static void scheduleDispatch(FfxBlurContext_Private* context,
                             const FfxPipelineState* pipeline,
                             uint32_t                dispatchX,
                             uint32_t                dispatchY,
                             uint32_t                dispatchZ)
{
    FfxGpuJobDescription dispatchJob = {FFX_GPU_JOB_COMPUTE};
    wcscpy_s(dispatchJob.jobLabel, pipeline->name);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        const uint32_t            currentResourceId        = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource          = context->srvResources[currentResourceId];

        dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name,
            pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    uint32_t uavEntry = 0;  // Uav resource offset (accounts for uav arrays)
    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        uint32_t currentResourceId = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;

        const FfxResourceInternal currentResource = context->uavResources[currentResourceId];

        dispatchJob.computeJobDescriptor.uavTextures[uavEntry].resource = currentResource;
        dispatchJob.computeJobDescriptor.uavTextures[uavEntry++].mip = 0;
#ifdef FFX_DEBUG
        wcscpy_s(dispatchJob.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name,
            pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    dispatchJob.computeJobDescriptor.dimensions[0] = dispatchX;
    dispatchJob.computeJobDescriptor.dimensions[1] = dispatchY;
    dispatchJob.computeJobDescriptor.dimensions[2] = dispatchZ;
    dispatchJob.computeJobDescriptor.pipeline      = *pipeline;

    // Only 1 constant buffer
#ifdef FFX_DEBUG
    wcscpy_s(dispatchJob.computeJobDescriptor.cbNames[0], pipeline->constantBufferBindings[0].name);
#endif
    dispatchJob.computeJobDescriptor.cbs[0] = context->blurConstants;


    context->contextDescription.backendInterface.fpScheduleGpuJob(
        &context->contextDescription.backendInterface, &dispatchJob);
}

static uint32_t getPipelineIndex(
    FfxBlurKernelPermutations kernelPerms,
    FfxBlurKernelPermutation kernelPerm,
    FfxUInt32 numKernelSizes,
    FfxBlurKernelSizes kernelSizes,
    FfxBlurKernelSize kernelSize)
{
    uint32_t kernelPermValue = static_cast<uint32_t>(kernelPerm);

    uint32_t pipelineIndex = 0;
    while (kernelPerms > 0)
    {
        if (kernelPermValue & 1)
        {
            uint32_t kernelSizeValue = static_cast<uint32_t>(kernelSize);
            FfxBlurKernelSizes kernelSizesCounter = kernelSizes;
            while (kernelSizesCounter > 0)
            {
                if (kernelSizeValue & 1)
                    break; // We found it.

                if (kernelSizesCounter & 1)
                    ++pipelineIndex;

                kernelSizesCounter >>= 1;
                kernelSizeValue >>= 1;
            }

            break; // If we get here we found it or something is wrong.
        }
        else if (kernelPerms & 1)
        {
            // Skip over the PSOs for this permutation of the kernel.
            pipelineIndex += numKernelSizes;
        }

        kernelPerms >>= 1;
        kernelPermValue >>= 1;
    }

    return pipelineIndex;
}

static FfxErrorCode blurDispatch(FfxBlurContext_Private* context, const FfxBlurDispatchDescription* params)
{
    // take a short cut to the command list
    FfxCommandList commandList = params->commandList;

    // Register resources for frame
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &params->input, context->effectContextId,
        &context->srvResources[FFX_BLUR_RESOURCE_IDENTIFIER_INPUT_SRC]);
    context->contextDescription.backendInterface.fpRegisterResource(
        &context->contextDescription.backendInterface, &params->output, context->effectContextId,
        &context->uavResources[FFX_BLUR_RESOURCE_IDENTIFIER_OUTPUT]);

    BlurConstants constants;
    constants.width = params->inputAndOutputSize.width;
    constants.height = params->inputAndOutputSize.height;

    // FFX-Blur uses persistent waves - A single row of work groups loop over the image.
    uint32_t dispatchX = FFX_DIVIDE_ROUNDING_UP(constants.width, FFX_BLUR_TILE_SIZE_X);
    uint32_t dispatchY = FFX_BLUR_DISPATCH_Y;
    uint32_t dispatchZ = 1;

    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(
        &context->contextDescription.backendInterface, &constants, sizeof(BlurConstants), &context->blurConstants);
    
    // Validate that specified kernel permutation and size were used during FFX Blur Context creation.
    FFX_ASSERT_OR_RETURN(context->contextDescription.kernelPermutations & params->kernelPermutation, FFX_ERROR_INVALID_ENUM);
    FFX_ASSERT_OR_RETURN(context->contextDescription.kernelSizes & params->kernelSize, FFX_ERROR_INVALID_ENUM);

    uint32_t pipelineIndex =
        getPipelineIndex(
            context->contextDescription.kernelPermutations, params->kernelPermutation,
            context->numKernelSizes,
            context->contextDescription.kernelSizes, params->kernelSize);

    scheduleDispatch(context, &context->pBlurPipelines[pipelineIndex], dispatchX, dispatchY, dispatchZ);

    // Execute all the work for the frame
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, commandList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, 
        commandList, 
        context->effectContextId);

    return FFX_OK;
}

FfxErrorCode ffxBlurContextDispatch(FfxBlurContext* context, const FfxBlurDispatchDescription* dispatchDescription)
{
    // check pointers are valid
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(dispatchDescription, FFX_ERROR_INVALID_POINTER);
    
    FfxBlurContext_Private* contextPrivate = (FfxBlurContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the SPD pass
    const FfxErrorCode errorCode = blurDispatch(contextPrivate, dispatchDescription);
    return errorCode;
}

FFX_API FfxVersionNumber ffxBlurGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_BLUR_VERSION_MAJOR, FFX_BLUR_VERSION_MINOR, FFX_BLUR_VERSION_PATCH);
}
