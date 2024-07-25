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

#ifndef FFX_SPD_COMMON_H
#define FFX_SPD_COMMON_H

// NOTE: Make sure this is always packed to 16 bytes
struct SPDDownsampleInfo
{
#ifdef __cplusplus
    unsigned int    OutSize[2];
    float           InvSize[2];
    
    // Only used for CS version
    int             Slice;
    int             Padding[3];
#else
    uint2   OutSize;
    float2  InvSize;
    
    // Only used for CS version
    int     Slice;
    int3    Padding;
#endif // _cplusplus
};

struct SPDVerifyConstants
{
#ifdef __cplusplus
    uint32_t    NumMips;
    uint32_t    SliceId;
    float       InvAspectRatio;
    uint32_t    Paddding;
#else
    uint    NumMips;
    uint    SliceId;
    float   InvAspectRatio;
    uint    Paddding;
#endif // _cplusplus
};

#endif // FFX_SPD_COMMON_H
