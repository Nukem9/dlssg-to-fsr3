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

// @defgroup LPM

#pragma once

// Include the interface for the backend of the LPM 1.0 API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup FfxLpm FidelityFX LPM
/// FidelityFX Luma Preserving Mapper runtime library
/// 
/// @ingroup SDKComponents

/// FidelityFX Luma Preserving Mapper 1.3 major version.
///
/// @ingroup FfxLpm
#define FFX_LPM_VERSION_MAJOR (1)

/// FidelityFX Luma Preserving Mapper 1.3 minor version.
///
/// @ingroup FfxLpm
#define FFX_LPM_VERSION_MINOR (4)

/// FidelityFX Luma Preserving Mapper 1.3 patch version.
///
/// @ingroup FfxLpm
#define FFX_LPM_VERSION_PATCH (0)

/// FidelityFX Luma Preserving Mapper context count
/// 
/// Defines the number of internal effect contexts required by LPM
///
/// @ingroup FfxLpm
#define FFX_LPM_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup FfxLpm
#define FFX_LPM_CONTEXT_SIZE (9300)

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)

/// An enumeration of all the passes which constitute the LPM algorithm.
///
/// LPM is implemented as a composite of several compute passes each
/// computing a key part of the final result. Each call to the
/// <c><i>FfxLPMScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single pass included in <c><i>FfxLPMPass</i></c>. For a
/// more comprehensive description of each pass, please refer to the LPM
/// reference documentation.
///
/// @ingroup FfxLpm
typedef enum FfxLpmPass
{

    FFX_LPM_PASS_FILTER      = 0,  ///< A pass which filters the color buffer using LPM's tone and gamut mapping solution.

    FFX_LPM_PASS_COUNT  ///< The number of passes performed by LPM.
} FfxLpmPass;

/// An enumeration of monitor display modes supported by LPM
/// FFX_LPM_DISPLAYMODE_LDR tagets low or standard dynamic range monitor using 8bit back buffer
/// FFX_LPM_DISPLAYMODE_HDR10_2084 targets HDR10 perceptual quantizer (PQ) transfer function using 10bit backbuffer
/// FFX_LPM_DISPLAYMODE_HDR10_SCRGB targets HDR10 linear output with no transfer function using 16bit backbuffer
/// FFX_LPM_DISPLAYMODE_FSHDR_2084 targets freesync premium pro hdr thorugh PQ transfer function using 10bit backbuffer
/// FFX_LPM_DISPLAYMODE_FSHDR_SCRGB targets linear output with no transfer function using 16bit backbuffer
/// @ingroup LPM
typedef enum class FfxLpmDisplayMode
{
    FFX_LPM_DISPLAYMODE_LDR = 0,
    FFX_LPM_DISPLAYMODE_HDR10_2084  = 1,
    FFX_LPM_DISPLAYMODE_HDR10_SCRGB = 2,
    FFX_LPM_DISPLAYMODE_FSHDR_2084  = 3,
    FFX_LPM_DISPLAYMODE_FSHDR_SCRGB = 4
} FfxLpmDisplayMode;

/// An enumeration of colourspaces supported by LPM
/// FFX_LPM_ColorSpace_REC709 uses rec709 colour primaries used for FFX_LPM_DISPLAYMODE_LDR, FFX_LPM_DISPLAYMODE_HDR10_SCRGB and FFX_LPM_DISPLAYMODE_FSHDR_SCRGB modes
/// FFX_LPM_ColorSpace_P3 uses P3 colour primaries
/// FFX_LPM_ColorSpace_REC2020 uses rec2020 colour primaries used for FFX_LPM_DISPLAYMODE_HDR10_2084 and FFX_LPM_DISPLAYMODE_FSHDR_2084 modes
/// FFX_LPM_ColorSapce_Display uses custom primaries queried from display
/// @ingroup LPM
typedef enum class FfxLpmColorSpace
{
    FFX_LPM_ColorSpace_REC709  = 0,
    FFX_LPM_ColorSpace_P3      = 1,
    FFX_LPM_ColorSpace_REC2020 = 2,
    FFX_LPM_ColorSapce_Display = 3,
} FfxLpmColorSpace;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxLpmContext</i></c>. See <c><i>FfxLpmContextDescription</i></c>.
///
/// @ingroup FfxLpm
typedef enum FfxLpmInitializationFlagBits
{
} FfxLpmInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Luma Preserving Mapper
///
/// @ingroup FfxLpm
typedef struct FfxLpmContextDescription
{
    uint32_t                    flags;
    FfxInterface                backendInterface;       ///< A set of pointers to the backend implementation for LPM.
} FfxLpmContextDescription;

/// A structure encapsulating the parameters for dispatching the various passes
/// of FidelityFX Luma Preserving Mapper 1.0
///
/// @ingroup FfxLpm
typedef struct FfxLpmDispatchDescription
{
    FfxCommandList    commandList;       ///< The <c><i>FfxCommandList</i></c> to record LPM rendering commands into.
    FfxResource       inputColor;        ///< A <c><i>FfxResource</i></c> containing the color buffer for the current frame.
    FfxResource       outputColor;       ///< A <c><i>FfxResource</i></c> containing the tone and gamut mapped output color buffer for the current frame.
    bool              shoulder;
    float             softGap;
    float             hdrMax;
    float             lpmExposure;
    float             contrast;
    float             shoulderContrast;
    float             saturation[3];
    float             crosstalk[3];
    FfxLpmColorSpace  colorSpace;
    FfxLpmDisplayMode displayMode;
    float             displayRedPrimary[2];
    float             displayGreenPrimary[2];
    float             displayBluePrimary[2];
    float             displayWhitePoint[2];
    float             displayMinLuminance;
    float             displayMaxLuminance;
} FfxLpmDispatchDescription;

/// A structure encapsulating the FidelityFX Luma Preserving 1.0 context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by LPM.
///
/// The <c><i>FfxLpmContext</i></c> object should have a lifetime matching
/// your use of LPM. Before destroying the LPM context care should be taken
/// to ensure the GPU is not accessing the resources created or used by LPM.
/// It is therefore recommended that the GPU is idle before destroying the
/// LPM context.
///
/// @ingroup FfxLpm
typedef struct FfxLpmContext
{
    uint32_t data[FFX_LPM_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxLpmContext;

/// Create a FidelityFX Luma Preserving 1.0 context from the parameters
/// programmed to the <c><i>FfxLpmContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the Luma Preserving
/// Mapper 1.0 API, and is responsible for the management of the internal resources
/// used by the LPM algorithm. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>callbacks</i></c>
/// structure. These callbacks will attempt to retreive the device capabilities,
/// and create the internal resources, and pipelines required by LPM
/// frame-to-frame function. Depending on the precise configuration used when
/// creating the <c><i>FfxLpmContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxLpmContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or LPM
/// tone and gamut mapping is disabled by a user. To destroy the LPM context you
/// should call <c><i>ffxLpmContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxLpmContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxLpmContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxLpmContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxLpm
FFX_API FfxErrorCode ffxLpmContextCreate(FfxLpmContext* pContext, const FfxLpmContextDescription* pContextDescription);

/// Dispatches work to the FidelityFX LPM context
/// @param [out] pContext                A pointer to a <c><i>FfxLpmContext</i></c> structure to populate.
/// @param [in]  pDispatchDescription    A pointer to a <c><i>FfxLpmDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup FfxLpm
FFX_API FfxErrorCode ffxLpmContextDispatch(FfxLpmContext* pContext, const FfxLpmDispatchDescription* pDispatchDescription);

/// Destroy the FidelityFX LPM context.
///
/// @param [out] pContext                A pointer to a <c><i>FfxLpmContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup FfxLpm
FFX_API FfxErrorCode ffxLpmContextDestroy(FfxLpmContext* pContext);

/// Sets up the constant buffer data necessary for LPM compute
/// 
/// @param [in] incon                   
/// @param [in] insoft
/// @param [in] incon2
/// @param [in] inclip
/// @param [in] inscaleOnly
/// @param [out] outcon
/// @param [out] outsoft
/// @param [out] outcon2
/// @param [out] outclip
/// @param [out] outscaleOnly
/// 
/// @retval
/// FFX_OK                      The operation completed successfully.
/// 
/// @ingroup FfxLpm
FFX_API FfxErrorCode FfxPopulateLpmConsts(bool      incon,
                                          bool      insoft,
                                          bool      incon2,
                                          bool      inclip,
                                          bool      inscaleOnly,
                                          uint32_t& outcon,
                                          uint32_t& outsoft,
                                          uint32_t& outcon2,
                                          uint32_t& outclip,
                                          uint32_t& outscaleOnly);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup FfxLpm
FFX_API FfxVersionNumber ffxLpmGetEffectVersion();

#if defined(__cplusplus)
}
#endif  // #if defined(__cplusplus)
