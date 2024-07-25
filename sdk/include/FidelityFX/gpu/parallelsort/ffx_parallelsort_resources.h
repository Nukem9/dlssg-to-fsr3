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

#ifndef FFX_PARALLELSORT_RESOURCES_H
#define FFX_PARALLELSORT_RESOURCES_H

#if defined(FFX_CPU) || defined(FFX_GPU)

// Physical resources
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_NULL                                  0
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_KEY_BUFFER                      1
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INPUT_PAYLOAD_BUFFER                  2
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SCRATCH_BUFFER                    3
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SCRATCH_BUFFER                4
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCRATCH_BUFFER                        5
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCED_SCRATCH_BUFFER                6
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_COUNT_SCATTER_ARGS_BUFFER    7
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_INDIRECT_REDUCE_SCAN_ARGS_BUFER       8

// Binding resources
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SUM_TABLE                             10
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_REDUCE_TABLE                          11
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SOURCE                           12
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_DST                              13
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_SCAN_SCRATCH                          14
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_SRC                               15
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_KEY_DST                               16
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_SRC                           17
#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_PAYLOAD_DST                           18

#define FFX_PARALLELSORT_RESOURCE_IDENTIFIER_COUNT                                 19

// CBV resource definitions
#define FFX_PARALLELSORT_CONSTANTBUFFER_IDENTIFIER_PARALLEL_SORT                   0

#endif  // #if defined(FFX_CPU) || defined(FFX_GPU)

#endif  // FFX_PARALLELSORT_RESOURCES_H
