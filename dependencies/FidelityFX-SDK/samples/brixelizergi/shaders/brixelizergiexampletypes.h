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

#ifndef BRIXELIZER_EXAMPLE_TYPES_H
#define BRIXELIZER_EXAMPLE_TYPES_H

enum BrixelizerExampleFlags {
	BRIXELIZER_EXAMPLE_SHOW_BRICK_OUTLINES = 1 << 0,
};

#ifdef __cplusplus
	#define ENUM_WIDTH : int32_t
#else
	#define ENUM_WIDTH
#endif

#define BRIXELIZER_EXAMPLE_OUTPUT_TYPES OT(DISTANCE) OT(UVW) OT(ITERATIONS) OT(GRADIENT) OT(BRICK_ID)
enum BrixelizerExampleOutputType ENUM_WIDTH
{
#define OT(name) BRIXELIZER_EXAMPLE_OUTPUT_TYPE_##name,
	BRIXELIZER_EXAMPLE_OUTPUT_TYPES
#undef OT
};

#undef ENUM_WIDTH

struct BrixelizerExampleConstants
{
#ifdef __cplusplus
	float     SolveEpsilon;
	float     TMin;
	float     TMax;
	uint32_t  State;

	float     InvView[16];

	float     InvProj[16];

	uint32_t  StartCascadeID;
	uint32_t  EndCascadeID;
	uint32_t  Flags;
	float     Alpha;
#else
	float     SolveEpsilon;
	float     TMin;
	float     TMax;
	uint      State;

	float4x4  InvView;

	float4x4  InvProj;

	uint      StartCascadeID;
	uint      EndCascadeID;
	uint      Flags;
	float     Alpha;
#endif
};

#endif