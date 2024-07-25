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

#ifndef FFX_SHADOWS_DENOISER_RESOURCES_H
#define FFX_SHADOWS_DENOISER_RESOURCES_H

#if defined(FFX_CPU) || defined(FFX_GPU)
#define FFX_DENOISER_RESOURCE_IDENTIFIER_NULL                               0
#define FFX_DENOISER_RESOURCE_IDENTIFIER_HIT_MASK_RESULTS                   1   

#define FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_BUFFER                        2   
#define FFX_DENOISER_RESOURCE_IDENTIFIER_SHADOW_MASK                        3
#define FFX_DENOISER_RESOURCE_IDENTIFIER_RAYTRACER_RESULT                   4

#define FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH                              5
#define FFX_DENOISER_RESOURCE_IDENTIFIER_VELOCITY                           6
#define FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL                             7
#define FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_FP16                        8   //same resource as NORMAL

#define FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_DEPTH                     9
#define FFX_DENOISER_RESOURCE_IDENTIFIER_TILE_META_DATA                     10

#define FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS0                           11
#define FFX_DENOISER_RESOURCE_IDENTIFIER_MOMENTS1                           12
#define FFX_DENOISER_RESOURCE_IDENTIFIER_PREVIOUS_MOMENTS                   13  //same resource as MOMENT0 & MOMENT1
#define FFX_DENOISER_RESOURCE_IDENTIFIER_CURRENT_MOMENTS                    14  //same resource as MOMENT0 & MOMENT1

#define FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH0                           15
#define FFX_DENOISER_RESOURCE_IDENTIFIER_SCRATCH1                           16
#define FFX_DENOISER_RESOURCE_IDENTIFIER_HISTORY                            17  //same resource as SCRATCH1
#define FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTION_RESULTS               18  //same resource as SCRATCH0
#define FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_INPUT                       19  //same resource as SCRATCH0 & SCRATCH1

#define FFX_DENOISER_RESOURCE_IDENTIFIER_FILTER_OUTPUT                      20  

#define FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_DEPTH_HIERARCHY              21
#define FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS               22
#define FFX_DENOISER_RESOURCE_IDENTIFIER_INPUT_NORMAL                       23
#define FFX_DENOISER_RESOURCE_IDENTIFIER_OUTPUT                             24
#define FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE                           25
#define FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_HISTORY                   26
#define FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE                           27
#define FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT                       28
#define FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE                   29
#define FFX_DENOISER_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST                 10
#define FFX_DENOISER_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS                31
#define FFX_DENOISER_RESOURCE_IDENTIFIER_DEPTH_HISTORY                      32
#define FFX_DENOISER_RESOURCE_IDENTIFIER_NORMAL_HISTORY                     33
#define FFX_DENOISER_RESOURCE_IDENTIFIER_ROUGHNESS_HISTORY                  34
#define FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_0                         35
#define FFX_DENOISER_RESOURCE_IDENTIFIER_RADIANCE_1                         36
#define FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_0                         37
#define FFX_DENOISER_RESOURCE_IDENTIFIER_VARIANCE_1                         38
#define FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_0                     39
#define FFX_DENOISER_RESOURCE_IDENTIFIER_SAMPLE_COUNT_1                     40
#define FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_0                 41
#define FFX_DENOISER_RESOURCE_IDENTIFIER_AVERAGE_RADIANCE_1                 42
#define FFX_DENOISER_RESOURCE_IDENTIFIER_REPROJECTED_RADIANCE               43
#define FFX_DENOISER_RESOURCE_IDENTIFIER_INDIRECT_ARGS                      44

#define FFX_DENOISER_RESOURCE_IDENTIFIER_COUNT                              45

// CBV resource definitions
#define FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS0            0
#define FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS1            1
#define FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS2            2
#define FFX_DENOISER_SHADOWS_CONSTANTBUFFER_IDENTIFIER_DENOISER_SHADOWS_COUNT       3

// CBV resource definitions
#define FFX_DENOISER_REFLECTIONS_CONSTANTBUFFER_IDENTIFIER                          0
#define FFX_DENOISER_REFLECTIONS_CONSTANTBUFFER_IDENTIFIER_COUNT                    1

#endif  // #if defined(FFX_CPU) || defined(FFX_GPU)

#endif  // FFX_SHADOWS_DENOISER_RESOURCES_H
