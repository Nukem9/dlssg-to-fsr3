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

// @defgroup PARALLEL_SORT

#pragma once

// Include the interface for the backend of the FSR2 API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup FfxParallelSort FidelityFX Parallel Sort
/// FidelityFX Single Pass Downsampler runtime library
/// 
/// @ingroup SDKComponents

/// FidelityFX Parallel Sort major version.
///
/// @ingroup FfxParallelSort
#define FFX_PARALLELSORT_VERSION_MAJOR     (1)

/// FidelityFX Parallel Sort minor version.
///
/// @ingroup FfxParallelSort
#define FFX_PARALLELSORT_VERSION_MINOR     (3)

/// FidelityFX Parallel Sort patch version.
///
/// @ingroup FfxParallelSort
#define FFX_PARALLELSORT_VERSION_PATCH     (0)

/// FidelityFX SPD context count
/// 
/// Defines the number of internal effect contexts required by SPD
///
/// @ingroup FfxParallelSort
#define FFX_PARALLELSORT_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup FfxParallelSort
#define FFX_PARALLELSORT_CONTEXT_SIZE      (373794)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// An enumeration of the passes which constitute the Parallel Sort algorithm.
///
/// Parallel Sort is implemented as a multi-pass algorithm that is invoked over
/// a number of successive iterations until all bits in the key are sorted.
/// For a more comprehensive description of Parallel Sort's inner workings, please
/// refer to the Parallel Sort reference documentation.
///
/// @ingroup FfxParallelSort
typedef enum FfxParallelSortPass
{

    FFX_PARALLELSORT_PASS_SETUP_INDIRECT_ARGS = 0,  ///< A pass which sets up indirect params to invoke sorting when <c><i>FFX_PARALLEL_SORT_INDIRECT</i></c> flag bit is set.
    FFX_PARALLELSORT_PASS_SUM,                      ///< A pass which counts the number of occurrences of each value in the data set.
    FFX_PARALLELSORT_PASS_REDUCE,                   ///< A pass which further reduces the counts across thread groups for faster offset calculations in large data sets.
    FFX_PARALLELSORT_PASS_SCAN,                     ///< A pass which prefixes the count totals into global offsets.
    FFX_PARALLELSORT_PASS_SCAN_ADD,                 ///< A pass which does a second prefix add the global offsets to each local thread group offset.
    FFX_PARALLELSORT_PASS_SCATTER,                  ///< A pass which performs a local sort of all values in the thread group and outputs to new global offset

    FFX_PARALLELSORT_PASS_COUNT                     ///< The number of passes in Parallel Sort
} FfxParallelSortPass;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxParallelSortContextDescription</i></c>. See <c><i>FfxParallelSortContextDescription</i></c>.
///
/// @ingroup FfxParallelSort
typedef enum FfxParallelSortInitializationFlagBits {

    FFX_PARALLELSORT_INDIRECT_SORT  = (1 << 0),     ///< A bit indicating if we should use indirect version of sort algorithm
    FFX_PARALLELSORT_PAYLOAD_SORT   = (1 << 1),     ///< A bit indicating if we should sort a payload buffer    

} FfxParallelSortInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Parallel Sort.
///
/// @ingroup FfxParallelSort
typedef struct FfxParallelSortContextDescription {

    uint32_t                    flags;                  ///< A collection of <c><i>FfxParallelSortInitializationFlagBits</i></c>.
    uint32_t                    maxEntries;             ///< Maximum number of entries to sort
    FfxInterface                backendInterface;       ///< A set of pointers to the backend implementation for FidelityFX.
} FfxParallelSortContextDescription;

/// A structure encapsulating the parameters needed to sort
/// the buffer(s) provided
///
/// @ingroup FfxParallelSort
typedef struct FfxParallelSortDispatchDescription {

    FfxCommandList              commandList;        ///< The <c><i>FfxCommandList</i></c> to record parallel sort compute commands into.
    FfxResource                 keyBuffer;          ///< The buffer resource containing the keys to sort
    FfxResource                 payloadBuffer;      ///< The (optional) payload buffer to sort (requires <c><i>FFX_PARALLELSORT_PAYLOAD_SORT</i></c> be set)
    uint32_t                    numKeysToSort;      ///< The number of keys in the buffer requiring sorting
} FfxParallelSortDispatchDescription;

/// A structure encapsulating the FidelityFX Parallel Sort context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by parallel sort.
///
/// The <c><i>FfxParallelSortContext</i></c> object should have a lifetime matching
/// your use of parallel sort. Before destroying the parallel sort context care
/// should be taken to ensure the GPU is not accessing the resources created or
/// used by parallel sort. It is therefore recommended that the GPU is idle
/// before destroying the parallel sort context.
///
/// @ingroup FfxParallelSort
typedef struct FfxParallelSortContext {

    uint32_t        data[FFX_PARALLELSORT_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxParallelSortContext;


/// Create a FidelityFX Parallel Sort context from the parameters
/// programmed to the <c><i>FfxParallelSortContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the parallel
/// sort API, and is responsible for the management of the internal resources
/// used by the parallel sort algorithm. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>callbacks</i></c>
/// structure. These callbacks will attempt to retreive the device capabilities,
/// and create the internal resources, and pipelines required by parallel sorts'
/// frame-to-frame function. Depending on the precise configuration used when
/// creating the <c><i>FfxParallelSortContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxParallelSortContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or parallel sort
/// upscaling is disabled by a user. To destroy the parallel sort context you
/// should call <c><i>ffxParallelSortContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxParallelSortContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxParallelSortContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxFsr2ContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxParallelSort
FFX_API FfxErrorCode ffxParallelSortContextCreate(FfxParallelSortContext* pContext, const FfxParallelSortContextDescription* pContextDescription);

/// Execute a FidelityFX Parallel Sort context to sort the provided data
/// according to the passed in dispatch description.
/// 
/// @param [out] pContext                A pointer to a <c><i>FfxParallelSortContext</i></c> structure to populate.
/// @param [in]  pDispatchDescription        A pointer to a <c><i>FfxParallelSortDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>sortDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxParallelSort
FFX_API FfxErrorCode ffxParallelSortContextDispatch(FfxParallelSortContext* pContext, const FfxParallelSortDispatchDescription* pDispatchDescription);

/// Destroy the FidelityFX Parallel Sort context.
///
/// @param [out] pContext                A pointer to a <c><i>FfxParallelSortContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup FfxParallelSort
FFX_API FfxErrorCode ffxParallelSortContextDestroy(FfxParallelSortContext* pContext);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup FfxParallelSort
FFX_API FfxVersionNumber ffxParallelSortGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
