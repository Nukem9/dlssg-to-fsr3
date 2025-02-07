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

#pragma once

#include "ffx_api.h"
#include "ffx_api_types.h"

#define FFX_API_EFFECT_ID_FRAMEGENERATION 0x00020000u

#if defined(__cplusplus)
extern "C" {
#endif

enum FfxApiCreateContextFramegenerationFlags
{
    FFX_FRAMEGENERATION_ENABLE_ASYNC_WORKLOAD_SUPPORT              = (1<<0),
    FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS   = (1<<1), ///< A bit indicating if the motion vectors are rendered at display resolution.
    FFX_FRAMEGENERATION_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION  = (1<<2), ///< A bit indicating that the motion vectors have the jittering pattern applied to them.
    FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED                      = (1<<3), ///< A bit indicating that the input depth buffer data provided is inverted [1..0].
    FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE                      = (1<<4), ///< A bit indicating that the input depth buffer data provided is using an infinite far plane.
    FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE                  = (1<<5), ///< A bit indicating if the input color data provided to all inputs is using a high-dynamic range.
};

enum FfxApiDispatchFramegenerationFlags
{
    FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_TEAR_LINES       = (1 << 0),  ///< A bit indicating that the debug tear lines will be drawn to the generated output.
    FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_RESET_INDICATORS = (1 << 1),  ///< A bit indicating that the debug reset indicators will be drawn to the generated output.
    FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_VIEW = (1 << 2),              ///< A bit indicating that the generated output resource will contain debug views with relevant information.
    FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY = (1 << 3),  ///< A bit indicating that the context should only run frame interpolation and not modify the swapchain.
    FFX_FRAMEGENERATION_FLAG_DRAW_DEBUG_PACING_LINES = (1 << 4),      ///< A bit indicating that the debug pacing lines will be drawn to the generated output.

};

enum FfxApiUiCompositionFlags
{
    FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_USE_PREMUL_ALPHA                    = (1 << 0),  ///< A bit indicating that we use premultiplied alpha for UI composition.
    FFX_FRAMEGENERATION_UI_COMPOSITION_FLAG_ENABLE_INTERNAL_UI_DOUBLE_BUFFERING = (1 << 1),  ///< A bit indicating that the swapchain should doublebuffer the UI resource.
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION 0x00020001u
struct ffxCreateContextDescFrameGeneration
{
    ffxCreateContextDescHeader header;
    uint32_t flags;                             ///< A combination of zero or more values from FfxApiCreateContextFramegenerationFlags.
    struct FfxApiDimensions2D displaySize;      ///< The resolution at which both rendered and generated frames will be displayed.
    struct FfxApiDimensions2D maxRenderSize;    ///< The maximum rendering resolution.
    uint32_t backBufferFormat;                  ///< The surface format for the backbuffer. One of the values from FfxApiSurfaceFormat.
};

#define FFX_API_CALLBACK_DESC_TYPE_FRAMEGENERATION_PRESENT 0x00020005u
struct ffxCallbackDescFrameGenerationPresent
{
    ffxDispatchDescHeader header;
    void* device;                                   ///< The device passed in (from a backend description) during context creation.
    void* commandList;                              ///< A command list that will be executed before presentation.
    struct FfxApiResource currentBackBuffer;        ///< Backbuffer image either rendered or generated.
    struct FfxApiResource currentUI;                ///< UI image for composition if passed. Otherwise empty.
    struct FfxApiResource outputSwapChainBuffer;    ///< Output image that will be presented.
    bool isGeneratedFrame;                          ///< true if this frame is generated, false if rendered.
    uint64_t              frameID;                  ///< Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic.
};

#define FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION 0x00020003u
struct ffxDispatchDescFrameGeneration
{
    ffxDispatchDescHeader header;
    void*                 commandList;                ///< The command list on which to register render commands.
    struct FfxApiResource presentColor;               ///< The current presentation color, this will be used as source data.
    struct FfxApiResource outputs[4];                 ///< Destination targets (1 for each frame in numGeneratedFrames).
    uint32_t              numGeneratedFrames;         ///< The number of frames to generate from the passed in color target.
    bool                  reset;                      ///< A boolean value which when set to true, indicates the camera has moved discontinuously.
    uint32_t              backbufferTransferFunction; ///< The transfer function use to convert frame generation source color data to linear RGB. One of the values from FfxApiBackbufferTransferFunction.
    float                 minMaxLuminance[2];         ///< Min and max luminance values, used when converting HDR colors to linear RGB.
    struct FfxApiRect2D   generationRect;             ///< The area of the backbuffer that should be used for generation in case only a part of the screen is used e.g. due to movie bars.
    uint64_t              frameID;                    ///< Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic.
};

typedef ffxReturnCode_t(*FfxApiPresentCallbackFunc)(ffxCallbackDescFrameGenerationPresent* params, void* pUserCtx);
typedef ffxReturnCode_t(*FfxApiFrameGenerationDispatchFunc)(ffxDispatchDescFrameGeneration* params, void* pUserCtx);

#define FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION 0x00020002u
struct ffxConfigureDescFrameGeneration
{
    ffxConfigureDescHeader header;
    void* swapChain;                                            ///< The swapchain to use with frame generation.
    FfxApiPresentCallbackFunc presentCallback;                  ///< A UI composition callback to call when finalizing the frame image.
    void* presentCallbackUserContext;                           ///< A pointer to be passed to the UI composition callback.
    FfxApiFrameGenerationDispatchFunc frameGenerationCallback;  ///< The frame generation callback to use to generate a frame.
    void* frameGenerationCallbackUserContext;                   ///< A pointer to be passed to the frame generation callback.
    bool frameGenerationEnabled;                                ///< Sets the state of frame generation. Set to false to disable frame generation.
    bool allowAsyncWorkloads;                                   ///< Sets the state of async workloads. Set to true to enable generation work on async compute.
    struct FfxApiResource HUDLessColor;                         ///< The hudless back buffer image to use for UI extraction from backbuffer resource. May be empty.
    uint32_t flags;                                             ///< Zero or combination of flags from FfxApiDispatchFrameGenerationFlags.
    bool onlyPresentGenerated;                                  ///< Set to true to only present generated frames.
    struct FfxApiRect2D generationRect;                         ///< The area of the backbuffer that should be used for generation in case only a part of the screen is used e.g. due to movie bars
    uint64_t frameID;                                           ///< Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic.
};

#define FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE 0x00020004u
struct ffxDispatchDescFrameGenerationPrepare
{
    ffxDispatchDescHeader      header;
    uint64_t                   frameID;            ///< Identifier used to select internal resources when async support is enabled. Must increment by exactly one (1) for each frame. Any non-exactly-one difference will reset the frame generation logic.
    uint32_t                   flags;              ///< Zero or combination of values from FfxApiDispatchFrameGenerationFlags.
    void*                      commandList;        ///< A command list to record frame generation commands into.
    struct FfxApiDimensions2D  renderSize;         ///< The dimensions used to render game content, dilatedDepth, dilatedMotionVectors are expected to be of ths size.
    struct FfxApiFloatCoords2D jitterOffset;       ///< The subpixel jitter offset applied to the camera.
    struct FfxApiFloatCoords2D motionVectorScale;  ///< The scale factor to apply to motion vectors.

    float                 frameTimeDelta;          ///< Time elapsed in milliseconds since the last frame.
    bool                  unused_reset;            ///< A (currently unused) boolean value which when set to true, indicates FrameGeneration will be called in reset mode
    float                 cameraNear;              ///< The distance to the near plane of the camera.
    float                 cameraFar;               ///< The distance to the far plane of the camera. This is used only used in case of non infinite depth.
    float                 cameraFovAngleVertical;  ///< The camera angle field of view in the vertical direction (expressed in radians).
    float                 viewSpaceToMetersFactor; ///< The scale factor to convert view space units to meters
    struct FfxApiResource depth;                   ///< The depth buffer data
    struct FfxApiResource motionVectors;           ///< The motion vector data    
};

#define FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_KEYVALUE 0x00020006u
struct ffxConfigureDescFrameGenerationKeyValue
{
    ffxConfigureDescHeader  header;
    uint64_t                key;        ///< Configuration key, member of the FfxApiConfigureFrameGenerationKey enumeration.
    uint64_t                u64;        ///< Integer value or enum value to set.
    void*                   ptr;        ///< Pointer to set or pointer to value to set.
};

enum FfxApiConfigureFrameGenerationKey
{
    // No values.
};

#define FFX_API_QUERY_DESC_TYPE_FRAMEGENERATION_GPU_MEMORY_USAGE 0x00020007u
struct ffxQueryDescFrameGenerationGetGPUMemoryUsage
{
    ffxQueryDescHeader header;
    struct FfxApiEffectMemoryUsage* gpuMemoryUsageFrameGeneration;
};

#define FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION_REGISTERDISTORTIONRESOURCE 0x00020008u
struct ffxConfigureDescFrameGenerationRegisterDistortionFieldResource
{
    ffxConfigureDescHeader header;
    struct FfxApiResource distortionField;            ///< A resource containing distortion offset data. Needs to be 2-component (ie. RG). Read by FG shaders via Sample. Resource's xy components encodes [UV coordinate of pixel after lens distortion effect- UV coordinate of pixel before lens distortion]. 
};

#define FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION_HUDLESS 0x00020009u
struct ffxCreateContextDescFrameGenerationHudless
{
    ffxCreateContextDescHeader header;
    uint32_t hudlessBackBufferFormat;           ///< The surface format for the hudless back buffer. One of the values from FfxApiSurfaceFormat.
};

#if defined(__cplusplus)
} // extern "C"
#endif
