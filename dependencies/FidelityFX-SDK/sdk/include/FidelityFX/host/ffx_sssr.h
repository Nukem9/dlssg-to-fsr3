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

// Include the interface for the backend of the SSSR API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup FfxSssr FidelityFX SSSR
/// FidelityFX Stochastic Screen Space Reflections runtime library
/// 
/// @ingroup SDKComponents

/// FidelityFX Stochastic Screen Space Reflections major version.
///
/// @ingroup FfxSssr
#define FFX_SSSR_VERSION_MAJOR      (1)

/// FidelityFX Stochastic Screen Space Reflections minor version.
///
/// @ingroup FfxSssr
#define FFX_SSSR_VERSION_MINOR      (5)

/// FidelityFX Stochastic Screen Space Reflections patch version.
///
/// @ingroup FfxSssr
#define FFX_SSSR_VERSION_PATCH      (0)

/// FidelityFX SSSR context count
/// 
/// Defines the number of internal effect contexts required by SSSR
/// We need 2, one for the SSSR context and one for the FidelityFX Denoiser
///
/// @ingroup FfxSssr
#define FFX_SSSR_CONTEXT_COUNT   2

/// The size of the context specified in 32bit values.
///
/// @ingroup FfxSssr
#define FFX_SSSR_CONTEXT_SIZE (118914)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// An enumeration of all the passes which constitute the SSSR algorithm.
///
/// SSSR is implemented as a composite of several compute passes each
/// computing a key part of the final result. Each call to the
/// <c><i>FfxSssrScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single pass included in <c><i>FfxSssrPass</i></c>. For a
/// more comprehensive description of each pass, please refer to the SSSR
/// reference documentation.
///
/// @ingroup FfxSssr
typedef enum FfxSssrPass
{
    FFX_SSSR_PASS_DEPTH_DOWNSAMPLE = 0,             ///< A pass which performs the hierarchical depth buffer generation
    FFX_SSSR_PASS_CLASSIFY_TILES = 1,               ///< A pass which classifies which pixels require screen space ray marching
    FFX_SSSR_PASS_PREPARE_BLUE_NOISE_TEXTURE = 2,   ///< A pass which generates an optimized blue noise texture
    FFX_SSSR_PASS_PREPARE_INDIRECT_ARGS = 3,        ///< A pass which generates the indirect arguments for the intersection pass.
    FFX_SSSR_PASS_INTERSECTION = 4,                 ///< A pass which performs the actual hierarchical depth ray marching.
    FFX_SSSR_PASS_COUNT
} FfxSssrPass;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxSssrContext</i></c>. See <c><i>FfxSssrContextDescription</i></c>.
///
/// @ingroup FfxSssr
typedef enum FfxSssrInitializationFlagBits {

    FFX_SSSR_ENABLE_DEPTH_INVERTED = (1 << 0)   ///< A bit indicating that the input depth buffer data provided is inverted [1..0].
} FfxSssrInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Stochastic Screen Space Reflections.
///
/// @ingroup ffxSssr
typedef struct FfxSssrContextDescription
{
    uint32_t                    flags;                          ///< A collection of <c><i>FfxSssrInitializationFlagBits</i></c>.
    FfxDimensions2D             renderSize;                     ///< The resolution we are currently rendering at
    FfxSurfaceFormat            normalsHistoryBufferFormat;     ///< The format used by the reflections denoiser to store the normals buffer history
    FfxInterface                backendInterface;               ///< A set of pointers to the backend implementation for FidelityFX SDK
} FfxSssrContextDescription;

/// A structure encapsulating the parameters for dispatching the various passes
/// of FidelityFX Stochastic Screen Space Reflections.
///
/// @ingroup ffxSssr
typedef struct FfxSssrDispatchDescription {

    FfxCommandList      commandList;                            ///< The <c><i>FfxCommandList</i></c> to record SSSR rendering commands into.
    FfxResource         color;                                  ///< A <c><i>FfxResource</i></c> containing the color buffer for the current frame.
    FfxResource         depth;                                  ///< A <c><i>FfxResource</i></c> containing the depth buffer for the current frame.
    FfxResource         motionVectors;                          ///< A <c><i>FfxResource</i></c> containing the motion vectors buffer for the current frame.
    FfxResource         normal;                                 ///< A <c><i>FfxResource</i></c> containing the normal buffer for the current frame.
    FfxResource         materialParameters;                     ///< A <c><i>FfxResource</i></c> containing the roughness buffer for the current frame. 
    FfxResource         environmentMap;                         ///< A <c><i>FfxResource</i></c> containing the environment map to fallback to when screenspace data is not sufficient.
    FfxResource         brdfTexture;                            ///< A <c><i>FfxResource</i></c> containing the precomputed brdf LUT.
    FfxResource         output;                                 ///< A <c><i>FfxResource</i></c> to store the result of the SSSR algorithm into.
    float               invViewProjection[16];                  ///< An array containing the inverse of the view projection matrix in column major layout.
    float               projection[16];                         ///< An array containing the projection matrix in column major layout.
    float               invProjection[16];                      ///< An array containing the inverse of the projection matrix in column major layout.
    float               view[16];                               ///< An array containing the view matrix in column major layout.
    float               invView[16];                            ///< An array containing the inverse of the view matrix in column major layout.
    float               prevViewProjection[16];                 ///< An array containing the previous frame's view projection matrix in column major layout.
    FfxDimensions2D     renderSize;                             ///< The resolution that was used for rendering the input resources.
    FfxFloatCoords2D    motionVectorScale;                      ///< The scale factor to apply to motion vectors.
    float               iblFactor;                              ///< A factor to control the intensity of the image based lighting. Set to 1 for an HDR probe.
    float               normalUnPackMul;                        ///< A multiply factor to transform the normal to the space expected by SSSR.
    float               normalUnPackAdd;                        ///< An offset to transform the normal to the space expected by SSSR.
    uint32_t            roughnessChannel;                       ///< The channel to read the roughness from the materialParameters texture
    bool                isRoughnessPerceptual;                  ///< A boolean to describe the space used to store roughness in the materialParameters texture. If false, we assume roughness squared was stored in the Gbuffer.
    float               temporalStabilityFactor;                ///< A factor to control the accmulation of history values. Higher values reduce noise, but are more likely to exhibit ghosting artefacts.
    float               depthBufferThickness;                   ///< A bias for accepting hits. Larger values can cause streaks, lower values can cause holes.
    float               roughnessThreshold;                     ///< Regions with a roughness value greater than this threshold won't spawn rays.
    float               varianceThreshold;                      ///< Luminance differences between history results will trigger an additional ray if they are greater than this threshold value.
    uint32_t            maxTraversalIntersections;              ///< Caps the maximum number of lookups that are performed from the depth buffer hierarchy. Most rays should terminate after approximately 20 lookups.
    uint32_t            minTraversalOccupancy;                  ///< Exit the core loop early if less than this number of threads are running.
    uint32_t            mostDetailedMip;                        ///< The most detailed MIP map level in the depth hierarchy. Perfect mirrors always use 0 as the most detailed level.
    uint32_t            samplesPerQuad;                         ///< The minimum number of rays per quad. Variance guided tracing can increase this up to a maximum of 4.
    uint32_t            temporalVarianceGuidedTracingEnabled;   ///< A boolean controlling whether a ray should be spawned on pixels where a temporal variance is detected or not.
} FfxSssrDispatchDescription;

/// A structure encapsulating the FidelityFX Stochastic Screen Space Reflections context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by SSSR.
///
/// The <c><i>FfxSssrContext</i></c> object should have a lifetime matching
/// your use of SSSR. Before destroying the SSSR context care should be taken
/// to ensure the GPU is not accessing the resources created or used by SSSR.
/// It is therefore recommended that the GPU is idle before destroying the
/// SSSR context.
///
/// @ingroup ffxSssr
typedef struct FfxSssrContext
{
    uint32_t data[FFX_SSSR_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxSssrContext;

/// Create a FidelityFX Stochastic Screen Space Reflections context from the parameters
/// programmed to the <c><i>FfxSssrCreateParams</i></c> structure.
///
/// The context structure is the main object used to interact with the SSSR
/// API, and is responsible for the management of the internal resources used
/// by the SSSR algorithm. When this API is called, multiple calls will be
/// made via the pointers contained in the <c><i>callbacks</i></c> structure.
/// These callbacks will attempt to retreive the device capabilities, and
/// create the internal resources, and pipelines required by SSSR's
/// frame-to-frame function. Depending on the precise configuration used when
/// creating the <c><i>FfxSssrContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The flags included in the <c><i>flags</i></c> field of
/// <c><i>FfxSssrContext</i></c> how match the configuration of your
/// application as well as the intended use of SSSR. It is important that these
/// flags are set correctly (as well as a correct programmed
/// <c><i>FfxSssrDispatchDescription</i></c>) to ensure correct operation. It is
/// recommended to consult the overview documentation for further details on
/// how SSSR should be integerated into an application.
///
/// When the <c><i>FfxSssrContext</i></c> is created, you should use the
/// <c><i>ffxSssrContextDispatch</i></c> function each frame where SSSR
/// algorithm should be applied. See the documentation of
/// <c><i>ffxSssrContextDispatch</i></c> for more details.
///
/// The <c><i>FfxSssrContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or SSSR upscaling is
/// disabled by a user. To destroy the SSSR context you should call
/// <c><i>ffxSssrContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxSssrContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxSssrContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxSssrContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxSssr
FFX_API FfxErrorCode ffxSssrContextCreate(FfxSssrContext* context, const FfxSssrContextDescription* contextDescription);

/// Dispatch the various passes that constitute the FidelityFX Stochastic Screen Space Reflections.
///
/// SSSR is a composite effect, meaning that it is compromised of multiple
/// constituent passes (implemented as one or more clears, copies and compute
/// dispatches). The <c><i>ffxSssrContextDispatch</i></c> function is the
/// function which (via the use of the functions contained in the
/// <c><i>callbacks</i></c> field of the <c><i>FfxSssrContext</i></c>
/// structure) utlimately generates the sequence of graphics API calls required
/// each frame.
///
/// As with the creation of the <c><i>FfxSssrContext</i></c> correctly
/// programming the <c><i>FfxSssrDispatchDescription</i></c> is key to ensuring
/// the correct operation of SSSR.
///
/// @param [in] pContext                 A pointer to a <c><i>FfxSssrContext</i></c> structure.
/// @param [in] pDispatchDescription     A pointer to a <c><i>FfxSssrDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE               The operation failed because the device inside the context was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxSssr
FFX_API FfxErrorCode ffxSssrContextDispatch(FfxSssrContext* context, const FfxSssrDispatchDescription* dispatchDescription);


/// Destroy the FidelityFX Stochastic Screen Space Reflections context.
///
/// @param [out] pContext               A pointer to a <c><i>FfxSssrContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxSssr
FFX_API FfxErrorCode ffxSssrContextDestroy(FfxSssrContext* context);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxSssr
FFX_API FfxVersionNumber ffxSssrGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
