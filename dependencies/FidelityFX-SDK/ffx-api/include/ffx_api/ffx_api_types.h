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

#include <stdint.h>

/// An enumeration of surface formats. Needs to match enum FfxSurfaceFormat
enum FfxApiSurfaceFormat
{
    FFX_API_SURFACE_FORMAT_UNKNOWN,                     ///< Unknown format
    FFX_API_SURFACE_FORMAT_R32G32B32A32_TYPELESS,       ///< 32 bit per channel, 4 channel typeless format
    FFX_API_SURFACE_FORMAT_R32G32B32A32_UINT,           ///< 32 bit per channel, 4 channel uint format
    FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT,          ///< 32 bit per channel, 4 channel float format
    FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT,          ///< 16 bit per channel, 4 channel float format
    FFX_API_SURFACE_FORMAT_R32G32B32_FLOAT,             ///< 32 bit per channel, 3 channel float format
    FFX_API_SURFACE_FORMAT_R32G32_FLOAT,                ///< 32 bit per channel, 2 channel float format
    FFX_API_SURFACE_FORMAT_R8_UINT,                     ///< 8 bit per channel, 1 channel float format
    FFX_API_SURFACE_FORMAT_R32_UINT,                    ///< 32 bit per channel, 1 channel float format
    FFX_API_SURFACE_FORMAT_R8G8B8A8_TYPELESS,           ///<  8 bit per channel, 4 channel typeless format
    FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM,              ///<  8 bit per channel, 4 channel unsigned normalized format
    FFX_API_SURFACE_FORMAT_R8G8B8A8_SNORM,              ///<  8 bit per channel, 4 channel signed normalized format
    FFX_API_SURFACE_FORMAT_R8G8B8A8_SRGB,               ///<  8 bit per channel, 4 channel srgb normalized
    FFX_API_SURFACE_FORMAT_B8G8R8A8_TYPELESS,           ///<  8 bit per channel, 4 channel typeless format
    FFX_API_SURFACE_FORMAT_B8G8R8A8_UNORM,              ///<  8 bit per channel, 4 channel unsigned normalized format
    FFX_API_SURFACE_FORMAT_B8G8R8A8_SRGB,               ///<  8 bit per channel, 4 channel srgb normalized
    FFX_API_SURFACE_FORMAT_R11G11B10_FLOAT,             ///< 32 bit 3 channel float format
    FFX_API_SURFACE_FORMAT_R10G10B10A2_UNORM,           ///< 10 bit per 3 channel, 2 bit for 1 channel normalized format
    FFX_API_SURFACE_FORMAT_R16G16_FLOAT,                ///< 16 bit per channel, 2 channel float format
    FFX_API_SURFACE_FORMAT_R16G16_UINT,                 ///< 16 bit per channel, 2 channel unsigned int format
    FFX_API_SURFACE_FORMAT_R16G16_SINT,                 ///< 16 bit per channel, 2 channel signed int format
    FFX_API_SURFACE_FORMAT_R16_FLOAT,                   ///< 16 bit per channel, 1 channel float format
    FFX_API_SURFACE_FORMAT_R16_UINT,                    ///< 16 bit per channel, 1 channel unsigned int format
    FFX_API_SURFACE_FORMAT_R16_UNORM,                   ///< 16 bit per channel, 1 channel unsigned normalized format
    FFX_API_SURFACE_FORMAT_R16_SNORM,                   ///< 16 bit per channel, 1 channel signed normalized format
    FFX_API_SURFACE_FORMAT_R8_UNORM,                    ///<  8 bit per channel, 1 channel unsigned normalized format
    FFX_API_SURFACE_FORMAT_R8G8_UNORM,                  ///<  8 bit per channel, 2 channel unsigned normalized format
    FFX_API_SURFACE_FORMAT_R8G8_UINT,                   ///<  8 bit per channel, 2 channel unsigned integer format
    FFX_API_SURFACE_FORMAT_R32_FLOAT,                   ///< 32 bit per channel, 1 channel float format
    FFX_API_SURFACE_FORMAT_R9G9B9E5_SHAREDEXP,          ///<  9 bit per channel, 5 bit exponent format

    FFX_API_SURFACE_FORMAT_R16G16B16A16_TYPELESS,       ///< 16 bit per channel, 4 channel typeless format
    FFX_API_SURFACE_FORMAT_R32G32_TYPELESS,             ///< 32 bit per channel, 2 channel typeless format
    FFX_API_SURFACE_FORMAT_R10G10B10A2_TYPELESS,        ///< 10 bit per 3 channel, 2 bit for 1 channel typeless format
    FFX_API_SURFACE_FORMAT_R16G16_TYPELESS,             ///< 16 bit per channel, 2 channel typless format
    FFX_API_SURFACE_FORMAT_R16_TYPELESS,                ///< 16 bit per channel, 1 channel typeless format
    FFX_API_SURFACE_FORMAT_R8_TYPELESS,                 ///<  8 bit per channel, 1 channel typeless format
    FFX_API_SURFACE_FORMAT_R8G8_TYPELESS,               ///<  8 bit per channel, 2 channel typeless format
    FFX_API_SURFACE_FORMAT_R32_TYPELESS,                ///< 32 bit per channel, 1 channel typeless format
};

/// An enumeration of resource usage.
enum FfxApiResourceUsage
{
    FFX_API_RESOURCE_USAGE_READ_ONLY = 0,                   ///< No usage flags indicate a resource is read only.
    FFX_API_RESOURCE_USAGE_RENDERTARGET = (1<<0),           ///< Indicates a resource will be used as render target.
    FFX_API_RESOURCE_USAGE_UAV = (1<<1),                    ///< Indicates a resource will be used as UAV.
    FFX_API_RESOURCE_USAGE_DEPTHTARGET = (1<<2),            ///< Indicates a resource will be used as depth target.
    FFX_API_RESOURCE_USAGE_INDIRECT = (1<<3),               ///< Indicates a resource will be used as indirect argument buffer
    FFX_API_RESOURCE_USAGE_ARRAYVIEW = (1<<4),              ///< Indicates a resource that will generate array views. Works on 2D and cubemap textures
    FFX_API_RESOURCE_USAGE_STENCILTARGET = (1<<5),          ///< Indicates a resource will be used as stencil target.
};
typedef FfxApiResourceUsage FfxApiResorceUsage;             // Corrects a typo that shipped with original API

/// An enumeration of resource states.
enum FfxApiResourceState
{
    FFX_API_RESOURCE_STATE_COMMON               = (1 << 0),
    FFX_API_RESOURCE_STATE_UNORDERED_ACCESS     = (1 << 1), ///< Indicates a resource is in the state to be used as UAV.
    FFX_API_RESOURCE_STATE_COMPUTE_READ         = (1 << 2), ///< Indicates a resource is in the state to be read by compute shaders.
    FFX_API_RESOURCE_STATE_PIXEL_READ           = (1 << 3), ///< Indicates a resource is in the state to be read by pixel shaders.
    FFX_API_RESOURCE_STATE_PIXEL_COMPUTE_READ   = (FFX_API_RESOURCE_STATE_PIXEL_READ | FFX_API_RESOURCE_STATE_COMPUTE_READ), ///< Indicates a resource is in the state to be read by pixel or compute shaders.
    FFX_API_RESOURCE_STATE_COPY_SRC             = (1 << 4), ///< Indicates a resource is in the state to be used as source in a copy command.
    FFX_API_RESOURCE_STATE_COPY_DEST            = (1 << 5), ///< Indicates a resource is in the state to be used as destination in a copy command.
    FFX_API_RESOURCE_STATE_GENERIC_READ         = (FFX_API_RESOURCE_STATE_COPY_SRC | FFX_API_RESOURCE_STATE_COMPUTE_READ),  ///< Indicates a resource is in generic (slow) read state.
    FFX_API_RESOURCE_STATE_INDIRECT_ARGUMENT    = (1 << 6), ///< Indicates a resource is in the state to be used as an indirect command argument
    FFX_API_RESOURCE_STATE_PRESENT              = (1 << 7), ///< Indicates a resource is in the state to be used to present to the swap chain
    FFX_API_RESOURCE_STATE_RENDER_TARGET        = (1 << 8), ///< Indicates a resource is in the state to be used as render target
};

/// An enumeration of surface dimensions.
enum FfxApiResourceDimension
{
    FFX_API_RESOURCE_DIMENSION_TEXTURE_1D,              ///< A resource with a single dimension.
    FFX_API_RESOURCE_DIMENSION_TEXTURE_2D,              ///< A resource with two dimensions.
};

/// An enumeration of resource flags.
enum FfxApiResourceFlags
{
    FFX_API_RESOURCE_FLAGS_NONE             = 0,            ///< No flags.
    FFX_API_RESOURCE_FLAGS_ALIASABLE        = (1 << 0),     ///< A bit indicating a resource does not need to persist across frames.
    FFX_API_RESOURCE_FLAGS_UNDEFINED        = (1 << 1),     ///< Special case flag used internally when importing resources that require additional setup
};

// An enumeration for different resource types
enum FfxApiResourceType
{
    FFX_API_RESOURCE_TYPE_BUFFER,                       ///< The resource is a buffer.
    FFX_API_RESOURCE_TYPE_TEXTURE1D,                    ///< The resource is a 1-dimensional texture.
    FFX_API_RESOURCE_TYPE_TEXTURE2D,                    ///< The resource is a 2-dimensional texture.
    FFX_API_RESOURCE_TYPE_TEXTURE_CUBE,                 ///< The resource is a cube map.
    FFX_API_RESOURCE_TYPE_TEXTURE3D,                    ///< The resource is a 3-dimensional texture.
};

enum FfxApiBackbufferTransferFunction
{
    FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB,
    FFX_API_BACKBUFFER_TRANSFER_FUNCTION_PQ,
    FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SCRGB
};

/// A structure encapsulating a 2-dimensional point, using 32bit unsigned integers.
struct FfxApiDimensions2D
{
    uint32_t width;  ///< The width of a 2-dimensional range.
    uint32_t height; ///< The height of a 2-dimensional range.
};

/// A structure encapsulating a 2-dimensional set of floating point coordinates.
struct FfxApiFloatCoords2D
{
    float x; ///< The x coordinate of a 2-dimensional point.
    float y; ///< The y coordinate of a 2-dimensional point.
};

/// A structure encapsulating a 2-dimensional rect.
struct FfxApiRect2D
{
    int32_t left;
    int32_t top;
    int32_t width;
    int32_t height;
};

/// A structure describing a resource.
///
/// @ingroup SDKTypes
struct FfxApiResourceDescription
{
    uint32_t     type;      ///< The type of the resource.
    uint32_t     format;    ///< The surface format.
    union {
        uint32_t width;     ///< The width of the texture resource.
        uint32_t size;      ///< The size of the buffer resource.
    };

    union {
        uint32_t height;    ///< The height of the texture resource.
        uint32_t stride;    ///< The stride of the buffer resource.
    };

    union {
        uint32_t depth;     ///< The depth of the texture resource.
        uint32_t alignment; ///< The alignment of the buffer resource.
    };

    uint32_t     mipCount;  ///< Number of mips (or 0 for full mipchain).
    uint32_t     flags;     ///< A set of resource flags.
    uint32_t     usage;     ///< Resource usage flags.
};

struct FfxApiResource
{
    void* resource;
    struct FfxApiResourceDescription description;
    uint32_t state;
};

//struct definition matches FfxEffectMemoryUsage
typedef struct FfxApiEffectMemoryUsage
{
    uint64_t totalUsageInBytes;
    uint64_t aliasableUsageInBytes;
} FfxApiEffectMemoryUsage;

/*
Tuning varianceFactor and safetyMarginInMs Tips:
Calculation of frame pacing algorithm's next target timestamp: 
target frametime delta = average Frametime - (variance * varianceFactor) - safetyMarginInMs

Default Tuning uses safetyMarginInMs==0.1ms and varianceFactor==0.1.
Say Tuning set A uses safetyMarginInMs==0.75ms, and varianceFactor==0.1.
Say Tuning Set B uses safetyMarginInMs==0.01ms and varianceFactor==0.3.

Example #1 - Actual Game Cutscene. Game's framerate after FG ON during camera pan from normal (19ms avg frametime) to complex (37ms avg frametime over 1 sec) and back to normal scene complexity (19ms avg frametime).
After the panning is done, 
- Default Tuning now gets stuck at targeting ~33ms after panning to a complex scene. GPU utilization significantly lower in this case.
- Tuning Set B and Tuning Set A are able to recover close to ~19ms because of these 2 tuning result in lower "target frametime delta" than Default Tuning. 

However, larger varianceFactor or safetyMarginInMs results in higher variance. As seen in Example #2 bellow.

Example #2 - FFX_API_FSR sample. Set app fps cap to 33.33ms. Use OCAT to capture 10s at default camera position.
FSR 3.1.0 FG msbetweenpresents ping-pong between 16.552 (5th-percentile) and 16.832 (95th-percentile). Variance is 0.01116.
Tuning set A FG msbetweenpresents ping-pong between 15.901 (5th-percentile) and 17.500 (95th-percentile). Variance is 0.057674.
Tuning Set B FG msbetweenpresents ping-pong between 16.589 (5th-percentile) and 16.971 (95th-percentile). Variance is 0.014452.

| FG output Frames timestamp | n | n+1    | n+2   | n+3    | frame delta n+1 to n+2 | frame delta n+2 to n+3 |
| -------------------------- | - | ------ | ----- | ------ | ---------------------- | ---------------------- |
| App real frame presents    | 0 |        | 33.33 |        |                        |                        |
| Default Tuning             | 0 | 16.552 | 33.33 | 49.882 | 16.778                 | 16.552                 |
| Tuning Set A               | 0 | 15.901 | 33.33 | 49.231 | 17.429                 | 15.901                 |
| Tuning Set B               | 0 | 16.589 | 33.33 | 49.919 | 16.741                 | 16.589                 |

Analysis of table data in words:
Ignoring the cost of FI,
"Tuning set A"'s "target frametime delta" of 15.901 results in larger frame to frame delta (or in other words larger variance) vs Default tuning.
"Tuning set B"'s "target frametime delta" of 16.589 results in a bit larger frame to frame delta (or in other words a bit larger variance) vs Default tuning.

TLDR:
If your game when using FG, frame rate is running at unexpectly low frame rate, after gradual transition from rendering complex to easy scene complexity, you could try setting "Tuning Set B" to recover lost FPS at cost of a bit higher variance.

*/

//struct definition matches FfxSwapchainFramePacingTuning
typedef struct FfxApiSwapchainFramePacingTuning
{
    float safetyMarginInMs; // in Millisecond. Default is 0.1ms
    float varianceFactor; // valid range [0.0,1.0]. Default is 0.1
    bool     allowHybridSpin; //Allows pacing spinlock to sleep. Default is false.
    uint32_t hybridSpinTime;  //How long to spin if allowHybridSpin is true. Measured in timer resolution units. Not recommended to go below 2. Will result in frequent overshoots. Default is 2.
    bool     allowWaitForSingleObjectOnFence; //Allows WaitForSingleObject instead of spinning for fence value. Default is false.
} FfxApiSwapchainFramePacingTuning;
