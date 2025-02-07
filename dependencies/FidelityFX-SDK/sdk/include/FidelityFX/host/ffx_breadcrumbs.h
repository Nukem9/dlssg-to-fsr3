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

/// @defgroup ffxBreadcrumbs FidelityFX Breadcrumbs
/// FidelityFX Breadcrumbs runtime library
///
/// @ingroup SDKComponents


#pragma once

// Include the interface for the backend of the Breadcrumbs API.
#include <FidelityFX/host/ffx_interface.h>

/// FidelityFX Breadcrumbs major version.
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_VERSION_MAJOR      (1)

/// FidelityFX Breadcrumbs minor version.
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_VERSION_MINOR      (0)

/// FidelityFX Breadcrumbs patch version.
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_VERSION_PATCH      (0)

/// FidelityFX Breadcrumbs context count
/// 
/// Defines the number of internal effect contexts required by Breadcrumbs
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_CONTEXT_SIZE (128)

/// Maximal number of markers that can be written into single memory block.
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_MAX_MARKERS_PER_BLOCK ((1U << 31) - 1U)

/// List of marker types to be used in X() macro.
///
/// @ingroup ffxBreadcrumbs
#define FFX_BREADCRUMBS_MARKER_LIST \
	X(BEGIN_EVENT) \
	X(BEGIN_QUERY) \
	X(CLEAR_DEPTH_STENCIL) \
	X(CLEAR_RENDER_TARGET) \
	X(CLEAR_STATE) \
	X(CLEAR_UNORDERED_ACCESS_FLOAT) \
	X(CLEAR_UNORDERED_ACCESS_UINT) \
	X(CLOSE) \
	X(COPY_BUFFER_REGION) \
	X(COPY_RESOURCE) \
	X(COPY_TEXTURE_REGION) \
	X(COPY_TILES) \
	X(DISCARD_RESOURCE) \
	X(DISPATCH) \
	X(DRAW_INDEXED_INSTANCED) \
	X(DRAW_INSTANCED) \
	X(END_EVENT) \
	X(END_QUERY) \
	X(EXECUTE_BUNDLE) \
	X(EXECUTE_INDIRECT) \
	X(RESET) \
	X(RESOLVE_QUERY_DATA) \
	X(RESOLVE_SUBRESOURCE) \
	X(RESOURCE_BARRIER) \
	X(SET_COMPUTE_ROOT_SIGNATURE) \
	X(SET_DESCRIPTORS_HEAP) \
	X(SET_GRAPHICS_ROOT_SIGNATURE) \
	X(SET_PIPELINE_STATE) \
	X(SET_PREDICATION) \
	X(ATOMIC_COPY_BUFFER_UINT) \
	X(ATOMIC_COPY_BUFFER_UINT64) \
	X(RESOLVE_SUBRESOURCE_REGION) \
	X(SET_SAMPLE_POSITION) \
	X(SET_VIEW_INSTANCE_MASK) \
	X(WRITE_BUFFER_IMMEDIATE) \
	X(SET_PROTECTED_RESOURCE_SESSION) \
	X(BEGIN_RENDER_PASS) \
	X(BUILD_RAY_TRACING_ACCELERATION_STRUCTURE) \
	X(COPY_RAY_TRACING_ACCELERATION_STRUCTURE) \
	X(DISPATCH_RAYS) \
	X(EMIT_RAY_TRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO) \
	X(END_RENDER_PASS) \
	X(EXECUTE_META_COMMANDS) \
	X(INITIALIZE_META_COMMANDS) \
	X(SET_RAY_TRACING_STATE) \
	X(SET_SHADING_RATE) \
	X(SET_SHADING_RATE_IMAGE) \
	X(BEGIN_CONDITIONAL_RENDERING_EXT) \
	X(BEGIN_DEBUG_UTILS_LABEL_EXT) \
	X(BEGIN_QUERY_INDEXED_EXT) \
	X(BEGIN_RENDER_PASS_2) \
	X(BEGIN_TRANSFORM_FEEDBACK_EXT) \
	X(BIND_DESCRIPTOR_SETS) \
	X(BIND_PIPELINES) \
	X(BIND_SHADING_RATE_IMAGE_NV) \
	X(BLIT_IMAGE) \
	X(BUILD_ACCELERATION_STRUCTURE_NV) \
	X(CLEAR_ATTACHMENTS) \
	X(CLEAR_COLOR_IMAGE) \
	X(CLEAR_DEPTH_STENCIL_IMAGE) \
	X(COPY_ACCELERATION_STRUCTURE_NV) \
	X(COPY_BUFFER) \
	X(COPY_BUFFER_TO_IMAGE) \
	X(COPY_IMAGE) \
	X(COPY_IMAGE_TO_BUFFER) \
	X(DEBUG_MARKER_BEGIN_EXT) \
	X(DEBUG_MARKER_END_EXT) \
	X(DEBUG_MARKER_INSERT_EXT) \
	X(DISPATCH_BASE) \
	X(DISPATCH_INDIRECT) \
	X(DRAW) \
	X(DRAW_INDEXED) \
	X(DRAW_INDEXED_INDIRECT) \
	X(DRAW_INDEXED_INDIRECT_COUNT) \
	X(DRAW_INDIRECT) \
	X(DRAW_INDIRECT_BYTE_COUNT_EXT) \
	X(DRAW_INDIRECT_COUNT) \
	X(DRAW_MESH_TASKS_INDIRECT_COUNT_NV) \
	X(DRAW_MESH_TASKS_INDIRECT_NV) \
	X(DRAW_MESH_TASKS_NV) \
	X(END_CONDITIONAL_RENDERING_EXT) \
	X(END_DEBUG_UTILS_LABEL_EXT) \
	X(END_QUERY_INDEXED_EXT) \
	X(END_RENDER_PASS_2) \
	X(END_TRANSFORM_FEEDBACK_EXT) \
	X(EXECUTE_COMMANDS) \
	X(FILL_BUFFER) \
	X(INSERT_DEBUG_UTILS_LABEL_EXT) \
	X(NEXT_SUBPASS) \
	X(NEXT_SUBPASS_2) \
	X(PIPELINE_BARRIER) \
	X(PROCESS_COMMANDS_NVX) \
	X(RESERVE_SPACE_FOR_COMMANDS_NVX) \
	X(RESET_EVENT) \
	X(RESET_QUERY_POOL) \
	X(RESOLVE_IMAGE) \
	X(SET_CHECKPOINT_NV) \
	X(SET_EVENT) \
	X(SET_PERFORMANCE_MARKER_INTEL) \
	X(SET_PERFORMANCE_OVERRIDE_INTEL) \
	X(SET_PERFORMANCE_STREAM_MARKER_INTEL) \
	X(SET_SAMPLE_LOCATIONS_EXT) \
	X(SET_VIEWPORT_SHADING_RATE_PALETTE_NV) \
	X(TRACE_RAYS_NV) \
	X(UPDATE_BUFFER) \
	X(WAIT_EVENTS) \
	X(WRITE_ACCELERATION_STRUCTURES_PROPERTIES_NV) \
	X(WRITE_BUFFER_MARKER_AMD) \
	X(WRITE_BUFFER_MARKER_2_AMD) \
	X(WRITE_TIMESTAMP)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// An enumeration of bit flags used when creating a
/// <c><i>FfxBreadcrumbsContext</i></c>. See <c><i>FfxBreadcrumbsContextDescription</i></c>.
///
/// @ingroup ffxBreadcrumbs
typedef enum FfxBreadcrumbsInitializationFlagBits {

    FFX_BREADCRUMBS_PRINT_FINISHED_LISTS          = (1<<0),   ///< A bit indicating that fully finished command lists will be expanded during status printing (otherwise their entries will be collapsed).
    FFX_BREADCRUMBS_PRINT_NOT_STARTED_LISTS       = (1<<1),   ///< A bit indicating that command lists that haven't started execution on GPU yet will be expanded during status printing (otherwise their entries will be collapsed).
    FFX_BREADCRUMBS_PRINT_FINISHED_NODES          = (1<<3),   ///< A bit indicating that nested markers which already have finished execution will be expanded during status printing (otherwise they will merged into top level marker).
    FFX_BREADCRUMBS_PRINT_NOT_STARTED_NODES       = (1<<4),   ///< A bit indicating that nested markers which haven't started execution yet will be expanded during status printing (otherwise they will merged into top level marker).
    FFX_BREADCRUMBS_PRINT_EXTENDED_DEVICE_INFO    = (1<<5),   ///< A bit indicating that additional info about active GPU will be printed into output status.
    FFX_BREADCRUMBS_PRINT_SKIP_DEVICE_INFO        = (1<<6),   ///< A bit indicating that no info about active GPU will be printed into outpus status.
    FFX_BREADCRUMBS_PRINT_SKIP_PIPELINE_INFO      = (1<<7),   ///< A bit indicating no info about pipelines used for commands recorded between markers will be printed into output status.
    FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION = (1<<8),   ///< A bit indicating if internal synchronization should be applied (when using Breadcrumbs concurrently from multiple threads).
} FfxBreadcrumbsInitializationFlagBits;

/// Type of currently recorded marker, purely informational.
/// 
/// based on available methods of `ID3D12GraphicsCommandListX`, values of `D3D12_AUTO_BREADCRUMB_OP` and Vulkan `vkCmd*()` functions.
/// When using <c><i>FFX_BREADCRUMBS_MARKER_PASS</i></c> it is required to supply custom name for recording this type of marker. Otherwise it can
/// be left out as <c><i>NULL</i></c> and the Breadcrumbs will use default tag for this marker. It can be useful when recording multiple similar
/// commands in a row. Breadcrumbs will automatically add numbering to them so it's not needed to create your own numbered dynamic string.
/// 
/// @ingroup ffxBreadcrumbs
typedef enum FfxBreadcrumbsMarkerType {

    FFX_BREADCRUMBS_MARKER_PASS, ///< Marker for grouping sets of commands. It is required to supply custom name for this type.
#define X(marker) FFX_BREADCRUMBS_MARKER_##marker,
    FFX_BREADCRUMBS_MARKER_LIST
#undef X
} FfxBreadcrumbsMarkerType;

/// A structure encapsulating the parameters required to initialize FidelityFX Breadcrumbs.
///
/// @ingroup ffxBreadcrumbs
typedef struct FfxBreadcrumbsContextDescription {

    uint32_t                    flags;                     ///< A collection of <c><i>FfxBreadcrumbsInitializationFlagBits</i></c>.
    uint32_t                    frameHistoryLength;        ///< Number of frames to records markers for. Have to be larger than 0.
    uint32_t                    maxMarkersPerMemoryBlock;  ///< Controls the number of markers saved in single memory block. Have to be in range of [1..<c><i>FFX_BREADCRUMBS_MAX_MARKERS_PER_BLOCK</i></c>].
    uint32_t                    usedGpuQueuesCount;        ///< Number of entries in <c><i>pUsedGpuQueues</i></c>. Have to be larger than 0.
    uint32_t*                   pUsedGpuQueues;            ///< Pointer to an array of unique indices representing GPU queues used for command lists used with AMD FidelityFX Breadcrumbs Library.
    FfxAllocationCallbacks      allocCallbacks;            ///< Callbacks for managing memory in the library.
    FfxInterface                backendInterface;          ///< A set of pointers to the backend implementation for FidelityFX SDK.
} FfxBreadcrumbsContextDescription;


/// Wrapper for custom Breadcrumbs name tags with indicator whether to perform copy on them.
///
/// When custom name is supplied <c><i>isNameExternallyOwned</i></c> field controls whether to perform copy on the string.
/// If string memory is managed by the application (ex. static string) the copy can be omitted to save memory.
/// 
/// @ingroup ffxBreadcrumbs
typedef struct FfxBreadcrumbsNameTag
{
    const char*                 pName;                     ///< Custom name for the object. By default optional, can be left to <c><i>NULL</i></c>.
    bool                        isNameExternallyOwned;     ///< Controls if AMD FidelityFX Breadcrumbs Library should copy a custom name with backed-up memory.
} FfxBreadcrumbsNameTag;

/// Description for new command list to be enabled for writing AMD FidelityFX Breadcrumbs Library markers.
///
/// @ingroup ffxBreadcrumbs
typedef struct FfxBreadcrumbsCommandListDescription {

    FfxCommandList              commandList;               ///< Handle to the command list that will be used with breadcrumbs operations.
    uint32_t                    queueType;                 ///< Type of queue that list is used on.
    FfxBreadcrumbsNameTag       name;                      ///< Custom name for the command list.
    FfxPipeline                 pipeline;                  ///< Optional pipeline state to associate with newly registered command list (can be set later).
    uint16_t                    submissionIndex;           ///< Information about submit number that command list is sent to GPU. Purely informational to help in analysing output later.
} FfxBreadcrumbsCommandListDescription;

/// Description for pipeline state that will be used to tag breadcrumbs markers.
///
/// @ingroup ffxBreadcrumbs
typedef struct FfxBreadcrumbsPipelineStateDescription
{
    FfxPipeline                 pipeline;                  ///< Pipeline state that will be associated with set of Breadcrumbs markers.
    FfxBreadcrumbsNameTag       name;                      ///< Custom name for the pipeline state.
    FfxBreadcrumbsNameTag       vertexShader;              ///< Name of used Vertex Shader. Part of classic geometry processing pipeline, cannot be set together with compute, ray tracing or new mesh processing pipeline.
    FfxBreadcrumbsNameTag       hullShader;                ///< Name of used Hull Shader. Part of classic geometry processing pipeline, cannot be set together with compute, ray tracing or new mesh processing pipeline.
    FfxBreadcrumbsNameTag       domainShader;              ///< Name of used Domain Shader. Part of classic geometry processing pipeline, cannot be set together with compute, ray tracing or new mesh processing pipeline.
    FfxBreadcrumbsNameTag       geometryShader;            ///< Name of used Geometry Shader. Part of classic geometry processing pipeline, cannot be set together with compute, ray tracing or new mesh processing pipeline.
    FfxBreadcrumbsNameTag       meshShader;                ///< Name of used Mesh Shader. Part of new mesh processing pipeline, cannot be set together with compute, ray tracing or classic geometry processing pipeline.
    FfxBreadcrumbsNameTag       amplificationShader;       ///< Name of used Amplification Shader. Part of new mesh processing pipeline, cannot be set together with compute, ray tracing or classic geometry processing pipeline.
    FfxBreadcrumbsNameTag       pixelShader;               ///< Name of used Pixel Shader. Cannot be set together with <c><i>computeShader</i></c> or <c><i>rayTracingShader</i></c>.
    FfxBreadcrumbsNameTag       computeShader;             ///< Name of used Compute Shader. Have to be set exclusively to other shader names (indicates compute pipeline).
    FfxBreadcrumbsNameTag       rayTracingShader;          ///< Name of used Ray Tracing Shader. Have to be set exclusively to other shader names (indicates ray tracing pipeline).
} FfxBreadcrumbsPipelineStateDescription;

/// Output with current AMD FidelityFX Breadcrumbs Library markers log for post-mortem analysis.
///
/// @ingroup ffxBreadcrumbs
typedef struct FfxBreadcrumbsMarkersStatus
{
    size_t                      bufferSize;                ///< Size of the status buffer.
    char*                       pBuffer;                   ///< UTF-8 encoded buffer with log about markers execution. Have to be released with <c><i>FFX_FREE</i></c>.
} FfxBreadcrumbsMarkersStatus;

/// A structure encapsulating the FidelityFX Breadcrumbs context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by AMD FidelityFX Breadcrumbs Library.
///
/// The <c><i>FfxBreadcrumbsContext</i></c> object should have a lifetime matching
/// your use of Breadcrumbs. Before destroying the Breadcrumbs context care should be taken
/// to ensure the GPU is not accessing the resources created or used by Breadcrumbs.
/// It is therefore recommended that the GPU is idle before destroying the
/// Breadcrumbs context.
///
/// @ingroup ffxBreadcrumbs
typedef struct FfxBreadcrumbsContext
{
    uint32_t                    data[FFX_BREADCRUMBS_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxBreadcrumbsContext;

/// Create a FidelityFX Breadcrumbs context from the parameters
/// programmed to the <c><i>FfxBreadcrumbsContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the Breadcrumbs
/// API, and is responsible for the management of the internal buffers used
/// by the Breadcrumbs algorithm. For each provided queue there will be created
/// a buffer that will hold contents of the saved markers, awaiting for retrieval
/// per call to <c><i>ffxBreadcrumbsPrintStatus()</i></c>
///
/// When choosing the number of frames to save markers for,
/// specified in the <c><i>frameHistoryLength</i></c> field of
/// <c><i>FfxBreadcrumbsContextDescription</i></c>, typically can be set to the number of
/// frames in flight in the application, but for longer history it can be increased.
/// 
/// Buffers for markers are allocated at fixed size, allowing for certain
/// number of markers to be saved in them. The size of this buffers are
/// determined by <c><i>maxMarkersPerMemoryBlock</i></c> field of
/// <c><i>FfxBreadcrumbsContextDescription</i></c>. When needed new ones are created but to avoid
/// multiple allocations you can estimate how many markers will be used in single frame.
///
/// The <c><i>FfxBreadcrumbsContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded. To destroy the Breadcrumbs context
/// you should call <c><i>ffxBreadcrumbsContextDestroy</i></c>.
///
/// @param [out] pContext               A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure to populate.
/// @param [in]  pContextDescription    A pointer to a <c><i>FfxBreadcrumbsContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxBreadcrumbsContextDescription.backendInterface</i></c> was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsContextCreate(FfxBreadcrumbsContext* pContext, const FfxBreadcrumbsContextDescription* pContextDescription);

/// Destroy the FidelityFX Breadcrumbs context.
/// 
/// Should always be called from a single thread for same context.
///
/// @param [out] pContext               A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsContextDestroy(FfxBreadcrumbsContext* pContext);

/// Begins new frame of execution for FidelityFX Breadcrumbs.
/// 
/// Should always be called from a single thread for same context.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsStartFrame(FfxBreadcrumbsContext* pContext);

/// Register new command list for current frame FidelityFX Breadcrumbs operations.
/// 
/// After call to <c><i>ffxBreadcrumbsStartFrame()</i></c> every previously used list has to be registered again.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
/// @param [in] pCommandListDescription A pointer to a <c><i>FfxBreadcrumbsCommandListDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> or <c><i>pCommandListDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          The operation failed because given command list has been already registered.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsRegisterCommandList(FfxBreadcrumbsContext* pContext, const FfxBreadcrumbsCommandListDescription* pCommandListDescription);

/// Register new pipeline state to associate later with FidelityFX Breadcrumbs operations.
/// 
/// Information about pipeline is preserved across frames so only single call after creation of pipeline is needed.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
/// @param [in] pPipelineDescription    A pointer to a <c><i>FfxBreadcrumbsPipelineStateDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> or <c><i>pPipelineDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          The operation failed because given pipeline has been already registered or <c><i>pPipelineDescription</i></c> contains incorrect data.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsRegisterPipeline(FfxBreadcrumbsContext* pContext, const FfxBreadcrumbsPipelineStateDescription* pPipelineDescription);

/// Associate specific pipeline state with following FidelityFX Breadcrumbs markers.
/// 
/// When recorded commands use specific pipelines you can save this information, associating said pipelines
/// with recorded markers, so later on additional information can be displayed when using <c><i>ffxBreadcrumbsPrintStatus()</i></c>.
/// To reset currently used pipeline just pass <c><i>NULL</i></c> as <c><i>pipeline</i></c> param.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
/// @param [in] commandList             Previously registered command list.
/// @param [in] pipeline                Previously registered pipeline.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>context</i></c> or <c><i>commandList</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          The operation failed because given pipeline or command list has not been registered yet.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsSetPipeline(FfxBreadcrumbsContext* pContext, FfxCommandList commandList, FfxPipeline pipeline);

/// Begin new FidelityFX Breadcrumbs marker section.
/// 
/// New section has to be ended with <c><i>ffxBreadcrumbsEndMarker()</i></c>
/// but multiple <c><i>ffxBreadcrumbsBeginMarker()</i></c> nesting calls are possible.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
/// @param [in] commandList             Previously registered command list.
/// @param [in] type                    Type of the marker section.
/// @param [in] pName                   Custom name for the marker section. Have to contain correct string if <c><i>type</i></c> is <c><i>FFX_BREADCRUMBS_MARKER_PASS()</i></c>.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> or <c><i>pName</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          The operation failed because given command list has not been registered yet or <c><i>pName</i></c> doesn't contain correct string.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsBeginMarker(FfxBreadcrumbsContext* pContext, FfxCommandList commandList, FfxBreadcrumbsMarkerType type, const FfxBreadcrumbsNameTag* pName);

/// End FidelityFX Breadcrumbs marker section.
/// 
/// Has to be preceeded by <c><i>ffxBreadcrumbsBeginMarker()</i></c>.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
/// @param [in] commandList             Previously registered command list.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because <c><i>pContext</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INVALID_ARGUMENT          The operation failed because given command list has not been registered yet.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsEndMarker(FfxBreadcrumbsContext* pContext, FfxCommandList commandList);

/// Gather information about current FidelityFX Breadcrumbs markers status.
/// 
/// After receiving device lost error on GPU you can use this method to print post-mortem log of markers execution
/// to determine which commands in which frame were in flight during the crash.
/// Should always be called from a single thread.
///
/// @param [in] pContext                A pointer to a <c><i>FfxBreadcrumbsContext</i></c> structure.
/// @param [out] pMarkersStatus         Buffer with post-mortem log of Breadcrumbs markers.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           The operation failed because either <c><i>pContext</i></c> or <c><i>pMarkersStatus</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxErrorCode ffxBreadcrumbsPrintStatus(FfxBreadcrumbsContext* pContext, FfxBreadcrumbsMarkersStatus* pMarkersStatus);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxBreadcrumbs
FFX_API FfxVersionNumber ffxBreadcrumbsGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
