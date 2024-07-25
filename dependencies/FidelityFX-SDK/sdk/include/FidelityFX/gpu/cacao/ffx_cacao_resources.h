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

#ifndef FFX_CACAO_RESOURCES_H
#define FFX_CACAO_RESOURCES_H

#if defined(FFX_CPU) || defined(FFX_GPU)
#define FFX_FSR2_RESOURCE_IDENTIFIER_NULL                                          0
#define FFX_CACAO_RESOURCE_IDENTIFIER_DEPTH_IN                                     1
#define FFX_CACAO_RESOURCE_IDENTIFIER_NORMAL_IN                                    2
#define FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_DEPTHS                         3
#define FFX_CACAO_RESOURCE_IDENTIFIER_DEINTERLEAVED_NORMALS                        4
#define FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PING                             5
#define FFX_CACAO_RESOURCE_IDENTIFIER_SSAO_BUFFER_PONG                             6
#define FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP                               7
#define FFX_CACAO_RESOURCE_IDENTIFIER_IMPORTANCE_MAP_PONG                          8
#define FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_SSAO_BUFFER                      9
#define FFX_CACAO_RESOURCE_IDENTIFIER_LOAD_COUNTER_BUFFER                          10
#define FFX_CACAO_RESOURCE_IDENTIFIER_OUTPUT                                       11
#define FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_DEPTH_MIPMAP_0                   12
#define FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_DEPTH_MIPMAP_1                   13
#define FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_DEPTH_MIPMAP_2                   14
#define FFX_CACAO_RESOURCE_IDENTIFIER_DOWNSAMPLED_DEPTH_MIPMAP_3                   15
#define FFX_CACAO_RESOURCE_IDENTIFIER_COUNT                                        16

#define FFX_CACAO_CONSTANTBUFFER_IDENTIFIER_CACAO                                   0
#define FFX_CACAO_CONSTANTBUFFER_IDENTIFIER_COUNT                                   1

#endif  // #if defined(FFX_CPU) || defined(FFX_GPU)

#endif  //!defined( FFX_FSR2_RESOURCES_H )
