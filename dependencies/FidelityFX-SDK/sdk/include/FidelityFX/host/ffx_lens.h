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

// @defgroup Lens

#pragma once

// Include the interface for the backend of the Lens API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup FfxLens FidelityFX Lens
/// FidelityFX Lens runtime library
/// 
/// @ingroup SDKComponents

/// FidelityFX Lens major version.
///
/// @ingroup FfxLens
#define FFX_LENS_VERSION_MAJOR (1)

/// FidelityFX Lens minor version.
///
/// @ingroup FfxLens
#define FFX_LENS_VERSION_MINOR (1)

/// FidelityFX Lens patch version.
///
/// @ingroup FfxLens
#define FFX_LENS_VERSION_PATCH (0)

/// FidelityFX Lens context count
/// 
/// Defines the number of internal effect contexts required by Lens
///
/// @ingroup FfxLens
#define FFX_LENS_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup FfxLens
#define FFX_LENS_CONTEXT_SIZE (9200)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// An enumeration of the pass which constitutes the Lens algorithm.
///
/// Lens is implemented as a single pass algorithm. Each call to the
/// <c><i>FfxLensScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single lens effect compute job. For a
/// more comprehensive description of Lens's inner workings, please
/// refer to the Lens reference documentation.
///
/// @ingroup FfxLens
typedef enum FfxLensPass
{
    FFX_LENS_PASS_MAIN_PASS = 0,     ///< A pass which which applies the lens effect
    FFX_LENS_PASS_COUNT              ///< The number of passes in Lens
} FfxLensPass;

typedef enum FfxLensFloatPrecision
{
    FFX_LENS_FLOAT_PRECISION_32BIT = 0,
    FFX_LENS_FLOAT_PRECISION_16BIT = 1,
    FFX_LENS_FLOAT_PRECISION_COUNT = 2
} FfxLensFloatPrecision;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxLensContext</i></c>. See <c><i>FfxLensContextDescription</i></c>.
///
/// @ingroup FfxLens
typedef enum FfxLensInitializationFlagBits {

    FFX_LENS_MATH_NONPACKED          = (1 << 0),     ///< A bit indicating if we should use floating point math
    FFX_LENS_MATH_PACKED             = (1 << 1)      ///< A bit indicating if we should use 16-bit half precision floating point math (favored)

} FfxLensInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Lens.
///
/// @ingroup FfxLens
typedef struct FfxLensContextDescription {

    uint32_t                    flags;                  ///< A collection of <c><i>FfxLensInitializationFlagBits</i></c>
    FfxSurfaceFormat            outputFormat;           ///< Format of the output target used for creation of output resource.
    FfxLensFloatPrecision       floatPrecision;         ///< A flag indicating the desired floating point precision for use in ffxBlurContextDispatch
    FfxInterface                backendInterface;       ///< A set of pointers to the backend implementation for FidelityFX.

} FfxLensContextDescription;

/// A structure encapsulating the parameters for dispatching
/// of FidelityFX Lens
///
/// @ingroup FfxLens
typedef struct FfxLensDispatchDescription {

    FfxCommandList              commandList;        ///< The <c><i>FfxCommandList</i></c> to record Lens rendering commands into.
    FfxResource                 resource;           ///< The <c><i>FfxResource</i></c> to run Lens on.
    FfxResource                 resourceOutput;     ///< The <c><i>FfxResource</i></c> to write Lens output to.
    FfxDimensions2D             renderSize;         ///< The resolution used for rendering the scene.
    float                       grainScale;         ///< Artistic tweaking constant for grain scale..
    float                       grainAmount;        ///< Artistic tweaking constant for how intense the grain is.
    uint32_t                    grainSeed;          ///< The seed for grain RNG.
    float                       chromAb;            ///< Artistic tweaking constant for chromatic aberration intensity.
    float                       vignette;           ///< Artistic tweaking constant for vignette intensity.

} FfxLensDispatchDescription;

/// A structure encapsulating the FidelityFX Lens context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by Lens.
///
/// The <c><i>FfxLensContext</i></c> object should have a lifetime matching
/// your use of Lens. Before destroying the Lens context care should be taken
/// to ensure the GPU is not accessing the resources created or used by Lens.
/// It is therefore recommended that the GPU is idle before destroying the
/// Lens context.
///
/// @ingroup FfxLens
typedef struct FfxLensContext {
    uint32_t                    data[FFX_LENS_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxLensContext;

/// Create a FidelityFX Lens Downsampler context from the parameters
/// programmed to the <c><i>FfxLensContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the Lens
/// API, and is responsible for the management of the internal resources
/// used by the Lens algorithm. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>backendInterface</i></c>
/// structure. This backend will attempt to retrieve the device capabilities,
/// and create the internal resources, and pipelines required by Lens to function.
/// Depending on the precise configuration used when
/// creating the <c><i>FfxLensContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxLensContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or Lens
/// upscaling is disabled by a user. To destroy the Lens context you
/// should call <c><i>ffxLensContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxLensContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxLensContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxLensContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxLens
FFX_API FfxErrorCode ffxLensContextCreate(FfxLensContext* pContext, const FfxLensContextDescription* pContextDescription);

/// Dispatches work to the FidelityFX Lens context
///
/// @param [out] pContext                A pointer to a <c><i>FfxLensContext</i></c> structure to populate.
/// @param [in]  pDispatchDescription    A pointer to a <c><i>FfxLensDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxLens
FFX_API FfxErrorCode ffxLensContextDispatch(FfxLensContext* pContext, const FfxLensDispatchDescription* pDispatchDescription);

/// Destroy the FidelityFX Lens context.
///
/// @param [out] pContext                A pointer to a <c><i>FfxLensContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup FfxLens
FFX_API FfxErrorCode ffxLensContextDestroy(FfxLensContext* pContext);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup FfxLens
FFX_API FfxVersionNumber ffxLensGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
