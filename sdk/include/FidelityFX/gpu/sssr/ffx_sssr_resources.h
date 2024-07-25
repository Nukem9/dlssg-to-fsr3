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

#ifndef FFX_SSSR2_RESOURCES_H
#define FFX_SSSR2_RESOURCES_H

#if defined(FFX_CPU) || defined(FFX_GPU)
#define FFX_SSSR_RESOURCE_IDENTIFIER_NULL                               0
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_COLOR                        1
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_DEPTH                        2
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MOTION_VECTORS               3
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_NORMAL                       4
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_MATERIAL_PARAMETERS          5
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_ENVIRONMENT_MAP              6
#define FFX_SSSR_RESOURCE_IDENTIFIER_INPUT_BRDF_TEXTURE                 7
#define FFX_SSSR_RESOURCE_IDENTIFIER_OUTPUT                             8
#define FFX_SSSR_RESOURCE_IDENTIFIER_DEPTH_HIERARCHY                    9
#define FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE                           10
#define FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_HISTORY                   11
#define FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE                           12
#define FFX_SSSR_RESOURCE_IDENTIFIER_RAY_LIST                           13
#define FFX_SSSR_RESOURCE_IDENTIFIER_DENOISER_TILE_LIST                 14
#define FFX_SSSR_RESOURCE_IDENTIFIER_RAY_COUNTER                        15
#define FFX_SSSR_RESOURCE_IDENTIFIER_INTERSECTION_PASS_INDIRECT_ARGS    16
#define FFX_SSSR_RESOURCE_IDENTIFIER_EXTRACTED_ROUGHNESS                17
#define FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_0                         18
#define FFX_SSSR_RESOURCE_IDENTIFIER_RADIANCE_1                         19
#define FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_0                         20
#define FFX_SSSR_RESOURCE_IDENTIFIER_VARIANCE_1                         21
#define FFX_SSSR_RESOURCE_IDENTIFIER_SOBOL_BUFFER                       22
#define FFX_SSSR_RESOURCE_IDENTIFIER_RANKING_TILE_BUFFER                23
#define FFX_SSSR_RESOURCE_IDENTIFIER_SCRAMBLING_TILE_BUFFER             24
#define FFX_SSSR_RESOURCE_IDENTIFIER_BLUE_NOISE_TEXTURE                 25
#define FFX_SSSR_RESOURCE_IDENTIFIER_SPD_GLOBAL_ATOMIC                  26
#define FFX_SSSR_RESOURCE_IDENTIFIER_COUNT                              27

#define FFX_SSSR_CONSTANTBUFFER_IDENTIFIER_SSSR                         0
#define FFX_SSSR_CONSTANTBUFFER_IDENTIFIER_COUNT                        1

#endif // #if defined(FFX_CPU) || defined(FFX_GPU)

#endif //!defined( FFX_SSSR_RESOURCES_H )
