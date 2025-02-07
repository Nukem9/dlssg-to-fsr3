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

// @defgroup DOF

#pragma once

// Include the interface for the backend of the FSR2 API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup ffxDof FidelityFX DoF
/// FidelityFX Depth of Field runtime library
/// 
/// @ingroup SDKComponents


/// FidelityFX DOF major version.
///
/// @ingroup ffxDof
#define FFX_DOF_VERSION_MAJOR (1)

/// FidelityFX DOF minor version.
///
/// @ingroup ffxDof
#define FFX_DOF_VERSION_MINOR (1)

/// FidelityFX DOF patch version.
///
/// @ingroup ffxDof
#define FFX_DOF_VERSION_PATCH (0)

/// FidelityFX DOF context count
/// 
/// Defines the number of internal effect contexts required by DOF
///
/// @ingroup ffxDof
#define FFX_DOF_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup ffxDof
#define FFX_DOF_CONTEXT_SIZE  (45674)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// An enumeration of the passes which constitute the DoF algorithm.
///
/// DOF is implemented as a composite of several compute passes each
/// computing a key part of the final result. Each call to the
/// <c><i>FfxDofScheduleGpuJobFunc</i></c> callback function will
/// correspond to a single pass included in <c><i>FfxDofPass</i></c>. For a
/// more comprehensive description of each pass, please refer to the DoF
/// reference documentation.
///
/// @ingroup ffxDof
typedef enum FfxDofPass {
    FFX_DOF_PASS_DOWNSAMPLE_DEPTH = 0,  ///< A pass which downsamples the depth buffer
    FFX_DOF_PASS_DOWNSAMPLE_COLOR = 1,  ///< A pass which downsamples the color buffer
    FFX_DOF_PASS_DILATE = 2,            ///< A pass which dilates the depth tile buffer
    FFX_DOF_PASS_BLUR = 3,              ///< A pass which performs the depth of field blur
    FFX_DOF_PASS_COMPOSITE = 4,         ///< A pass which combines the blurred images with the sharp input
    FFX_DOF_PASS_COUNT              ///< The number of passes in DOF
} FfxDofPass;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxDofContext</i></c>. See <c><i>FfxDofContextDescription</i></c>.
/// Curently, no flags exist.
///
/// @ingroup ffxDof
typedef enum FfxDofInitializationFlagBits {
    FFX_DOF_REVERSE_DEPTH      = (1 << 0), ///< A bit indicating whether input depth is reversed (1 is closest)
    FFX_DOF_OUTPUT_PRE_INIT    = (1 << 1), ///< A bit indicating whether the output is pre-initialized with the input color (e.g. it is the same texture)
    FFX_DOF_DISABLE_RING_MERGE = (1 << 2), ///< A bit indicating whether to disable merging kernel rings
} FfxDofInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Depth of Field.
///
/// @ingroup ffxDof
typedef struct FfxDofContextDescription {
    uint32_t                    flags;                              ///< A collection of @ref FfxDofInitializationFlagBits
    uint32_t                    quality;                            ///< The number of rings to be used in the DoF blur kernel
    FfxDimensions2D             resolution;                         ///< Resolution of the input and output textures
    FfxInterface                backendInterface;                   ///< A set of pointers to the backend implementation for FidelityFX
    float                       cocLimitFactor;                     ///< The limit to apply to circle of confusion size as a factor for resolution height
} FfxDofContextDescription;

/// A structure encapsulating the parameters for dispatching
/// of FidelityFX Depth of Field
///
/// @ingroup ffxDof
typedef struct FfxDofDispatchDescription {
    FfxCommandList              commandList;        ///< The <c><i>FfxCommandList</i></c> to record DoF rendering commands into.
    FfxResource                 color;              ///< The <c><i>FfxResource</i></c> containing color information
    FfxResource                 depth;              ///< The <c><i>FfxResource</i></c> containing depth information
    FfxResource                 output;             ///< The <c><i>FfxResource</i></c> to output into. Can be the same as the color input.
    float                       cocScale;           ///< The factor converting depth to circle of confusion size. Can be calculated using ffxDofCalculateCocScale.
    float                       cocBias;            ///< The bias to apply to circle of confusion size. Can be calculated using ffxDofCalculateCocBias.
} FfxDofDispatchDescription;

/// A structure encapsulating the FidelityFX Depth of Field context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by DoF.
///
/// The <c><i>FfxDofContext</i></c> object should have a lifetime matching
/// your use of DoF. Before destroying the DoF context care should be taken
/// to ensure the GPU is not accessing the resources created or used by DoF.
/// It is therefore recommended that the GPU is idle before destroying the
/// DoF context.
///
/// @ingroup ffxDof
typedef struct FfxDofContext {
    uint32_t                    data[FFX_DOF_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxDofContext;

/// Create a FidelityFX Depth of Field context from the parameters
/// programmed to the <c><i>FfxDofContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the Depth of
/// Field API, and is responsible for the management of the internal resources
/// used by the DoF algorithm. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>backendInterface</i></c>
/// structure. This backend will attempt to retrieve the device capabilities,
/// and create the internal resources, and pipelines required by DoF to function.
/// Depending on the precise configuration used when
/// creating the <c><i>FfxDofContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxDofContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or DoF
/// is disabled by a user. To destroy the DoF context you
/// should call <c><i>ffxDofContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxDofContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxDofContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxDofContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxDof
FFX_API FfxErrorCode ffxDofContextCreate(FfxDofContext* pContext, const FfxDofContextDescription* pContextDescription);

/// Dispatches work to the FidelityFX DoF context
///
/// @param [in] pContext                 A pointer to a <c><i>FfxDofContext</i></c> structure.
/// @param [in] pDispatchDescription     A pointer to a <c><i>FfxDofDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxDof
FFX_API FfxErrorCode ffxDofContextDispatch(FfxDofContext* pContext, const FfxDofDispatchDescription* pDispatchDescription);

/// Destroy the FidelityFX DoF context.
///
/// @param [out] pContext                A pointer to a <c><i>FfxDofContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxDof
FFX_API FfxErrorCode ffxDofContextDestroy(FfxDofContext* pContext);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxDof
FFX_API FfxVersionNumber ffxDofGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)


/// Calculates the scale parameter in the thin lens model according to lens and projection parameters.
/// @param aperture    Aperture radius in view space units
/// @param focus       Distance to focus plane in view space units
/// @param focalLength Lens focal length in view space units
/// @param conversion  Conversion factor for view units to pixels (i.e. image width in pixels / sensor size)
/// @param proj33      Element (3,3) of the projection matrix (z range scale)
/// @param proj34      Element (3,4) of the projection matrix (z range offset)
/// @param proj43      Element (4,3) of the projection matrix (typically 1 or -1)
/// 
/// @ingroup ffxDof
static inline float ffxDofCalculateCocScale(float aperture, float focus, float focalLength, float conversion,
	float proj33, float proj34, float proj43)
{
    (void)proj33;
    // z = (vd * proj33 + proj34) / (vd * proj43) = proj33/proj43 + proj34/(vd*proj43)
	// => view depth = proj34 / (z*proj43 - proj33)
	// C = (A * L * (F - D)) / (D * (F - L))
	//   = AL/(F-L) * (F/D - 1)
	//   = AL/(F-L) * (F(z*p43-p33)/p34 - 1) ignore non-factor terms
	//   = AL/(F-L) * (F(z*p43-.)/p34 - .)
	//   = AL/(F-L) * (F(z*p43-.)/p34 - .)
	//   = AL/(F-L) * F*z*p43/p34 + .
	//   = z * AL/(F-L) * F*p43/p34 + .
    float absFocus  = focus < 0 ? -focus : focus;
    float commonFactor = conversion * aperture * focalLength / (absFocus - focalLength);
    return commonFactor * focus * (proj43 / proj34);
}

/// Calculates the bias parameter in the thin lens model according to lens and projection parameters.
/// @param aperture    Aperture radius in view space units
/// @param focus       Distance to focus plane in view space units
/// @param focalLength Lens focal length in view space units
/// @param conversion  Conversion factor for view units to pixels (i.e. image width in pixels / sensor size)
/// @param proj33      Element (3,3) of the projection matrix (a.k.a. z range scale)
/// @param proj34      Element (3,4) of the projection matrix (a.k.a. z range offset)
/// @param proj43      Element (4,3) of the projection matrix (typically 1 or -1)
/// 
/// @ingroup ffxDof
static inline float ffxDofCalculateCocBias(float aperture, float focus, float focalLength, float conversion,
	float proj33, float proj34, float proj43)
{
    (void)proj43;
    // z = (D * proj33 + proj34) / (D * proj43) = proj33/proj43 + proj34/(D*proj43)
	// => view depth D = proj34 / (z*proj43 - proj33)
	// C = (A * L * (F - D)) / (D * (F - L))
	//   = AL/(F-L) * (F/D - 1)
	//   = AL/(F-L) * (F(z*p43-p33)/p34 - 1) ignore factor terms
	//   = AL/(F-L) * (F(0*p43-p33)/p34 - 1)
	//   = AL/(F-L) * (-F*p33/p34 - 1)
    float absFocus  = focus < 0 ? -focus : focus;
    float commonFactor = conversion * aperture * focalLength / (absFocus - focalLength);
    return commonFactor * (-focus * (proj33 / proj34) - 1);
}
