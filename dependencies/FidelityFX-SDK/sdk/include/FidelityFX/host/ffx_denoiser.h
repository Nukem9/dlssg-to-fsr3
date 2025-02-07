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

// Include the interface for the backend of the Denoiser API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup FfxDenoiser FidelityFX Denoiser
/// FidelityFX Denoiser runtime library
///
/// @ingroup SDKComponents

/// FidelityFX Denoiser major version.
///
/// @ingroup FfxDenoiser
#define FFX_DENOISER_VERSION_MAJOR (1)

/// FidelityFX Denoiser minor version.
///
/// @ingroup FfxDenoiser
#define FFX_DENOISER_VERSION_MINOR (3)

/// FidelityFX Denoiser patch version.
///
/// @ingroup FfxDenoiser
#define FFX_DENOISER_VERSION_PATCH (0)

/// FidelityFX denoiser context count
///
/// Defines the number of internal effect contexts required by the denoiser
///
/// @ingroup FfxDenoiser
#define FFX_DENOISER_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup Denoiser
#define FFX_DENOISER_CONTEXT_SIZE (73098)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// An enumeration of the pass which constitutes the Denoiser algorithm.
///
/// @ingroup Denoiser
typedef enum FfxDenoiserPass
{
    FFX_DENOISER_PASS_PREPARE_SHADOW_MASK           = 0,
    FFX_DENOISER_PASS_SHADOWS_TILE_CLASSIFICATION   = 1,
    FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_0         = 2,
    FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_1         = 3,
    FFX_DENOISER_PASS_FILTER_SOFT_SHADOWS_2         = 4,
    FFX_DENOISER_PASS_REPROJECT_REFLECTIONS         = 5,    ///< A pass which reprojects and estimates the variance.
    FFX_DENOISER_PASS_PREFILTER_REFLECTIONS         = 6,    ///< A pass which spatially filters the reflections.
    FFX_DENOISER_PASS_RESOLVE_TEMPORAL_REFLECTIONS  = 7,    ///< A pass which temporally filters the reflections.

    FFX_DENOISER_PASS_COUNT                                 ///< The number of passes in Denoiser
} FfxDenoiserPass;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxDenoiserContext</i></c>. See <c><i>FfxDenoiserContextDescription</i></c>.
///
/// @ingroup FfxDenoiser
typedef enum FfxDenoiserInitializationFlagBits {

    FFX_DENOISER_SHADOWS                = (1 << 0),     ///< A bit indicating that the denoiser is used for denoising shadows
    FFX_DENOISER_REFLECTIONS            = (1 << 1),     ///< A bit indicating that the denoiser is used for denoising reflections
    FFX_DENOISER_ENABLE_DEPTH_INVERTED  = (1 << 2),     ///< A bit indicating that the input depth buffer data provided is inverted [1..0].
} FfxDenoiserInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX Denoiser
///
/// @ingroup FfxDenoiser
typedef struct FfxDenoiserContextDescription {

    uint32_t                    flags;                              ///< A collection of <c><i>FfxDenoiserInitializationFlagBits</i></c>
    FfxDimensions2D             windowSize;                         ///< The resolution that was used for rendering the input resource.
    FfxSurfaceFormat            normalsHistoryBufferFormat;         ///< The format used by the reflections denoiser to store the normals buffer history
    FfxInterface                backendInterface;                   ///< A set of pointers to the backend implementation for FidelityFX.
} FfxDenoiserContextDescription;

/// A structure encapsulating the parameters for dispatching
/// of FidelityFX Denoiser
///
/// @ingroup FfxDenoiser
typedef struct FfxDenoiserShadowsDispatchDescription {

    FfxCommandList commandList;        ///< The <c><i>FfxCommandList</i></c> to record Denoiser rendering commands into.
    FfxResource    hitMaskResults;     ///< A <c><i>FfxResource</i></c> containing the raytracing results where every pixel represents a 8x4 tile.
    FfxResource    depth;              ///< A <c><i>FfxResource</i></c> containing 32bit depth values for the current frame.
    FfxResource    velocity;           ///< A <c><i>FfxResource</i></c> containing 2-dimensional motion vectors.
    FfxResource    normal;             ///< A <c><i>FfxResource</i></c> containing the normals.
    FfxResource    shadowMaskOutput;   ///< A <c><i>FfxResource</i></c> which is used to store the fullscreen raytracing output.

    FfxFloat32x2   motionVectorScale;  ///< A multiply factor to transform the motion vectors to the space expected by the shadow denoiser.
    FfxFloat32     normalsUnpackMul;   ///< A multiply factor to transform the normal to the space expected by the shadow denoiser.
    FfxFloat32     normalsUnpackAdd;   ///< An offset to transform the normal to the space expected by the shadow denoiser.

    FfxFloat32x3   eye;                        ///< The camera position
    uint32_t       frameIndex;                 ///< The current frame index
    FfxFloat32     projectionInverse[16];      ///< The inverse of the camera projection matrix
    FfxFloat32     reprojectionMatrix[16];     ///< The result of multiplying the projection matrix of the current frame by the result of the multiplication between the camera previous's frame view matrix by the inverse of the view-projection matrix
    FfxFloat32     viewProjectionInverse[16];  ///< The inverse of the camera view-projection matrix

    FfxFloat32     depthSimilaritySigma;       ///< A constant factor used in the denoising filters, defaults to 1.0f

} FfxDenoiserShadowsDispatchDescription;

/// A structure encapsulating the parameters for dispatching
/// of FidelityFX Denoiser
///
/// @ingroup FfxDenoiser
typedef struct FfxDenoiserReflectionsDispatchDescription {

    FfxCommandList      commandList;                ///< The <c><i>FfxCommandList</i></c> to record Denoiser rendering commands into.
    FfxResource         depthHierarchy;             ///< A <c><i>FfxResource</i></c> containing the depth buffer with full mip maps for the current frame.
    FfxResource         motionVectors;              ///< A <c><i>FfxResource</i></c> containing the motion vectors buffer for the current frame.
    FfxResource         normal;                     ///< A <c><i>FfxResource</i></c> containing the normal buffer for the current frame.
    FfxResource         radianceA;                  ///< A <c><i>FfxResource</i></c> containing the ping-pong radiance buffers to filter.
    FfxResource         radianceB;                  ///< A <c><i>FfxResource</i></c> containing the ping-pong radiance buffers to filter.
    FfxResource         varianceA;                  ///< A <c><i>FfxResource</i></c> containing the ping-pong variance buffers used to filter and guide reflections.
    FfxResource         varianceB;                  ///< A <c><i>FfxResource</i></c> containing the ping-pong variance buffers used to filter and guide reflections.
    FfxResource         extractedRoughness;         ///< A <c><i>FfxResource</i></c> containing the roughness of the current frame.
    FfxResource         denoiserTileList;           ///< A <c><i>FfxResource</i></c> containing the tiles to be denoised.
    FfxResource         indirectArgumentsBuffer;    ///< A <c><i>FfxResource</i></c> containing the indirect arguments used by the indirect dispatch calls tha compose the denoiser.
    FfxResource         output;                     ///< A <c><i>FfxResource</i></c> to store the denoised reflections.
    FfxDimensions2D     renderSize;                 ///< The resolution that was used for rendering the input resources.
    FfxFloatCoords2D    motionVectorScale;          ///< The scale factor to apply to motion vectors.
    float               invProjection[16];          ///< An array containing the inverse of the projection matrix in column major layout.
    float               invView[16];                ///< An array containing the inverse of the view matrix in column major layout.
    float               prevViewProjection[16];     ///< An array containing the view projection matrix of the previous frame in column major layout.
    float               normalsUnpackMul;           ///< A multiply factor to transform the normal to the space expected by SSSR.
    float               normalsUnpackAdd;           ///< An offset to transform the normal to the space expected by SSSR.
    bool                isRoughnessPerceptual;      ///< A boolean to describe the space used to store roughness in the materialParameters texture. If false, we assume roughness squared was stored in the Gbuffer.
    uint32_t            roughnessChannel;           ///< The channel to read the roughness from the materialParameters texture
    float               temporalStabilityFactor;    ///< A boolean to describe the space used to store roughness in the materialParameters texture. If false, we assume roughness squared was stored in the Gbuffer.
    float               roughnessThreshold;         ///< Regions with a roughness value greater than this threshold won't spawn rays.
    uint32_t            frameIndex;                 ///< The index of the current frame.
    bool                reset;
} FfxDenoiserReflectionsDispatchDescription;


/// A structure encapsulating the FidelityFX denoiser context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by Denoiser.
///
/// The <c><i>FfxDenoiserContext</i></c> object should have a lifetime matching
/// your use of Denoiser. Before destroying the Denoiser context care should be taken
/// to ensure the GPU is not accessing the resources created or used by Denoiser.
/// It is therefore recommended that the GPU is idle before destroying the
/// Denoiser context.
///
/// @ingroup FfxDenoiser
typedef struct FfxDenoiserContext {

    uint32_t                    data[FFX_DENOISER_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxDenoiserContext;

/// Create a FidelityFX Denoiser context from the parameters
/// programmed to the <c><i>FfxDenoiserContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the
/// Denoiser API, and is responsible for the management of the internal resources
/// used by the Denoiser algorithm. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>backendInterface</i></c>
/// structure. This backend will attempt to retrieve the device capabilities,
/// and create the internal resources, and pipelines required by Denoiser to function.
/// Depending on the precise configuration used when
/// creating the <c><i>FfxDenoiserContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxDenoiserContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or shadow
/// denoising is disabled by a user. To destroy the Denoiser context you
/// should call <c><i>ffxDenoiserContextDestroy</i></c>.
///
/// @param [out] context                A pointer to a <c><i>FfxDenoiserContext</i></c> structure to populate.
/// @param [in]  contextDescription     A pointer to a <c><i>FfxDenoiserContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxDenoiserContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxDenoiser
FFX_API FfxErrorCode ffxDenoiserContextCreate(FfxDenoiserContext* pContext, const FfxDenoiserContextDescription* pContextDescription);

/// Dispatches work to the FidelityFX Denoiser context
///
/// @param [out] context                A pointer to a <c><i>FfxDenoiserContext</i></c> structure to populate.
/// @param [in]  dispatchDescription    A pointer to a <c><i>FfxDenoiserShadowsDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxDenoiser
FFX_API FfxErrorCode ffxDenoiserContextDispatchShadows(FfxDenoiserContext* context, const FfxDenoiserShadowsDispatchDescription* dispatchDescription);

/// Dispatches work to the FidelityFX Denoiser context
///
/// @param [out] context                A pointer to a <c><i>FfxDenoiserContext</i></c> structure to populate.
/// @param [in]  dispatchDescription    A pointer to a <c><i>FfxDenoiserReflectionsDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxDenoiser
FFX_API FfxErrorCode ffxDenoiserContextDispatchReflections(FfxDenoiserContext* context, const FfxDenoiserReflectionsDispatchDescription* dispatchDescription);

/// Destroy the FidelityFX Denoiser context.
///
/// @param [out] context                A pointer to a <c><i>FfxDenoiserContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup FfxDenoiser
FFX_API FfxErrorCode ffxDenoiserContextDestroy(FfxDenoiserContext* context);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup FfxDenoiser
FFX_API FfxVersionNumber ffxDenoiserGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
