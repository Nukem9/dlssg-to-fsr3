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

// @defgroup Classifier

#pragma once
#define FFX_CLASSIFIER_MAX_SHADOW_MAP_TEXTURES_COUNT 4

// Include the interface for the backend of the FSR2 API.
#include <FidelityFX/host/ffx_interface.h>

/// @defgroup ffxClassifier FidelityFX Classifier
/// FidelityFX Classifier runtime library
///
/// @ingroup SDKComponents


/// FidelityFX Classifier major version.
///
/// @ingroup ffxClassifier
#define FFX_CLASSIFIER_VERSION_MAJOR (1)

/// FidelityFX Classifier minor version.
///
/// @ingroup ffxClassifier
#define FFX_CLASSIFIER_VERSION_MINOR (3)

/// FidelityFX Classifier patch version.
///
/// @ingroup ffxClassifier
#define FFX_CLASSIFIER_VERSION_PATCH (0)

/// FidelityFX Classifier context count
///
/// Defines the number of internal effect contexts required by Classifier
///
/// @ingroup ffxClassifier
#define FFX_CLASSIFIER_CONTEXT_COUNT   1

/// The size of the context specified in 32bit values.
///
/// @ingroup ffxClassifier
#define FFX_CLASSIFIER_CONTEXT_SIZE  (18500)

#if defined(__cplusplus)
extern "C" {
#endif // #if defined(__cplusplus)

/// Enum to specify which blur pass (currently only one).
///
/// @ffxClassifier
typedef enum FfxClassifierPass
{
    FFX_CLASSIFIER_SHADOW_PASS_CLASSIFIER  = 0,  ///< The Tile Classification Pass
    FFX_CLASSIFIER_REFLECTION_PASS_TILE_CLASSIFIER = 1,  ///< Reflections tile classification pass
    FFX_CLASSIFIER_PASS_COUNT  ///< The number of passes in the Classifier effect
} FfxClassifierPass;

/// An enumeration of bit flags used when creating a
/// <c><i>FfxClassifierContext</i></c>. See <c><i>FfxClassifierContextDescription</i></c>.
/// Currently, no flags exist.
///
/// @ingroup ffxClassifier
typedef enum FfxClassifierInitializationFlagBits {
    FFX_CLASSIFIER_SHADOW                = (1 << 0),  ///< A bit indicating the intent is to classify shadows
    FFX_CLASSIFIER_CLASSIFY_BY_NORMALS   = (1 << 1),  ///< A bit indicating the intent is to classify by normals
    FFX_CLASSIFIER_CLASSIFY_BY_CASCADES  = (1 << 2),  ///< A bit indicating the intent is to classify by cascades
    FFX_CLASSIFIER_ENABLE_DEPTH_INVERTED = (1 << 3),  ///< A bit indicating that the input depth buffer data provided is inverted [1..0].
    FFX_CLASSIFIER_REFLECTION            = (1 << 4),  ///< A bit indicating the intent is to classify reflections
} FfxClassifierInitializationFlagBits;

/// A structure encapsulating the parameters required to initialize FidelityFX
/// Classifier.
///
/// @ingroup ffxClassifier
typedef struct FfxClassifierContextDescription {
    uint32_t                    flags;                              ///< A collection of @ref FfxClassifierInitializationFlagBits
    FfxDimensions2D             resolution;                         ///< Resolution of the shadow dispatch call
    FfxInterface                backendInterface;                   ///< A set of pointers to the backend implementation for FidelityFX Classifier
} FfxClassifierContextDescription;

/// A structure encapsulating the parameters for dispatching
/// of FidelityFX Classifier for shadows
///
/// @ingroup ffxClassifier
typedef struct FfxClassifierShadowDispatchDescription {
    FfxCommandList              commandList;        ///< The <c><i>FfxCommandList</i></c> to record Classifier rendering commands into.
    FfxResource                 depth;              ///< The <c><i>FfxResource</i></c> (SRV Texture 0)containing depth information
    FfxResource                 normals;            ///< The <c><i>FfxResource</i></c> (SRV Texture 1)containing normals information
    FfxResource shadowMaps[FFX_CLASSIFIER_MAX_SHADOW_MAP_TEXTURES_COUNT];  ///< The <c><i>FfxResource</i></c> (SRV Texture 2)containing shadowMap(s) information
    FfxResource                 workQueue;          ///< The <c><i>FfxResource</i></c> (UAV Buffer 0)Work Queue: rwsb_tiles.
    FfxResource                 workQueueCount;     ///< The <c><i>FfxResource</i></c> (UAV Buffer 1)Work Queue Counter: rwb_tileCount
    FfxResource                 rayHitTexture;      ///< The <c><i>FfxResource</i></c> (UAV Texture 0)Ray Hit Texture

    FfxFloat32 normalsUnPackMul;  		    ///< A multiply factor to transform the normal to the space expected by the Classifier
    FfxFloat32 normalsUnPackAdd;  		    ///< An offset to transform the normal to the space expected by the Classifier

    // Constant Data
    FfxFloat32x3 lightDir; 			    ///< The light direction
    FfxFloat32   sunSizeLightSpace; 		    ///< The sun size
    FfxUInt32    tileCutOff; 			    ///< The tile cutoff

    FfxBoolean   bRejectLitPixels; 		    ///< UI Setting, selects wheter to reject lit pixels in the shadows maps
    FfxUInt32    cascadeCount; 			    ///< The number of cascades
    FfxFloat32   blockerOffset; 		    ///< UI Setting, the blocker offsett

    FfxBoolean   bUseCascadesForRayT; 		    ///< UI Setting, selects whether to use the classifier to save ray intervals
    FfxFloat32   cascadeSize; 			    ///< The cascade size

    FfxFloat32x4 cascadeScale[4];  		    ///< A multiply factor for each cascade
    FfxFloat32x4 cascadeOffset[4]; 		    ///< An offeset factor for each cascade

    // Matrices
    FfxFloat32 viewToWorld[16];
    FfxFloat32 lightView[16];
    FfxFloat32 inverseLightView[16];

} FfxClassifierShadowDispatchDescription;

/// A structure encapsulating the parameters for dispatching
/// of FidelityFX Classifier for reflections
///
/// @ingroup ffxClassifier
typedef struct FfxClassifierReflectionDispatchDescription {
    FfxCommandList  commandList;        ///< The <c><i>FfxCommandList</i></c> to record Hybrid Reflections rendering commands into.
    FfxResource     depth;              ///< A <c><i>FfxResource</i></c> containing the depth buffer for the current frame (at render resolution).
    FfxResource     motionVectors;      ///< A <c><i>FfxResource</i></c> containing the motion vectors buffer for the current frame (at render resolution).
    FfxResource     normal;             ///< A <c><i>FfxResource</i></c> containing the normal buffer for the current frame (at render resolution).
    FfxResource     materialParameters; ///< A <c><i>FfxResource</i></c> containing the aoRoughnessMetallic buffer for the current frame (at render resolution).
    FfxResource     environmentMap;     ///< A <c><i>FfxResource</i></c> containing the environment map to fallback to when screenspace data is not sufficient
    FfxResource     radiance;
    FfxResource     varianceHistory;
    FfxResource     hitCounter;
    FfxResource     hitCounterHistory;
    FfxResource     rayList;
    FfxResource     rayListHW;
    FfxResource     extractedRoughness;
    FfxResource     rayCounter;
    FfxResource     denoiserTileList;
    FfxDimensions2D renderSize;              ///< The resolution that was used for rendering the input resources.
    float           invViewProjection[16];   ///< An array containing the inverse of the view projection matrix in column major layout.
    float           projection[16];          ///< An array containing the projection matrix in column major layout.
    float           invProjection[16];       ///< An array containing the inverse of the projection matrix in column major layout.
    float           view[16];                ///< An array containing the view matrix in column major layout.
    float           invView[16];             ///< An array containing the inverse of the view matrix in column major layout.
    float           prevViewProjection[16];  ///< An array containing the previous frame's view projection matrix in column major layout.
    float           iblFactor;               ///< A factor to control the intensity of the image based lighting. Set to 1 for an HDR probe.
    uint32_t        frameIndex;
    uint32_t        samplesPerQuad;
    uint32_t        temporalVarianceGuidedTracingEnabled;
    float           globalRoughnessThreshold;
    float           rtRoughnessThreshold;
    uint32_t        mask;
    uint32_t        reflectionWidth;
    uint32_t        reflectionHeight;
    float           hybridMissWeight;
    float           hybridSpawnRate;
    float           vrtVarianceThreshold;
    float           reflectionsBackfacingThreshold;
    uint32_t        randomSamplesPerPixel;
    float           motionVectorScale[2];
    float           normalsUnpackMul;
    float           normalsUnpackAdd;
    uint32_t        roughnessChannel;
    bool            isRoughnessPerceptual;
} FfxClassifierReflectionDispatchDescription;

/// A structure encapsulating the FidelityFX Classifier context.
///
/// This sets up an object which contains all persistent internal data and
/// resources that are required by the Classifier.
///
/// The <c><i>FfxClassifierContext</i></c> object should have a lifetime matching
/// your use of the Classifier. Before destroying the Classifier context care should be taken
/// to ensure the GPU is not accessing the resources created or used by the Classifier.
/// It is therefore recommended that the GPU is idle before destroying the
/// Classifier context.
///
/// @ingroup ffxClassifier
typedef struct FfxClassifierContext {
    uint32_t                    data[FFX_CLASSIFIER_CONTEXT_SIZE];  ///< An opaque set of <c>uint32_t</c> which contain the data for the context.
} FfxClassifierContext;

/// Create a FidelityFX Classifier context from the parameters
/// programmed to the <c><i>FfxClassifierContextDescription</i></c> structure.
///
/// The context structure is the main object used to interact with the Classifier
/// API, and is responsible for the management of the internal resources
/// used by the Classifier. When this API is called, multiple calls
/// will be made via the pointers contained in the <c><i>backendInterface</i></c>
/// structure. This backend will attempt to retrieve the device capabilities,
/// and create the internal resources, and pipelines required by the Classifier to function.
/// Depending on the precise configuration used when
/// creating the <c><i>FfxClassifierContext</i></c> a different set of resources and
/// pipelines might be requested via the callback functions.
///
/// The <c><i>FfxClassifierContext</i></c> should be destroyed when use of it is
/// completed, typically when an application is unloaded or the Classifier
/// is disabled by a user. To destroy the Classifier context you
/// should call <c><i>FfxClassifierContextDestroy</i></c>.
///
/// @param [out] pContext                A pointer to a <c><i>FfxClassifierContext</i></c> structure to populate.
/// @param [in]  pContextDescription     A pointer to a <c><i>FfxClassifierContextDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>contextDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_INCOMPLETE_INTERFACE      The operation failed because the <c><i>FfxClassifierContextDescription.callbacks</i></c>  was not fully specified.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxClassifier
FFX_API FfxErrorCode ffxClassifierContextCreate(FfxClassifierContext* pContext, const FfxClassifierContextDescription* pContextDescription);

/// Dispatches work to the FidelityFX Classifier context for shadows
///
/// @param [in] pContext                 A pointer to a <c><i>FfxClassifierContext</i></c> structure.
/// @param [in] pDispatchDescription     A pointer to a <c><i>FfxClassifierShadowDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxClassifier
FFX_API FfxErrorCode ffxClassifierContextShadowDispatch(FfxClassifierContext* pContext, const FfxClassifierShadowDispatchDescription* pDispatchDescription);

/// Dispatches work to the FidelityFX Classifier context for reflections
///
/// @param [in] pContext                 A pointer to a <c><i>FfxClassifierContext</i></c> structure.
/// @param [in] pDispatchDescription     A pointer to a <c><i>FfxClassifierReflectionDispatchDescription</i></c> structure.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because either <c><i>context</i></c> or <c><i>dispatchDescription</i></c> was <c><i>NULL</i></c>.
/// @retval
/// FFX_ERROR_BACKEND_API_ERROR         The operation failed because of an error returned from the backend.
///
/// @ingroup ffxClassifier
FFX_API FfxErrorCode ffxClassifierContextReflectionDispatch(FfxClassifierContext* pContext, const FfxClassifierReflectionDispatchDescription* pDispatchDescription);

/// Destroy the FidelityFX Classifier context.
///
/// @param [out] pContext               A pointer to a <c><i>FfxClassifierContext</i></c> structure to destroy.
///
/// @retval
/// FFX_OK                              The operation completed successfully.
/// @retval
/// FFX_ERROR_CODE_NULL_POINTER         The operation failed because <c><i>context</i></c> was <c><i>NULL</i></c>.
///
/// @ingroup ffxClassifier
FFX_API FfxErrorCode ffxClassifierContextDestroy(FfxClassifierContext* pContext);

/// Queries the effect version number.
///
/// @returns
/// The SDK version the effect was built with.
///
/// @ingroup ffxClassifier
FFX_API FfxVersionNumber ffxClassifierGetEffectVersion();

#if defined(__cplusplus)
}
#endif // #if defined(__cplusplus)
