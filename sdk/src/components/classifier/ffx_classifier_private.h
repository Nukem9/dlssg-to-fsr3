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
#include <FidelityFX/gpu/classifier/ffx_classifier_resources.h>

/// An enumeration of all the permutations that can be passed to the DoF algorithm.
///
/// DoF features are organized through a set of pre-defined compile
/// permutation options that need to be specified. Which shader blob
/// is returned for pipeline creation will be determined by what combination
/// of shader permutations are enabled.
///
/// @ingroup CLASSIFIER
typedef enum ClassifierShaderPermutationOptions
{
    CLASSIFIER_SHADER_PERMUTATION_FORCE_WAVE64             = (1 << 1),  ///< doesn't map to a define, selects different table
    CLASSIFIER_SHADER_PERMUTATION_ALLOW_FP16               = (1 << 2),  ///< Enables fast math computations where possible
    CLASSIFIER_SHADER_PERMUTATION_DEPTH_INVERTED           = (1 << 3),  ///< Indicates input resources were generated with inverted depth
    CLASSIFIER_SHADER_PERMUTATION_CLASSIFY_BY_NORMALS      = (1 << 4),  ///< Perform classification of tiles only using normals
    CLASSIFIER_SHADER_PERMUTATION_CLASSIFY_BY_CASCADES     = (1 << 5),  ///< Perform classification of tiles using normals and shadow maps
} ClassifierShaderPermutationOptions;

// Constants for the Classifier dispatch. Must be kept in sync with cbClassifier in ffx_classifier_callbacks_hlsl.h
typedef struct ClassifierConstants
{
    FfxFloat32x4 textureSize;
    FfxFloat32x3 lightDir;
    float skyHeight;

    FfxFloat32x4 blockerOffset_cascadeSize_sunSizeLightSpace_pad;
    FfxUInt32x4 cascadeCount_tileTolerance_pad_pad;
    FfxFloat32x4  bRejectLitPixels_bUseCascadesForRayT_normalsUnpackMul_unpackAdd;

    FfxFloat32x4 cascadeScale[4];
    FfxFloat32x4 cascadeOffset[4];

    // Matrices
    FfxFloat32 viewToWorld[16];
    FfxFloat32 lightView[16];
    FfxFloat32 inverseLightView[16];
} ClassifierConstants;

typedef struct ClassifierReflectionsConstants
{
    float           invViewProjection[16];
    float           projection[16];
    float           invProjection[16];
    float           view[16];
    float           invView[16];
    float           prevViewProjection[16];
    uint32_t        renderSize[2];
    float           inverseRenderSize[2];
    float           iblFactor;
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
    uint32_t        isRoughnessPerceptual;

} ClassifierReflectionsConstants;


struct FfxClassifierContextDescription;
struct FfxDeviceCapabilities;
struct FfxPipelineState;
struct FfxResource;

// FfxClassifierContext_Private
// The private implementation of the Classifier context.
typedef struct FfxClassifierContext_Private {

    FfxClassifierContextDescription contextDescription;
    FfxUInt32                       effectContextId;

    FfxConstantBuffer reflectionsConstants;
    FfxConstantBuffer classifierConstants;

    FfxDevice             device;
    FfxDeviceCapabilities deviceCapabilities;

    FfxPipelineState shadowClassifierPipeline;
    FfxPipelineState reflectionsClassifierPipeline;

    FfxResourceInternal srvResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_COUNT];
    FfxResourceInternal uavResources[FFX_CLASSIFIER_RESOURCE_IDENTIFIER_COUNT];

} FfxClassifierContext_Private;
