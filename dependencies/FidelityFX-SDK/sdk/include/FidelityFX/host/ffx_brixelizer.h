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

#include <FidelityFX/host/ffx_brixelizer_raw.h>

/// The size of the context specified in 32bit values.
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_CONTEXT_SIZE            (5938838)

/// The size of the update description specified in 32bit values.
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_UPDATE_DESCRIPTION_SIZE 2099376

#ifdef __cplusplus
extern "C" {
#endif

/// A structure encapsulating the FidelityFX Brixelizer context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by Brixelizer.
///
/// The <c><i>FfxBrixelizerContext</i></c> object should have a lifetime matching
/// your use of Brixelizer. Before destroying the Brixelizer context care
/// should be taken to ensure the GPU is not accessing the resources created
/// or used by Brixelizer. It is therefore recommended that the GPU is idle
/// before destroying the Brixelizer context.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerContext {
    uint32_t data[FFX_BRIXELIZER_CONTEXT_SIZE];
} FfxBrixelizerContext;

/// A structure representing an axis aligned bounding box for use with Brixelizer.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerAABB {
    float min[3];    ///< The minimum bounds of the AABB.
    float max[3];    ///< The maximum bounds of the AABB.
} FfxBrixelizerAABB;

/// Flags used for cascade creation. A cascade may be specified
/// as having static geometry, dynamic geometry, or both by combining these flags.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerCascadeFlag {
    FFX_BRIXELIZER_CASCADE_STATIC  = (1 << 0),
    FFX_BRIXELIZER_CASCADE_DYNAMIC = (1 << 1),
} FfxBrixelizerCascadeFlag;

/// A structure encapsulating the parameters for cascade creation.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerCascadeDescription {
    FfxBrixelizerCascadeFlag flags;        ///< Flags for cascade creation. See <c><i>FfxBrixelizerCascadeFlag</i></c>.
    float                    voxelSize;    ///< The edge size of voxels in world space for the cascade.
} FfxBrixelizerCascadeDescription;

/// A structure encapsulating the parameters for creating a Brixelizer context.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerContextDescription {
    float                           sdfCenter[3];                                 ///< The point in world space around which to center the cascades.
    uint32_t                        numCascades;                                  ///< The number of cascades managed by the Brixelizer context.
    FfxBrixelizerContextFlags       flags;                                        ///< A combination of <c><i>FfxBrixelizerContextFlags</i></c> specifying options for the context.
    FfxBrixelizerCascadeDescription cascadeDescs[FFX_BRIXELIZER_MAX_CASCADES];    ///< Parameters describing each of the cascades, see <c><i>FfxBrixelizerCascadeDescription</i></c>.
    FfxInterface                    backendInterface;                             ///< An implementation of the FidelityFX backend for use with Brixelizer.
} FfxBrixelizerContextDescription;

/// Flags used for setting which AABBs to draw in a debug visualization of Brixelizer
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerPopulateDebugAABBsFlags {
    FFX_BRIXELIZER_POPULATE_AABBS_NONE               = 0,                                                                                    ///< Draw no AABBs.
    FFX_BRIXELIZER_POPULATE_AABBS_STATIC_INSTANCES   = 1 << 0,                                                                               ///< Draw AABBs for all static instances.
    FFX_BRIXELIZER_POPULATE_AABBS_DYNAMIC_INSTANCES  = 1 << 1,                                                                               ///< Draw AABBs for all dynamic instances.
    FFX_BRIXELIZER_POPULATE_AABBS_INSTANCES          = FFX_BRIXELIZER_POPULATE_AABBS_STATIC_INSTANCES | FFX_BRIXELIZER_POPULATE_AABBS_DYNAMIC_INSTANCES,     ///< Draw AABBs for all instances.
    FFX_BRIXELIZER_POPULATE_AABBS_CASCADE_AABBS      = 1 << 2,                                                                               ///< Draw AABBs for all cascades.
} FfxBrixelizerPopulateDebugAABBsFlags;

/// A structure containing the statistics for a Brixelizer context readable after an update of the Brixelizer API.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerContextStats {
    uint32_t brickAllocationsAttempted;      ///< Total number of brick allocations attempted this frame.
    uint32_t brickAllocationsSucceeded;      ///< Total number of brick allocations succeeded this frame.
    uint32_t bricksCleared;                  ///< Total number of bricks cleared in SDF atlas at the beginning of this frame.
    uint32_t bricksMerged;                   ///< Total number of bricks merged this frame.
    uint32_t freeBricks;                     ///< The number of free bricks in the Brixelizer context.
} FfxBrixelizerContextStats;

/// A structure containing the statistics for a Brixelizer cascade readable after an update of the Brixelizer API.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerCascadeStats {
    uint32_t trianglesAllocated;       ///< The number of triangle allocations that were attempted to the cascade in a given frame.
    uint32_t referencesAllocated;      ///< The number of reference allocations that were attempted to the cascade in a given frame.
    uint32_t bricksAllocated;          ///< The number of brick allocations that were attempted to the cascade in a given frame.
} FfxBrixelizerCascadeStats;

/// A structure containing the statistics readable after an update of the Brixelizer API.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerStats {
    uint32_t                  cascadeIndex;         ///< The index of the cascade that the statisticss have been collected for.
    FfxBrixelizerCascadeStats staticCascadeStats;   ///< The statistics for the static cascade.
    FfxBrixelizerCascadeStats dynamicCascadeStats;  ///< The statistics for the dynamic cascade.
    FfxBrixelizerContextStats contextStats;         ///< The statistics for the Brixelizer context.
} FfxBrixelizerStats;

/// A structure encapsulating the parameters used for computing an update by the
/// Brixelizer context.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerUpdateDescription {
    FfxBrixelizerResources                      resources;                    ///< Structure containing all resources to be used by the Brixelizer context.
    uint32_t                                    frameIndex;                   ///< The index of the current frame.
    float                                       sdfCenter[3];                 ///< The center of the cascades.
    FfxBrixelizerPopulateDebugAABBsFlags        populateDebugAABBsFlags;      ///< Flags determining which AABBs to draw in a debug visualization. See <c><i>FfxBrixelizerPopulateDebugAABBsFlag</i></c>.
    FfxBrixelizerDebugVisualizationDescription *debugVisualizationDesc;       ///< An optional debug visualization description. If this parameter is set to <c><i>NULL</i></c> no debug visualization is drawn.
    uint32_t                                    maxReferences;                ///< The maximum number of triangle voxel references to be stored in the update.
    uint32_t                                    triangleSwapSize;             ///< The size of the swap space available to be used for storing triangles in the update.
    uint32_t                                    maxBricksPerBake;             ///< The maximum number of bricks to be updated.
    size_t                                     *outScratchBufferSize;         ///< An optional pointer to a <c><i>size_t</i></c> to receive the size of the GPU scratch buffer needed to process the update.
    FfxBrixelizerStats                         *outStats;                     ///< An optional pointer to an <c><i>FfxBrixelizerStats</i></c> struct to receive statistics for the update. Note, stats read back after a call to update do not correspond to the same frame that the stats were requested, as reading of stats requires readback from GPU buffers which is performed with a delay.
} FfxBrixelizerUpdateDescription;

/// A structure generated by Brixelizer from an <c><i>FfxBrixelizerUpdateDescription</i></c> structure
/// used for storing parameters necessary for an update with the underlying raw Brixelizer API.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerBakedUpdateDescription {
    uint32_t data[FFX_BRIXELIZER_UPDATE_DESCRIPTION_SIZE];
} FfxBrixelizerBakedUpdateDescription;

/// Flags used for specifying instance properties.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerInstanceFlags {
    FFX_BRIXELIZER_INSTANCE_FLAG_NONE    = 0,       ///< No instance flags set.
    FFX_BRIXELIZER_INSTANCE_FLAG_DYNAMIC = 1 << 0,  ///< This flag is set for any instance which should be added to the dynamic cascade. Indicates that this instance will be resubmitted every frame.
} FfxBrixelizerInstanceFlags;

/// A structure encapsulating the parameters necessary to create an instance with Brixelizer.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerInstanceDescription {
    uint32_t                    maxCascade;           ///< The index of the highest cascade this instance will be submitted to. This helps avoid submitting many small objects to least detailed cascades.
    FfxBrixelizerAABB           aabb;                 ///< An AABB surrounding the instance.
    FfxFloat32x3x4              transform;            ///< A transform of the instance into world space. The transform is in row major order.

    FfxIndexFormat              indexFormat;          ///< The format of the index buffer. Accepted formats are FFX_INDEX_UINT16 or FFX_INDEX_UINT32.
    uint32_t                    indexBuffer;          ///< The index of the index buffer set with ffxBrixelizerContextSetBuffer.
    uint32_t                    indexBufferOffset;    ///< An offset into the index buffer.
    uint32_t                    triangleCount;        ///< The count of triangles in the index buffer.

    uint32_t                    vertexBuffer;         ///< The index of the vertex buffer set with ffxBrixelizerContextSetBuffer.
    uint32_t                    vertexStride;         ///< The stride of the vertex buffer in bytes.
    uint32_t                    vertexBufferOffset;   ///< An offset into the vertex buffer.
    uint32_t                    vertexCount;          ///< The count of vertices in the vertex buffer.
    FfxSurfaceFormat            vertexFormat;         ///< The format of vertices in the vertex buffer. Accepted values are FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT and FFX_SURFACE_FORMAT_R32G32B32_FLOAT.

    FfxBrixelizerInstanceFlags  flags;                ///< Flags specifying properties of the instance. See <c><i>FfxBrixelizerInstanceFlags</i></c>.

    FfxBrixelizerInstanceID    *outInstanceID;        ///< A pointer to an <c><i>FfxBrixelizerInstanceID</i></c> storing the ID of the created instance.
} FfxBrixelizerInstanceDescription;

/// Get the size in bytes needed for an <c><i>FfxBrixelizerContext</i></c> struct.
/// Note that this function is provided for consistency, and the size of the
/// <c><i>FfxBrixelizerContext</i></c> is a known compile time value which can be
/// obtained using <c><i>sizeof(FfxBrixelizerContext)</i></c>.
///
/// @return  The size in bytes of an <c><i>FfxBrixelizerContext</i></c> struct.
///
/// @ingroup ffxBrixelizer
inline size_t ffxBrixelizerGetContextSize()
{
    return sizeof(FfxBrixelizerContext);
}

/// Create a FidelityFX Brixelizer context from the parameters
/// specified to the <c><i>FfxBrixelizerContextDesc</i></c> struct.
///
/// The context structure is the main object used to interact with the Brixelizer
/// API, and is responsible for the management of the internal resources used by the
/// Brixelizer algorithm. When this API is called, multiple calls will be made via
/// the pointers contained in the <b><i>backendInterface</i></b> structure. This
/// backend will attempt to retrieve the device capabilities, and create the internal
/// resources, and pipelines required by Brixelizer.
///
/// Depending on the parameters passed in via the <b><i>contextDescription</b></i> a
/// different set of resources and pipelines may be requested by the callback functions.
///
/// The <c><i>FfxBrixelizerContext</i></c> should be destroyed when use of it is completed.
/// To destroy the context you should call <c><i>ffxBrixelizerContextDestroy</i></c>.
///
/// @param [in]  desc             An <c><i>FfxBrixelizerContextDescription</i></c> structure with the parameters for context creation.
/// @param [out] outContext       An <c><i>FfxBrixelizerContext</i></c> structure for receiving the initialized Brixelizer context.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerContextCreate(const FfxBrixelizerContextDescription* desc, FfxBrixelizerContext* outContext);

/// Delete the Brixelizer context associated with the <c><i>FfxBrixelizerContext</i></c> struct.
///
/// @param [inout] context        An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerContextDestroy(FfxBrixelizerContext* context);

/// Fill in an <c><i>FfxBrixelizerContextInfo</i></c> struct for necessary for updating a constant buffer for use
/// by Brixelizer when ray marching.
///
/// @param [inout] context        An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [out]   contextInfo    An <c><i>FfxBrixelizerContextInfo</i></c> struct to be filled.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerGetContextInfo(FfxBrixelizerContext* context, FfxBrixelizerContextInfo* contextInfo);

/// Build an <c><i>FfxBrixelizerBakedUpdateDescription</i></c> struct from an <c><i>FfxBrixelizerUpdateDescription</i></c> struct
/// for use in doing a Brixelizer update.
///
/// @param [inout] context        An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [in]    desc           An <c><i>FfxBrixelizerUpdateDescription</i></c> struct containing the parameters for the update.
/// @param [out]   outDesc        An <c><i>FfxBrixelizerBakedUpdateDescription</i></c> struct to be filled in.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerBakeUpdate(FfxBrixelizerContext* context, const FfxBrixelizerUpdateDescription* desc, FfxBrixelizerBakedUpdateDescription* outDesc);

/// Perform an update of Brixelizer, recording GPU commands to a command list.
///
/// @param [inout] context        An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [in]    desc           An <c><i>FfxBrixelizerBakedUpdateDescription</i></c> describing the update to compute.
/// @param [out]   scratchBuffer  An <c><i>FfxResource</i></c> to be used as scratch space by the update.
/// @param [out]   commandList    An <c><i>FfxCommandList</i></c> to write GPU commands to.
///
/// @retval
/// FFX_OK                        The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerUpdate(FfxBrixelizerContext* context, FfxBrixelizerBakedUpdateDescription* desc, FfxResource scratchBuffer, FfxCommandList commandList);

/// Register a vertex or index buffer to use with Brixelizer.
///
/// @param [inout] context      An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [in]    buffer       An <c><i>FfxResource</i></c> of the vertex or index buffer.
/// @param [out]   index        The index of the registered buffer.
///
/// @retval
/// FFX_OK                      The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRegisterBuffers(FfxBrixelizerContext* context, const FfxBrixelizerBufferDescription* bufferDescs, uint32_t numBufferDescs);

/// Unregister a previously registered vertex or index buffer.
///
/// @param [inout] context      An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [in]    index        The index of the buffer to unregister.
///
/// @retval
/// FFX_OK                      The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerUnregisterBuffers(FfxBrixelizerContext* context, const uint32_t* indices, uint32_t numIndices);

/// Create a static instance for a Brixelizer context.
///
/// @param [inout] context      An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [in]    descs        An array of <c><i>FfxBrixelizerInstanceDescription</i></c> structs with the parameters for instance creation.
/// @param [in]    numDescs     The number of entries in the array passed in by <c><i>descs</i></c>.
///
/// @retval
/// FFX_OK                      The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerCreateInstances(FfxBrixelizerContext* context, const FfxBrixelizerInstanceDescription* descs, uint32_t numDescs);

/// Delete a static instance from a Brixelizer context.
///
/// @param [inout] context         An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [in]    instanceIDs     An array of <c><i>FfxBrixelizerInstanceID</i></c>s corresponding to instances to be destroyed.
/// @param [in]    numInstnaceIDs  The number of elements in the array passed in by <c><i>instanceIDs</i></c>.
///
/// @retval
/// FFX_OK                      The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerDeleteInstances(FfxBrixelizerContext* context, const FfxBrixelizerInstanceID* instanceIDs, uint32_t numInstanceIDs);

/// Get a pointer to the underlying Brixelizer raw context from a Brixelizer context.
///
/// @param [inout] context    An <c><i>FfxBrixelizerContext</i></c> containing the Brixelizer context.
/// @param [out]   outContext A <c><i>FfxBrixelizerRawContext</i></c> representing the underlying Brixelizer raw context.
///
/// @return
/// FFX_ERROR_INVALID_POINTER  The pointer given was invalid.
/// @return
/// FFX_OK                     The operation completed successfully.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerGetRawContext(FfxBrixelizerContext* context, FfxBrixelizerRawContext** outContext);

#ifdef __cplusplus
}
#endif
