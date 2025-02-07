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

// Include the interface for the backend of the Brixelizer GI API.
#include <FidelityFX/host/ffx_interface.h>
#include <FidelityFX/host/ffx_brixelizer_raw.h>

/// @defgroup ffxBrixgi FidelityFX Brixelizer GI
/// FidelityFX Brixelizer GI runtime library
///
/// @ingroup SDKComponents


/// FidelityFX Brixelizer GI major version.
///
/// @ingroup ffxBrixgi
#define FFX_BRIXELIZER_GI_VERSION_MAJOR (1)

/// FidelityFX Brixelizer GI minor version.
///
/// @ingroup ffxBrixgi
#define FFX_BRIXELIZER_GI_VERSION_MINOR (0)

/// FidelityFX Brixelizer GI patch version.
///
/// @ingroup ffxBrixgi
#define FFX_BRIXELIZER_GI_VERSION_PATCH (0)

/// The size of the context specified in 32bit values.
///
/// @ingroup ffxBrixgi
#define FFX_BRIXELIZER_GI_CONTEXT_SIZE (210000)

/// FidelityFX Brixelizer GI context count
/// 
/// Defines the number of internal effect contexts required by Brixelizer
///
/// @ingroup ffxBrixgi
#define FFX_BRIXELIZER_GI_CONTEXT_COUNT 1

#ifdef __cplusplus
extern "C" {
#endif

/// A structure encapsulating the FidelityFX Brixelizer GI context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by Brixelizer GI.
///
/// The <c><i>FfxBrixelizerGIContext</i></c> object should have a lifetime matching
/// your use of Brixelizer GI. Before destroying the Brixelizer GI context care
/// should be taken to ensure the GPU is not accessing the resources created
/// or used by Brixelizer GI. It is therefore recommended that the GPU is idle
/// before destroying the Brixelizer GI context.
///
/// @ingroup ffxBrixgi
typedef struct FfxBrixelizerGIContext
{
    uint32_t data[FFX_BRIXELIZER_GI_CONTEXT_SIZE];
} FfxBrixelizerGIContext;

/// An enumeration of flag bits used when creating an <c><i>FfxBrixelizerGIContext</i></c>.
/// See <c><i>FfxBrixelizerGIContextDescription</i></c>.
///
/// @ingroup ffxBrixgi
typedef enum FfxBrixelizerGIFlags 
{
    FFX_BRIXELIZER_GI_FLAG_DEPTH_INVERTED   = (1 << 0),  ///< Indicates input resources were generated with inverted depth.
    FFX_BRIXELIZER_GI_FLAG_DISABLE_SPECULAR = (1 << 1),  ///< Disable specular GI.
    FFX_BRIXELIZER_GI_FLAG_DISABLE_DENOISER = (1 << 2),  ///< Disable denoising. Only allowed at native resolution.
} FfxBrixelizerGIFlags;

/// An enumeration of the quality modes supported by FidelityFX Brixelizer GI.
///
/// @ingroup ffxBrixgi
typedef enum FfxBrixelizerGIInternalResolution
{
    FFX_BRIXELIZER_GI_INTERNAL_RESOLUTION_NATIVE,     ///< Output GI at native resolution.
    FFX_BRIXELIZER_GI_INTERNAL_RESOLUTION_75_PERCENT, ///< Output GI at 75% of native resolution.
    FFX_BRIXELIZER_GI_INTERNAL_RESOLUTION_50_PERCENT, ///< Output GI at 50% of native resolution.
    FFX_BRIXELIZER_GI_INTERNAL_RESOLUTION_25_PERCENT, ///< Output GI at 25% of native resolution.
} FfxBrixelizerGIInternalResolution;

/// A structure encapsulating the parameters used for creating an
/// <c><i>FfxBrixelizerGIContext</i></c>.
///
/// @ingroup ffxBrixgi
typedef struct FfxBrixelizerGIContextDescription
{
    FfxBrixelizerGIFlags              flags;              ///< A bit field representings various options.
    FfxBrixelizerGIInternalResolution internalResolution; ///< The scale at which Brixelizer GI will output GI at internally. The output will be internally upscaled to the specified displaySize.
    FfxDimensions2D                   displaySize;        ///< The size of the presentation resolution targeted by the upscaling process.
    FfxInterface                      backendInterface;   ///< An implementation of the FidelityFX backend for use with Brixelizer.
} FfxBrixelizerGIContextDescription;

/// A structure encapsulating the parameters used for computing a dispatch by the
/// Brixelizer GI context.
///
/// @ingroup ffxBrixgi
typedef struct FfxBrixelizerGIDispatchDescription
{
    FfxFloat32x4x4 view;                    ///< The view matrix for the scene in row major order.
    FfxFloat32x4x4 projection;              ///< The projection matrix for the scene in row major order.
    FfxFloat32x4x4 prevView;                ///< The view matrix for the previous frame of the scene in row major order.
    FfxFloat32x4x4 prevProjection;          ///< The projection matrix for the scene in row major order.

    FfxFloat32x3   cameraPosition;          ///< A 3-dimensional vector representing the position of the camera.
    FfxUInt32      startCascade;            ///< The index of the start cascade for use with ray marching with Brixelizer.
    FfxUInt32      endCascade;              ///< The index of the end cascade for use with ray marching with Brixelizer.
    FfxFloat32     rayPushoff;              ///< The distance from a surface along the normal vector to offset the diffuse ray origin.
    FfxFloat32     sdfSolveEps;             ///< The epsilon value for ray marching to be used with Brixelizer for diffuse rays.
    FfxFloat32     specularRayPushoff;      ///< The distance from a surface along the normal vector to offset the specular ray origin.
    FfxFloat32     specularSDFSolveEps;     ///< The epsilon value for ray marching to be used with Brixelizer for specular rays.
    FfxFloat32     tMin;                    ///< The TMin value for use with Brixelizer.
    FfxFloat32     tMax;                    ///< The TMax value for use with Brixelizer.

    FfxResource environmentMap;             ///< The environment map.
    FfxResource prevLitOutput;              ///< The lit output from the previous frame.
    FfxResource depth;                      ///< The input depth buffer.
    FfxResource historyDepth;               ///< The previous frame input depth buffer.
    FfxResource normal;                     ///< The input normal buffer.
    FfxResource historyNormal;              ///< The previous frame input normal buffer.
    FfxResource roughness;                  ///< The resource containing roughness information.
    FfxResource motionVectors;              ///< The input motion vectors texture.
    FfxResource noiseTexture;               ///< The input blue noise texture.
    
    FfxFloat32       normalsUnpackMul;       ///< A multiply factor to transform the normal to the space expected by Brixelizer GI.
    FfxFloat32       normalsUnpackAdd;       ///< An offset to transform the normal to the space expected by Brixelizer GI.
    FfxBoolean       isRoughnessPerceptual;  ///< A boolean to describe the space used to store roughness in the materialParameters texture. If false, we assume roughness squared was stored in the Gbuffer.
    FfxUInt32        roughnessChannel;       ///< The channel to read the roughness from the roughness texture
    FfxFloat32       roughnessThreshold;     ///< Regions with a roughness value greater than this threshold won't spawn specular rays.
    FfxFloat32       environmentMapIntensity;///< The value to scale the contribution from the environment map.
    FfxFloatCoords2D motionVectorScale;      ///< The scale factor to apply to motion vectors.

    FfxResource sdfAtlas;                   ///< The SDF Atlas resource used by Brixelizer.
    FfxResource bricksAABBs;                ///< The brick AABBs resource used by Brixelizer.
    FfxResource cascadeAABBTrees[24];       ///< The cascade AABB tree resources used by Brixelizer.
    FfxResource cascadeBrickMaps[24];       ///< The cascade brick map resources used by Brixelizer.

    FfxResource outputDiffuseGI;            ///< A texture to write the output diffuse GI calculated by Brixelizer GI.
    FfxResource outputSpecularGI;           ///< A texture to write the output specular GI calculated by Brixelizer GI.

    FfxBrixelizerRawContext *brixelizerContext; ///< A pointer to the Brixelizer context for use with Brixelizer GI.
} FfxBrixelizerGIDispatchDescription;

/// An enumeration of which output mode to be used by Brixelizer GI debug visualization.
/// See <c><i>FfxBrixelizerGIDebugDescription</i></c>.
///
/// @ingroup ffxBrixgi
typedef enum FfxBrixelizerGIDebugMode
{
    FFX_BRIXELIZER_GI_DEBUG_MODE_RADIANCE_CACHE,   ///< Draw the radiance cache.
    FFX_BRIXELIZER_GI_DEBUG_MODE_IRRADIANCE_CACHE, ///< Draw the irradiance cache.
} FfxBrixelizerGIDebugMode;

/// A structure encapsulating the parameters for drawing a debug visualization.
///
/// @ingroup ffxBrixgi
typedef struct FfxBrixelizerGIDebugDescription
{
    FfxFloat32x4x4           view;              ///< The view matrix for the scene in row major order.
    FfxFloat32x4x4           projection;        ///< The projection matrix for the scene in row major order.
    FfxUInt32                startCascade;      ///< The index of the start cascade for use with ray marching with Brixelizer.
    FfxUInt32                endCascade;        ///< The index of the end cascade for use with ray marching with Brixelizer.
    FfxUInt32                outputSize[2];     ///< The dimensions of the output texture.
    FfxBrixelizerGIDebugMode debugMode;         ///< The mode for the debug visualization. See <c><i>FfxBrixelizerGIDebugMode</i></c>.
    FfxFloat32               normalsUnpackMul;  ///< A multiply factor to transform the normal to the space expected by Brixelizer GI.
    FfxFloat32               normalsUnpackAdd;  ///< An offset to transform the normal to the space expected by Brixelizer GI.

    FfxResource depth;                   ///< The input depth buffer.
    FfxResource normal;                  ///< The input normal buffer.

    FfxResource sdfAtlas;                ///< The SDF Atlas resource used by Brixelizer.
    FfxResource bricksAABBs;             ///< The brick AABBs resource used by Brixelizer.
    FfxResource cascadeAABBTrees[24];    ///< The cascade AABB tree resources used by Brixelizer.
    FfxResource cascadeBrickMaps[24];    ///< The cascade brick map resources used by Brixelizer.

    FfxResource outputDebug;             ///< The output texture for the debug visualization.

    FfxBrixelizerRawContext* brixelizerContext; ///< A pointer to the Brixelizer context for use with Brixelizer GI.
} FfxBrixelizerGIDebugDescription;

/// An enumeration of all the passes which constitute the Brixelizer algorithm.
///
/// Brixelizer GI is implemented as a composite of several compute passes each
/// computing a key part of the final result. Each call to the
/// <c><i>FfxBrixelizerScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single pass included in <c><i>FfxBrixelizerPass</i></c>. For a
/// more comprehensive description of each pass, please refer to the Brixelizer
/// reference documentation.
///
/// @ingroup ffxBrixgi
typedef enum FfxBrixelizerGIPass
{
    FFX_BRIXELIZER_GI_PASS_BLUR_X,
    FFX_BRIXELIZER_GI_PASS_BLUR_Y,
    FFX_BRIXELIZER_GI_PASS_CLEAR_CACHE,
    FFX_BRIXELIZER_GI_PASS_EMIT_IRRADIANCE_CACHE,
    FFX_BRIXELIZER_GI_PASS_EMIT_PRIMARY_RAY_RADIANCE,
    FFX_BRIXELIZER_GI_PASS_FILL_SCREEN_PROBES,
    FFX_BRIXELIZER_GI_PASS_INTERPOLATE_SCREEN_PROBES,
    FFX_BRIXELIZER_GI_PASS_PREPARE_CLEAR_CACHE,
    FFX_BRIXELIZER_GI_PASS_PROJECT_SCREEN_PROBES,
    FFX_BRIXELIZER_GI_PASS_PROPAGATE_SH,
    FFX_BRIXELIZER_GI_PASS_REPROJECT_GI,
    FFX_BRIXELIZER_GI_PASS_REPROJECT_SCREEN_PROBES,
    FFX_BRIXELIZER_GI_PASS_SPAWN_SCREEN_PROBES,
    FFX_BRIXELIZER_GI_PASS_SPECULAR_PRE_TRACE,
    FFX_BRIXELIZER_GI_PASS_SPECULAR_TRACE,
    FFX_BRIXELIZER_GI_PASS_DEBUG_VISUALIZATION,
    FFX_BRIXELIZER_GI_PASS_GENERATE_DISOCCLUSION_MASK,
    FFX_BRIXELIZER_GI_PASS_DOWNSAMPLE,
    FFX_BRIXELIZER_GI_PASS_UPSAMPLE,

    FFX_BRIXELIZER_GI_PASS_COUNT  ///< The number of passes performed by Brixelizer GI.
} FfxBrixelizerGIPass;

/// Get the size in bytes needed for an <c><i>FfxBrixelizerGIContext</i></c> struct.
/// Note that this function is provided for consistency, and the size of the
/// <c><i>FfxBrixelizerGIContext</i></c> is a known compile time value which can be
/// obtained using <c><i>sizeof(FfxBrixelizerGIContext)</i></c>.
///
/// @return  The size in bytes of an <c><i>FfxBrixelizerGIContext</i></c> struct.
///
/// @ingroup ffxBrixgi
inline size_t ffxBrixelizerGIGetContextSize()
{
    return sizeof(FfxBrixelizerGIContext);
}

/// Create a FidelityFX Brixelizer GI context from the parameters
/// specified to the <c><i>FfxBrixelizerGIContextDescription</i></c> struct.
///
/// The context structure is the main object used to interact with the Brixelizer GI API,
/// and is responsible for the management of the internal resources used by the
/// Brixelizer GI algorithm. When this API is called, multiple calls will be made via
/// the pointers contained in the <b><i>backendInterface</i></b> structure. This
/// backend will attempt to retrieve the device capabilities, and create the internal
/// resources, and pipelines required by Brixelizer GI.
///
/// Depending on the parameters passed in via the <b><i>pContextDescription</b></i> a
/// different set of resources and pipelines may be requested by the callback functions.
///
/// The <c><i>FfxBrixelizerGIContext</i></c> should be destroyed when use of it is completed.
/// To destroy the context you should call <c><i>ffxBrixelizerGIContextDestroy</i></c>.
///
/// @param [out] pContext               A pointer to a <c><i>FfxBrixelizerGIContext</i></c> to populate.
/// @param [in]  pContextDescription    A pointer to a <c><i>FfxBrixelizerGIContextDescription</i></c> specifying the parameters for context creation.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because either <c><i>pContext</i></c> or <c><i>pContextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE     The operation failed because <c><i>pContextDescription->backendInterface</i></c> was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR        The operation failed because of an error from the backend.
///
/// @ingroup ffxBrixgi
FFX_API FfxErrorCode ffxBrixelizerGIContextCreate(FfxBrixelizerGIContext* pContext, const FfxBrixelizerGIContextDescription* pContextDescription);

/// Destroy the FidelityFX Brixelizer GI context.
///
/// @param [out] pContext        A pointer to a <c><i>FfxBrixelizerGIContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER   The <c><i>pContext</i></c> pointer provided was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixgi
FFX_API FfxErrorCode ffxBrixelizerGIContextDestroy(FfxBrixelizerGIContext* pContext);

/// Perform an update of Brixelizer GI, recording GPU commands to a command list.
///
/// @param [inout] context              An <c><i>FfxBrixelizerGIContext</i></c> containing the Brixelizer GI context.
/// @param [in]    pDispatchDescription A pointer to a <c><i>FfxBrixelizerGIDispatchDescription</i></c> describing the dispatch to compute.
/// @param [inout] pCommandList         An <c><i>FfxCommandList</i></c> to write GPU commands to.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixgi
FFX_API FfxErrorCode ffxBrixelizerGIContextDispatch(FfxBrixelizerGIContext* pContext, const FfxBrixelizerGIDispatchDescription* pDispatchDescription, FfxCommandList pCommandList);

/// Make a debug visualization from the <c><i>FfxBrixelizerGIContext</i></c>.
///
/// @param [inout] context              An <c><i>FfxBrixelizerGIContext</i></c> containing the Brixelizer GI context.
/// @param [in]    pDebugDescription    A pointer to a <c><i>FfxBrixelizerGIDebugDescription</i></c> describing the debug visualization to draw.
/// @param [inout] pCommandList         An <c><i>FfxCommandList</i></c> to write GPU commands to.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixgi
FFX_API FfxErrorCode ffxBrixelizerGIContextDebugVisualization(FfxBrixelizerGIContext* pContext, const FfxBrixelizerGIDebugDescription* pDebugDescription, FfxCommandList pCommandList);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxBrixgi
FFX_API FfxVersionNumber ffxBrixelizerGIGetEffectVersion();

#ifdef __cplusplus
}
#endif
