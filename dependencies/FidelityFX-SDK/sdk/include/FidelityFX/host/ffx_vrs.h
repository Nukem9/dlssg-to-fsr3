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

// @defgroup VRS

#pragma once

// Include the interface for the backend of the FSR 2.0 API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup FfxVrs FidelityFX VRS
/// FidelityFX Variable Rate Shading runtime library
///
/// @ingroup SDKComponents

/// FidelityFX VRS major version.
///
/// @ingroup FfxVrs
#define FFX_VRS_VERSION_MAJOR (1)

/// FidelityFX VRS minor version.
///
/// @ingroup FfxVrs
#define FFX_VRS_VERSION_MINOR (2)

/// FidelityFX VRS patch version.
///
/// @ingroup FfxVrs
#define FFX_VRS_VERSION_PATCH (0)

/// FidelityFX VRS context count
///
/// Defines the number of internal effect contexts required by VRS
///
/// @ingroup FfxVrs
#define FFX_VRS_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup FfxVrs
#define FFX_VRS_CONTEXT_SIZE (16536)

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)

/// An enumeration of the pass which constitutes the ShadingRateImage
/// generation algorithm.
///
/// VRS is implemented as a single pass algorithm. Each call to the
/// <c><i>FfxScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single image generation job. For a
/// more comprehensive description of VRS's inner workings, please
/// refer to the VRS reference documentation.
///
/// @ingroup FfxVrs
typedef enum FfxVrsPass
{
    FFX_VRS_PASS_IMAGEGEN = 0,  ///< A pass which generates a ShadingRateImage.
    FFX_VRS_PASS_COUNT          ///< The number of passes performed by VRS.
} FfxVrsPass;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxVrsContext</i></c>. See <c><i>FfxVrsContextDescription</i></c>.
///
/// @ingroup FfxVrs
typedef enum FfxVrsInitializationFlagBits
{
    FFX_VRS_ALLOW_ADDITIONAL_SHADING_RATES = (1 << 0),  ///< A bit indicating if we should enable additional shading rates.
} FfxVrsInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Variable Shading
///
/// @ingroup FfxVrs
typedef struct FfxVrsContextDescription
{
    uint32_t     flags;                                 ///< A collection of <c><i>FfxVrsInitializationFlagBits</i></c>
    uint32_t     shadingRateImageTileSize;              ///< ShadingRateImage tile size.
    FfxInterface backendInterface;                      ///< A set of pointers to the backend implementation for FidelityFX.
} FfxVrsContextDescription;

/// A structure encapsulating the parameters for dispatching the various passes
/// of FidelityFX Variable Shading
///
/// @ingroup FfxVrs
typedef struct FfxVrsDispatchDescription
{
    FfxCommandList   commandList;        ///< The <c><i>FfxCommandList</i></c> to record VRS rendering commands into.
    FfxResource      historyColor;       ///< A <c><i>FfxResource</i></c> containing the color buffer for the previous frame (at presentation resolution).
    FfxResource      motionVectors;      ///< A <c><i>FfxResource</i></c> containing the velocity buffer for the current frame (at presentation resolution).
    FfxResource      output;             ///< A <c><i>FfxResource</i></c> containing the ShadingRateImage buffer for the current frame.
    FfxDimensions2D  renderSize;         ///< The resolution that was used for rendering the input resource.
    float            varianceCutoff;     ///< This value specifies how much variance in luminance is acceptable to reduce shading rate.
    float            motionFactor;       ///< The lower this value, the faster a pixel has to move to get the shading rate reduced.
    uint32_t         tileSize;           ///< ShadingRateImage tile size.
    FfxFloatCoords2D motionVectorScale;  ///< Scale motion vectors to different format
} FfxVrsDispatchDescription;

/// A structure encapsulating the FidelityFX Variable Shading context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by VRS.
///
/// The <c><i>FfxVrsContext</i></c> object should have a lifetime matching
/// your use of VRS. Before destroying the VRS context care should be taken
/// to ensure the GPU is not accessing the resources created or used by VRS.
/// It is therefore recommended that the GPU is idle before destroying the
/// VRS context.
///
/// @ingroup FfxVrs
typedef struct FfxVrsContext
{
    uint32_t data[FFX_VRS_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxVrsContext;

/// Create a FidelityFX Variable Shading context from the parameters
/// programmed to the <c><i>FfxVrsContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the Variable
/// Shading API, and is responsible for the management of the internal resources
/// used by the VRS algorithm. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>backendInterface</i></c>
/// structure. This backend will attempt to retrieve the device capabilities,
/// and create the internal resources, and pipelines required by VRS to function.
/// Depending on the precise configuration used when
/// creating the <c><i>FfxVrsContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxVrsContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or Variable
/// Shading is disabled by a user. To destroy the VRS context you
/// should call <c><i>ffxVrsContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxVrsContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxVrsContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxVrsContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxVrs
FFX_API FfxErrorCode ffxVrsContextCreate(FfxVrsContext* pContext, const FfxVrsContextDescription* pContextDescription);

/// Dispatches work to the FidelityFX VRS context
///
/// @param [in] pContext                A pointer to a <c><i>FfxVrsContext</i></c> structure to populate.
/// @param [in] pDispatchDescription    A pointer to a <c><i>FfxVrsDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxVrs
FFX_API FfxErrorCode ffxVrsContextDispatch(FfxVrsContext* pContext, const FfxVrsDispatchDescription* pDispatchDescription);

/// Destroy the FidelityFX VRS context.
///
/// @param [in] pContext                A pointer to a <c><i>FfxVrsContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup FfxVrs
FFX_API FfxErrorCode ffxVrsContextDestroy(FfxVrsContext* pContext);

/// A helper function to calculate the ShadingRateImage size from a target
/// resolution.
///
///
/// @param [out] pImageWidth                A pointer to a <c>uint32_t</c> which will hold the calculated image width.
/// @param [out] pImageHeight               A pointer to a <c>uint32_t</c> which will hold the calculated image height.
/// @param [in] renderWidth                The render resolution width.
/// @param [in] renderHeight               The render resolution height.
/// @param [in] shadingRateImageTileSize   The ShadingRateImage tile size.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_INVALID_POINTER           Either <c><i>imageWidth</i></c> or <c><i>imageHeight</i></c> was <c>NULL</c>.
///
/// @ingroup FfxVrs
FFX_API FfxErrorCode ffxVrsGetImageSizeFromeRenderResolution(uint32_t* pImageWidth, uint32_t* pImageHeight, uint32_t renderWidth, uint32_t renderHeight, uint32_t shadingRateImageTileSize);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup FfxVrs
FFX_API FfxVersionNumber ffxVrsGetEffectVersion();

#if defined(__cplusplus)
}
#endif  // #if defined(__cplusplus)
