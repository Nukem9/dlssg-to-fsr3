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

/// @defgroup ffxBlur FidelityFX Blur
/// FidelityFX Blur runtime library
///
/// @ingroup SDKComponents

//------------------------------------------------------------------------------------------------------------------------------
//
// ABOUT
// =====
// AMD FidelityFX Blur is a collection of blurring effects implemented on compute shaders, hand-optimized for maximum performance.
// FFX-Blur includes
// - Gaussian Blur w/ large kernel support (up to 21x21)
//
//==============================================================================================================================

#pragma once

#include <FidelityFX/host/ffx_interface.h>

/// FidelityFX Blur major version.
///
/// @ingroup ffxBlur
#define FFX_BLUR_VERSION_MAJOR (1)

/// FidelityFX Blur minor version.
///
/// @ingroup ffxBlur
#define FFX_BLUR_VERSION_MINOR (1)

/// FidelityFX Blur patch version.
///
/// @ingroup ffxBlur
#define FFX_BLUR_VERSION_PATCH (0)

/// FidelityFX Blur context count
/// 
/// Defines the number of internal effect contexts required by Blur
///
/// @ingroup ffxBlur
#define FFX_BLUR_CONTEXT_COUNT   1

/// The size of the context specified in uint32_t units.
///
/// @ingroup ffxBlur
#define FFX_BLUR_CONTEXT_SIZE (1024)

#if defined(__cplusplus)
extern "C" {
#endif  // #if defined(__cplusplus)

/// Enum to specify which blur pass (currently only one).
///
/// @ingroup ffxBlur
typedef enum FfxBlurPass
{
    FFX_BLUR_PASS_BLUR = 0,  ///< A pass which which blurs the input
    FFX_BLUR_PASS_COUNT  ///< The number of passes in the Blur effect
} FfxBlurPass;

/// Use this macro for FfxBlurContextDescription::kernelSizes to enable all kernel sizes.
///
/// @ingroup ffxBlur
#define FFX_BLUR_KERNEL_SIZE_ALL ((1 << FFX_BLUR_KERNEL_SIZE_COUNT) - 1)

typedef enum FfxBlurKernelPermutation
{
    FFX_BLUR_KERNEL_PERMUTATION_0 = (1 << 0), ///< Sigma value of 1.6 used for generation of Gaussian kernel.
    FFX_BLUR_KERNEL_PERMUTATION_1 = (1 << 1), ///< Sigma value of 2.8 used for generation of Gaussian kernel.
    FFX_BLUR_KERNEL_PERMUTATION_2 = (1 << 2), ///< Sigma value of 4.0 used for generation of Gaussian kernel.
} FfxBlurKernelPermutation;
#define FFX_BLUR_KERNEL_PERMUTATION_COUNT 3

/// Use this macro for FfxBlurContextDescription::sigmaPermutations to enable all sigma permutations.
///
/// @ingroup ffxBlur
#define FFX_BLUR_KERNEL_PERMUTATIONS_ALL ((1 << FFX_BLUR_KERNEL_PERMUTATION_COUNT) - 1)

/// Enum to specify the size of the blur kernel. Use logical OR to enable multiple kernels
/// when setting the FfxBlurContextDescription::kernelSizes parameter prior to calling
/// ffxBlurContextCreate.
///
/// @ingroup ffxBlur
typedef enum FfxBlurKernelSize
{
    FFX_BLUR_KERNEL_SIZE_3x3   = (1 << 0),
    FFX_BLUR_KERNEL_SIZE_5x5   = (1 << 1),
    FFX_BLUR_KERNEL_SIZE_7x7   = (1 << 2),
    FFX_BLUR_KERNEL_SIZE_9x9   = (1 << 3),
    FFX_BLUR_KERNEL_SIZE_11x11 = (1 << 4),
    FFX_BLUR_KERNEL_SIZE_13x13 = (1 << 5),
    FFX_BLUR_KERNEL_SIZE_15x15 = (1 << 6),
    FFX_BLUR_KERNEL_SIZE_17x17 = (1 << 7),
    FFX_BLUR_KERNEL_SIZE_19x19 = (1 << 8),
    FFX_BLUR_KERNEL_SIZE_21x21 = (1 << 9)
} FfxBlurKernelSize;
#define FFX_BLUR_KERNEL_SIZE_COUNT 10

/// Enum to specify whether to initialize the FP32 or FP16 bit permutation of the blur shader(s).
/// Use this when setting the FfxBlurContextDescription::floatPrecision parameter prior to calling
/// ffxBlurContextCreate.
///
/// @ingroup ffxBlur
typedef enum FfxBlurFloatPrecision
{
    FFX_BLUR_FLOAT_PRECISION_32BIT = 0,
    FFX_BLUR_FLOAT_PRECISION_16BIT = 1,
    FFX_BLUR_FLOAT_PRECISION_COUNT = 2
} FfxBlurFloatPrecision;

typedef uint32_t FfxBlurKernelPermutations;
typedef uint32_t FfxBlurKernelSizes;

/// FfxBlurContextDescription struct is used to create/initialize an FfxBlurContext.
///
/// @ingroup ffxBlur
typedef struct FfxBlurContextDescription
{
    FfxBlurKernelPermutations   kernelPermutations;     ///< A bit mask of FfxBlurKernelPermutation values to indicate which kernels to enable for use.
    FfxBlurKernelSizes          kernelSizes;            ///< A bit mask of FfxBlurKernelSize values to indicated which kernel sizes to enable for use.
    FfxBlurFloatPrecision       floatPrecision;         ///< A flag indicating the desired floating point precision for use in ffxBlurContextDispatch
    FfxInterface                backendInterface;       ///< A set of pointers to the backend implementation for FidelityFX.
} FfxBlurContextDescription;

/// FfxBlurContext must be created via ffxBlurContextCreate to use the FFX Blur effect.
///
/// @ingroup ffxBlur
typedef struct FfxBlurContext
{
    uint32_t data[FFX_BLUR_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxBlurContext;

/// Create and initialize the FfxBlurContext.
///
/// @param [out] pContext             The FfxBlurContext to create and initialize.
/// @param [in] pContextDescription   The initialization configuration parameters.
///
/// @ingroup ffxBlur
FFX_API FfxErrorCode ffxBlurContextCreate(FfxBlurContext* pContext, const FfxBlurContextDescription* pContextDescription);

/// Destroy and free resources associated with the FfxBlurContext.
///
/// @param [inout] pContext           The FfxBlurContext to destroy.
///
/// @ingroup ffxBlur
FFX_API FfxErrorCode ffxBlurContextDestroy(FfxBlurContext* pContext);

/// FfxBlurDispatchDescription struct defines configuration of a blur dispatch (see ffxBlurContextDispatch).
///
/// @ingroup ffxBlur
typedef struct FfxBlurDispatchDescription
{
    FfxCommandList           commandList;        ///< The <c><i>FfxCommandList</i></c> to record rendering commands into.
    FfxBlurKernelPermutation kernelPermutation;  ///< The permutation of the kernel (must be the one specified in FfxBlurContextDescription::kernelPermutations).
    FfxBlurKernelSize        kernelSize;         ///< The kernel size to use for blurring (must be one specified in FfxBlurContextDescription::kernelSizes).
    FfxDimensions2D          inputAndOutputSize; ///< The width and height in pixels of the input and output resources.
    FfxResource              input;              ///< The <c><i>FfxResource</i></c> to blur.
    FfxResource              output;             ///< The <c><i>FfxResource</i></c> containing the output buffer for the blurred output.
} FfxBlurDispatchDescription;

/// Create and initialize the FfxBlurContext.
///
/// @param [in] pContext             The FfxBlurContext to use for the dispatch.
/// @param [in] pDispatchDescription The dispatch configuration parameters (see FfxBlurDispatchDescription).
///
/// @ingroup ffxBlur
FFX_API FfxErrorCode ffxBlurContextDispatch(FfxBlurContext* pContext, const FfxBlurDispatchDescription* pDispatchDescription);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxBlur
FFX_API FfxVersionNumber ffxBlurGetEffectVersion();

#if defined(__cplusplus)
}
#endif  // #if defined(__cplusplus
