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

/// @defgroup ffxBrixelizer FidelityFX Brixelizer
/// FidelityFX Brixelizer runtime library
///
/// @ingroup SDKComponents


// Include the interface for the backend of the Brixelizer API.
#include <FidelityFX/host/ffx_interface.h>
#include <FidelityFX/gpu/brixelizer/ffx_brixelizer_host_gpu_shared.h>

/// FidelityFX Brixelizer major version.
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_VERSION_MAJOR (1)

/// FidelityFX Brixelizer minor version.
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_VERSION_MINOR (0)

/// FidelityFX Brixelizer patch version.
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_VERSION_PATCH (0)

/// FidelityFX Brixelizer context count
/// 
/// Defines the number of internal effect contexts required by Brixelizer
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_CONTEXT_COUNT 1

/// The size of the raw context specified in 32bit values.
///
/// @ingroup ffxBrixelizer
#define FFX_BRIXELIZER_RAW_CONTEXT_SIZE (2924058)

#ifdef __cplusplus
extern "C" {
#endif

/// An enumeration of all the passes which constitute the Brixelizer algorithm.
///
/// Brixelizer is implemented as a composite of several compute passes each
/// computing a key part of the final result. Each call to the
/// <c><i>FfxBrixelizerScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single pass included in <c><i>FfxBrixelizerPass</i></c>. For a
/// more comprehensive description of each pass, please refer to the Brixelizer
/// reference documentation.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerPass
{
    
    FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_COUNTERS,
    FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_CLEAR_BRICKS,
    FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_CLEAR_BRICKS,
    FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_BRICK,
    FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_DIRTY_BRICKS,
    FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_EIKONAL_ARGS,
    FFX_BRIXELIZER_PASS_CONTEXT_EIKONAL,
    FFX_BRIXELIZER_PASS_CONTEXT_MERGE_CASCADES,
    FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_MERGE_BRICKS_ARGS,
    FFX_BRIXELIZER_PASS_CONTEXT_MERGE_BRICKS,
    FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BUILD_COUNTERS,
    FFX_BRIXELIZER_PASS_CASCADE_RESET_CASCADE,
    FFX_BRIXELIZER_PASS_CASCADE_SCROLL_CASCADE,
    FFX_BRIXELIZER_PASS_CASCADE_CLEAR_REF_COUNTERS,
    FFX_BRIXELIZER_PASS_CASCADE_CLEAR_JOB_COUNTER,
    FFX_BRIXELIZER_PASS_CASCADE_INVALIDATE_JOB_AREAS,
    FFX_BRIXELIZER_PASS_CASCADE_COARSE_CULLING,
    FFX_BRIXELIZER_PASS_CASCADE_SCAN_JOBS,
    FFX_BRIXELIZER_PASS_CASCADE_VOXELIZE,
    FFX_BRIXELIZER_PASS_CASCADE_SCAN_REFERENCES,
    FFX_BRIXELIZER_PASS_CASCADE_COMPACT_REFERENCES,
    FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BRICK_STORAGE,
    FFX_BRIXELIZER_PASS_CASCADE_EMIT_SDF,
    FFX_BRIXELIZER_PASS_CASCADE_COMPRESS_BRICK,
    FFX_BRIXELIZER_PASS_CASCADE_INITIALIZE_CASCADE,
    FFX_BRIXELIZER_PASS_CASCADE_MARK_UNINITIALIZED,
    FFX_BRIXELIZER_PASS_CASCADE_BUILD_TREE_AABB,
    FFX_BRIXELIZER_PASS_CASCADE_FREE_CASCADE,
    FFX_BRIXELIZER_PASS_DEBUG_VISUALIZATION,
    FFX_BRIXELIZER_PASS_DEBUG_INSTANCE_AABBS,
    FFX_BRIXELIZER_PASS_DEBUG_AABB_TREE,

    FFX_BRIXELIZER_PASS_COUNT  ///< The number of passes performed by Brixelizer.
} FfxBrixelizerPass;

/// An ID value for an instance created with Brixelizer.
///
/// @ingroup ffxBrixelizer
typedef uint32_t FfxBrixelizerInstanceID;

///  A structure representing the external resources needed for a Brixelizer cascade.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerCascadeResources
{
    FfxResource aabbTree;       ///< An FfxResource for storing the AABB tree of the cascade. This should be a structured buffer of size FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE and stride FFX_BRIXELIZER_CASCADE_AABB_TREE_STRIDE.
    FfxResource brickMap;       ///< An FfxResource for storing the brick map of the cascade. This should be a structured buffer of size FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE and stride FFX_BRIXELIZER_CASCADE_BRICK_MAP_STRIDE.
} FfxBrixelizerCascadeResources;

/// A structure representing all external resources for use with Brixelizer.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerResources
{
    FfxResource                   sdfAtlas;                                      ///< An FfxResource for storing the SDF atlas. This should be a 512x512x512 3D texture of 8-bit unorm values.
    FfxResource                   brickAABBs;                                    ///< An FfxResource for storing the brick AABBs. This should be a structured buffer containing 64*64*64 32-bit values.
    FfxBrixelizerCascadeResources cascadeResources[FFX_BRIXELIZER_MAX_CASCADES]; ///< Cascade resources.
} FfxBrixelizerResources;

/// A structure encapsulating the parameters necessary to register a buffer with
/// the Brixelizer API.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerBufferDescription {
    FfxResource  buffer;        ///< An <c><i>FfxResource</i></c> of the buffer.
    uint32_t    *outIndex;      ///< A pointer to a <c><i>uint32_t</i></c> to receive the index assigned to the buffer.
} FfxBrixelizerBufferDescription;

/// Flags used for specifying debug drawing of AABBs.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerCascadeDebugAABB {
    FFX_BRIXELIZER_CASCADE_DEBUG_AABB_NONE,
    FFX_BRIXELIZER_CASCADE_DEBUG_AABB_BOUNDING_BOX,
    FFX_BRIXELIZER_CASCADE_DEBUG_AABB_AABB_TREE,
} FfxBrixelizerCascadeDebugAABB;

/// A structure encapsulating the parameters for drawing a debug visualization.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerDebugVisualizationDescription
{
    float                             inverseViewMatrix[16];                         ///< Inverse view matrix for the scene in row major order.
    float                             inverseProjectionMatrix[16];                   ///< Inverse projection matrix for the scene in row major order.
    FfxBrixelizerTraceDebugModes      debugState;                                    ///< An FfxBrixelizerTraceDebugModes determining what kind of debug output to draw.
    uint32_t                          startCascadeIndex;                             ///< The index of the most detailed cascade in the cascade chain.
    uint32_t                          endCascadeIndex;                               ///< The index of the least detailed cascade in the cascade chain.
    float                             sdfSolveEps;                                   ///< The epsilon value used in SDF ray marching.
    float                             tMin;                                          ///< The tMin value for minimum ray intersection.
    float                             tMax;                                          ///< The tMax value for maximum ray intersection.
    uint32_t                          renderWidth;                                   ///< The width of the output resource.
    uint32_t                          renderHeight;                                  ///< The height of the output resource.
    FfxResource                       output;                                        ///< An FfxResource to draw the debug visualization to.

    FfxCommandList                    commandList;                                   ///< An FfxCommandList to write the draw commands to.
    uint32_t                          numDebugAABBInstanceIDs;                       ///< The number of FfxBrixelizerInstanceIDs in the debugAABBInstanceIDs array.
    const FfxBrixelizerInstanceID    *debugAABBInstanceIDs;                          ///< An array of FfxBrixelizerInstanceIDs for instances to draw the bounding boxes of.
    FfxBrixelizerCascadeDebugAABB     cascadeDebugAABB[FFX_BRIXELIZER_MAX_CASCADES]; ///< An array of flags showing what AABB debug output to draw for each cascade.
} FfxBrixelizerDebugVisualizationDescription;

/// Flags for options for Brixelizer context creation.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerContextFlags
{
    FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CONTEXT_READBACK_BUFFERS = (1 << 0), ///< Create a context with context readback buffers enabled. Needed to use <c><i>ffxBrixelizerContextGetDebugCounters</i></c>.
    FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS = (1 << 1), ///< Create a context with cascade readback buffers enabled. Needed to use <c><i>ffxBrixelizerContextGetCascadeCounters</i></c>.
    FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_AABBS                    = (1 << 2), ///< Create a context with debug AABBs enabled.
    FFX_BRIXELIZER_CONTEXT_FLAG_ALL_DEBUG                      = FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CONTEXT_READBACK_BUFFERS | FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS | FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_AABBS, ///< Create a context with all debugging features enabled.
} FfxBrixelizerContextFlags;

/// Flags used for creating Brixelizer jobs. Determines whether a job is a submission of geometry or invalidating
/// an area described by an AABB.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerRawJobFlags {
    FFX_BRIXELIZER_RAW_JOB_FLAG_NONE       = 0u,
    FFX_BRIXELIZER_RAW_JOB_FLAG_INVALIDATE = 1u << 2u,
} FfxBrixelizerRawJobFlags;

/// Flags used for creating Brixelizer instances.
///
/// @ingroup ffxBrixelizer
typedef enum FfxBrixelizerRawInstanceFlags {
    FFX_BRIXELIZER_RAW_INSTANCE_FLAG_NONE = 0u,
    FFX_BRIXELIZER_RAW_INSTANCE_FLAG_USE_INDEXLESS_QUAD_LIST = 1u << 1u,
} FfxBrixelizerRawInstanceFlags;

/// A structure encapsulating the FidelityFX Brixelizer context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by Brixelizer.
///
/// The <c><i>FfxBrixelizerRawContext</i></c> object should have a lifetime matching
/// your use of Brixelizer. Before destroying the Brixelizer context care
/// should be taken to ensure the GPU is not accessing the resources created
/// or used by Brixelizer. It is therefore recommended that the GPU is idle
/// before destroying the Brixelizer context.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerRawContext
{
    uint32_t data[FFX_BRIXELIZER_RAW_CONTEXT_SIZE];
} FfxBrixelizerRawContext;

/// A structure encapsulating the parameters for creating a Brixelizer context.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerRawContextDescription
{
    size_t                        maxDebugAABBs;    ///< The maximum number of AABBs that can be drawn in debug mode. Note to use debug AABBs the flag <c><i>FFX_BRIXELIZER_CONTEXT_FLAG</i></c> must be passed at context creation.
    FfxBrixelizerContextFlags     flags;            ///< A combination of <c><i>FfxBrixelizerContextFlags</i></c> specifying options for the context.
    FfxInterface                  backendInterface; ///< An FfxInterface representing the FidelityFX backend interface.
    
} FfxBrixelizerRawContextDescription;

/// A structure encapsulating the parameters for creating a Brixelizer cascade.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerRawCascadeDescription
{
    float       brickSize;      ///< The edge size of a brick in world units.
    float       cascadeMin[3];  ///< Corner of the first brick.
    uint32_t    index;          ///< Index of the cascade.
} FfxBrixelizerRawCascadeDescription;

/// A structure describing a Brixelizer job.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerRawJobDescription
{
    float    aabbMin[3];      ///< The mimimum corner of the AABB of the job.
    float    aabbMax[3];      ///< The maximum corner of the AABB of the job.
    uint32_t flags;           ///< Flags for the job (to be set from FfxBrixelizerRawJobFlags).
    uint32_t instanceIdx;     ///< The ID for an instance for the job.
} FfxBrixelizerRawJobDescription;

/// A structure encapsulating the parameters for updating a Brixelizer cascade.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerRawCascadeUpdateDescription
{
    uint32_t                              maxReferences;                 ///< storage for triangle->voxel references
    uint32_t                              triangleSwapSize;              ///< scratch storage for triangles
    uint32_t                              maxBricksPerBake;              ///< max SDF brick baked per update
    int32_t                               cascadeIndex;                  ///< Target Cascade
    const FfxBrixelizerRawJobDescription *jobs;                          ///< A pointer to an array of jobs.
    size_t                                numJobs;                       ///< The number of jobs in the array pointed to by jobs.
    float                                 cascadeMin[3];                 ///< Lower corner of the first brick in world space.
    int32_t                               clipmapOffset[3];              ///< Changing that invalidates portion of the cascade. it's an offset in the voxel->brick table.
    uint32_t                              flags;                         ///< See FfxBrixelizerCascadeUpdateFlags.
} FfxBrixelizerRawCascadeUpdateDescription;

/// A structure encapsulating the parameters for an instance to be added to a
/// Brixelizer context.
///
/// @ingroup ffxBrixelizer
typedef struct FfxBrixelizerRawInstanceDescription
{
    float                     aabbMin[3];           ///< The minimum coordinates of an AABB surrounding the instance.
    float                     aabbMax[3];           ///< The maximum coordinates of an AABB surrounding the instance.
    FfxFloat32x3x4            transform;            ///< A tranform of the instance into world space. The transform is in row major order.

    FfxIndexFormat            indexFormat;          ///< The format of the index buffer. Accepted formats are FFX_INDEX_UINT16 or FFX_INDEX_UINT32.
    uint32_t                  indexBuffer;          ///< The index of the index buffer set with ffxBrixelizerContextSetBuffer.
    uint32_t                  indexBufferOffset;    ///< An offset into the index buffer.
    uint32_t                  triangleCount;        ///< The count of triangles in the index buffer.

    uint32_t                  vertexBuffer;         ///< The index of the vertex buffer set with ffxBrixelizerContextSetBuffer.
    uint32_t                  vertexStride;         ///< The stride of the vertex buffer in bytes.
    uint32_t                  vertexBufferOffset;   ///< An offset into the vertex buffer.
    uint32_t                  vertexCount;          ///< The count of vertices in the vertex buffer.
    FfxSurfaceFormat          vertexFormat;         ///< The format of vertices in the vertex buffer. Accepted values are FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT and FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT.

    uint32_t                  flags;                ///< Flags for the instance. See <c><i>FfxBrixelizerRawInstanceFlags</i></c>.
    FfxBrixelizerInstanceID  *outInstanceID;        ///< A pointer to an <c><i>FfxBrixelizerInstanceID</i></c> to be filled with the instance ID assigned for the instance.
} FfxBrixelizerRawInstanceDescription;


/// Get the size in bytes needed for an <c><i>FfxBrixelizerRawContext</i></c> struct.
/// Note that this function is provided for consistency, and the size of the
/// <c><i>FfxBrixelizerRawContext</i></c> is a known compile time value which can be
/// obtained using <c><i>sizeof(FfxBrixelizerRawContext)</i></c>.
///
/// @return  The size in bytes of an <c><i>FfxBrixelizerRawContext</i></c> struct.
///
/// @ingroup ffxBrixelizer
inline size_t ffxBrixelizerRawGetContextSize()
{
    return sizeof(FfxBrixelizerRawContext);
}

/// Create a FidelityFX Brixelizer context from the parameters
/// specified to the <c><i>FfxBrixelizerRawContextDescription</i></c> struct.
///
/// The context structure is the main object used to interact with the Brixelizer API,
/// and is responsible for the management of the internal resources used by the
/// Brixelizer algorithm. When this API is called, multiple calls will be made via
/// the pointers contained in the <b><i>backendInterface</i></b> structure. This
/// backend will attempt to retrieve the device capabilities, and create the internal
/// resources, and pipelines required by Brixelizer.
///
/// Depending on the parameters passed in via the <b><i>contextDescription</b></i> a
/// different set of resources and pipelines may be requested by the callback functions.
///
/// The <c><i>FfxBrixelizerRawContext</i></c> should be destroyed when use of it is completed.
/// To destroy the context you should call <c><i>ffxBrixelizerContextDestroy</i></c>.
///
/// @param [out] context               A pointer to a <c><i>FfxBrixelizerRawContext</i></c> to populate.
/// @param [in]  contextDescription    A pointer to a <c><i>FfxBrixelizerRawContextDescription</i></c> specifying the parameters for context creation.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE     The operation failed because <c><i>contextDescription->backendInterface</i></c> was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR        The operation failed because of an error from the backend.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextCreate(FfxBrixelizerRawContext* context, const FfxBrixelizerRawContextDescription* contextDescription);

/// Destroy the FidelityFX Brixelizer context.
///
/// @param [out] context        A pointer to a <c><i>FfxBrixelizerRawContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER   The <c><i>context</i></c> pointer provided was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextDestroy(FfxBrixelizerRawContext* context);

/// Get an <c><i>FfxBrixelizerContextInfo</i></c> structure with the details for <c><i>context</i></c>.
/// This call is intended to be used to fill in a constant buffer necessary for making ray
/// queries.
///
/// @param [out] context        The <c><i>FfxBrixelizerRawContext</i></c> to receive the <c><i>FfxBrixelizerContextInfo</i></c> of.
/// @param [out] contextInfo    A <c><i>FfxBrixelizerContextInfo</i></c> struct to be filled in.
///
/// @retval
/// FFX_OK                      The operation was successful.
/// @retval
/// FFX_ERROR_INVALID_POINTER   The <c><i>context</i></c> pointer provided was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextGetInfo(FfxBrixelizerRawContext* context, FfxBrixelizerContextInfo* contextInfo);

/// Create a cascade for use with Brixelizer.
///
/// @param [out] context               The <c><i>FfxBrixelizerRawContext</i></c> to create a cascade for.
/// @param [in]  cascadeDescription    A <c><i>FfxBrixelizerRawCascadeDescription</i></c> struct specifying the parameters for cascade creation.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because one of <c><i>context</i></c>, <c><i>cascadeDescription</i></c>, c><i>cascadeDescription->aabbTree</i></c> or c><i>cascadeDescription->brickMap</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR        The operation encountered an error in the backend.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextCreateCascade(FfxBrixelizerRawContext* context, const FfxBrixelizerRawCascadeDescription* cascadeDescription);

/// Destroy a cascade previously created with <c><i>ffxBrixelizerContextCreateCascade</i></c>.
///
/// @param [out] context               The <c><i>FfxBrixelizerRawContext</i></c> to delete a cascade for.
/// @param [in]  cascadeIndex          The index of the cascade to delete.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextDestroyCascade(FfxBrixelizerRawContext* context, uint32_t cascadeIndex);

/// Reset a cascade previously created with <c><i>ffxBrixelizerContextCreateCascade</i></c>.
///
/// @param [out] context               The <c><i>FfxBrixelizerRawContext</i></c> to reset a cascade for.
/// @param [in]  cascadeIndex          The index of the cascade to reset.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT         No cascade with index <c><i>cascadeIndex</i></c> exists.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextResetCascade(FfxBrixelizerRawContext* context, uint32_t cascadeIndex);

/// Begin constructing GPU commands for updating SDF acceleration structures with Brixelizer.
/// Must be called between calls to <c><i>ffxBrixelizerContextBegin</i></c> and <c><i>ffxBrixelizerContextEnd</i></c>.
///
/// @param [out] context               The <c><i>FfxBrixelizerRawContext</i></c> to begin a frame for.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE              The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextBegin(FfxBrixelizerRawContext* context, FfxBrixelizerResources resources);

/// End construcring GPU commands for updating the SDF acceleration structures with Brixelizer.
///
/// @param [out] context               The <c><i>FfxBrixelizerRawContext</i></c> to end a frame for.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE              The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextEnd(FfxBrixelizerRawContext* context);

/// Record GPU commands to a <c><i>FfxCommandList</i></c> for updating acceleration structures with Brixelizer.
///
/// @param [out] context               The <c><i>FfxBrixelizerRawContext</i></c> to record GPU commands from.
/// @param [out] cmdList               The <c><i>FfxCommandList</i></c> to record commands to.
///
/// @retval
/// FFX_OK                             The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER          The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE              The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextSubmit(FfxBrixelizerRawContext* context, FfxCommandList cmdList);

/// Get the size in bytes needed from a <c><i>FfxResource</i></c> to be used as a scratch buffer in a cascade update.
///
/// @param [out] context                   The <c><i>FfxBrixelizerRawContext</i></c> to calculate the required scratch buffer size for.
/// @param [in]  cascadeUpdateDescription  A <c><i>FfxBrixelizerRawCascadeUpdateDescription</i></c> struct with the parameters for the cascade update.
/// @param [out] size                      A <c><i>size_t</i></c> to store the required scratch buffer size to.
///
/// @retval
/// FFX_OK                                 The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER              The operation failed because <c><i>context</i></c> or <c><i>cascadeUpdateDescription</c></i> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE                  The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextGetScratchMemorySize(FfxBrixelizerRawContext* context, const FfxBrixelizerRawCascadeUpdateDescription* cascadeUpdateDescription, size_t* size);

/// Update a cascade in a Brixelizer context.
///
/// @param [out] context                   The <c><i>FfxBrixelizerRawContext</i></c> to perform the cascade update on.
/// @param [in]  cascadeUpdateDescription  A <c><i>FfxBrixelizerRawCascadeUpdateDescription</i></c> struct with the parameters for the cascade update.
///
/// @retval
/// FFX_OK                                 The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER              The operation failed because <c><i>context</i></c> or <c><i>cascadeUpdateDescription</c></i> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE                  The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextUpdateCascade(FfxBrixelizerRawContext* context, const FfxBrixelizerRawCascadeUpdateDescription* cascadeUpdateDescription);

/// Merge two cascades in a Brixelizer context.
/// Must be called between calls to <c><i>ffxBrixelizerRawContextBegin</i></c> and <c><i>ffxBrixelizerRawContextEnd</i></c>.
///
/// @param [out] context                   The <c><i>FfxBrixelizerRawContext</i></c> to merge cascades for.
/// @param [in]  srcCascadeAIdx            The index of the first source cascade.
/// @param [in]  srcCascadeBIdx            A <c><i>FfxResource</i></c> to store the required scratch buffer size to.
/// @param [in]  dstCascadeIdx             A <c><i>FfxResource</i></c> to store the required scratch buffer size to.
///
/// @retval
/// FFX_OK                                 The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER              The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE                  The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextMergeCascades(FfxBrixelizerRawContext* context, uint32_t src_cascade_A_idx, uint32_t src_cascade_B_idx, uint32_t dst_cascade_idx);

/// Build an AABB tree for a cascade in a Brixelizer context.
/// Must be called between calls to <c><i>ffxBrixelizerRawContextBegin</i></c> and <c><i>ffxBrixelizerRawContextEnd</i></c>.
///
/// @param [out] context                   The <c><i>FfxBrixelizerRawContext</i></c> to build an AABB tree for.
/// @param [in]  cascadeIndex              The index of the cascade to build the AABB tree of.
///
/// @retval
/// FFX_OK                                 The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER              The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE                  The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextBuildAABBTree(FfxBrixelizerRawContext* context, uint32_t cascadeIndex);

/// Create a debug visualization output of a Brixelizer context.
/// Must be called between calls to <c><i>ffxBrixelizerRawContextBegin</i></c> and <c><i>ffxBrixelizerRawContextEnd</i></c>.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to create a debug visualization for.
/// @param [in]  debugVisualizationDescription  A <c><i>FfxBrixelizerDebugVisualizationDescription</i></c> providing the parameters for the debug visualization.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> or <c><i>debugVisualizationDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_NULL_DEVICE                       The operation failed because the <c><i>FfxDevice</i></c> provided to the <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextDebugVisualization(FfxBrixelizerRawContext* context, const FfxBrixelizerDebugVisualizationDescription* debugVisualizationDescription);

/// Get the debug counters from a Brixelizer context.
/// Note to use this function the flag <c><i>FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CONTEXT_READBACK_BUFFERS</i></c> must
/// be passed at context creation.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to read the debug counters of.
/// @param [out] debugCounters                  A <c><i>FfxBrixelizerDebugCounters</i></c> struct to read the debug counters to.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> or <c><i>debugCounters</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextGetDebugCounters(FfxBrixelizerRawContext* context, FfxBrixelizerDebugCounters* debugCounters);

/// Get the cascade counters from a Brixelizer context.
/// Note to use this function the flag <c><i>FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS</i></c> must
/// be passed at context creation.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to read the cascade counters of.
/// @param [in]  cascadeIndex                   The index of the cascade to read the cascade counters of.
/// @param [out] counters                       A <c><i>FfxBrixelizerScratchCounters</i></c> struct to read the cascade counters to.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> or <c><i>counters</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextGetCascadeCounters(FfxBrixelizerRawContext* context, uint32_t cascadeIndex, FfxBrixelizerScratchCounters* counters);

/// Create an instance in a Brixelizer context.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to create an instance for.
/// @param [in]  instanceDescription            A <c><i>FfxBrixelizerRawInstanceDescription</i></c> struct with the parameters for the instance to create.
/// @param [out] numInstanceDescriptions        A <c><i>FfxBrixelizerInstanceID</i></c> to read the instance ID to.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> or <c><i>instanceDescription</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextCreateInstances(FfxBrixelizerRawContext* context, const FfxBrixelizerRawInstanceDescription* instanceDescriptions, uint32_t numInstanceDescriptions);

/// Destroy an instance in a Brixelizer context.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to delete an instance for.
/// @param [in]  instanceId                     The <c><i>FfxBrixelizerInstanceID</i></c> of the instance to delete.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextDestroyInstances(FfxBrixelizerRawContext* context, const FfxBrixelizerInstanceID* instanceIDs, uint32_t numInstanceIDs);

/// Flush all instances added to the Brixelizer context with <c><i>ffxBrixelizerRawContextCreateInstance</i></c> to the GPU.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to flush the instances for.
/// @param [in]  cmdList                        An <c><i>FfxCommandList</i></c> to record GPU commands to.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextFlushInstances(FfxBrixelizerRawContext* context, FfxCommandList cmdList);

/// Register a vertex or index buffer for use with Brixelizer.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to register a buffer for.
/// @param [in]  buffer                         An <c><i>FfxResource</i></c> with the buffer to be set.
/// @param [out] index                          The index of the registered buffer.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextRegisterBuffers(FfxBrixelizerRawContext* context, const FfxBrixelizerBufferDescription* bufferDescs, uint32_t numBufferDescs);

/// Unregister a previously registered vertex or index buffer.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to unregister the buffer from
/// @param [in]  index                          The index of the buffer to unregister.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextUnregisterBuffers(FfxBrixelizerRawContext* context, const uint32_t* indices, uint32_t numIndices);

/// Get the index of the recommended cascade to update given the total number of cascades and current frame.
/// Follows the pattern 0 1 0 2 0 1 0 3 0 etc. If 0 is the most detailed cascade and <c><i>maxCascades - 1</i></c>
/// is the least detailed cascade this ordering updates more detailed cascades more often.
///
/// @param [out] context                        The <c><i>FfxBrixelizerRawContext</i></c> to set a buffer for.
/// @param [in]  scratchBuffer                  A <c><i>FfxResource</i></c> for use as a scratch buffer.
///
/// @retval
/// FFX_OK                                      The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER                   The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API FfxErrorCode ffxBrixelizerRawContextRegisterScratchBuffer(FfxBrixelizerRawContext* context, FfxResource scratchBuffer);

/// Get the index of the recommended cascade to update given the total number of cascades and current frame.
/// Follows the pattern 0 1 0 2 0 1 0 3 0 etc. If 0 is the most detailed cascade and <c><i>maxCascades - 1</i></c>
/// is the least detailed cascade this ordering updates more detailed cascades more often.
///
/// @param [in] frameIndex                      The current frame index.
/// @param [in] maxCascades                     The total number of cascades.
///
/// @retval                                     The index of the cascade to update.
///
/// @ingroup ffxBrixelizer
FFX_API uint32_t     ffxBrixelizerRawGetCascadeToUpdate(uint32_t frameIndex, uint32_t maxCascades);

/// Check whether an <c><i>FfxResource</i></c> is <c><i>NULL</i></c>.
///
/// @param [in] resource                        An <c><i>FfxResource</i></c> to check for nullness.
///
/// @retval                                     <c><i>true</c></i> if <c><i>resource</i></c> is <c><i>NULL</i></c> else <c><i>false</i></c>.
///
/// @ingroup ffxBrixelizer
FFX_API bool         ffxBrixelizerRawResourceIsNull(FfxResource resource);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxBrixelizer
FFX_API FfxVersionNumber ffxBrixelizerGetEffectVersion();

#ifdef __cplusplus
}
#endif
