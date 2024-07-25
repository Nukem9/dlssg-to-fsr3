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

#include "ffx_core.h"
#include "ffx_cacao_defines.h"
#include "ffx_cacao_utils.h"

// all in one, SIMD in yo SIMD dawg, shader
#define FFX_CACAO_TILE_WIDTH  4
#define FFX_CACAO_TILE_HEIGHT 3
#define FFX_CACAO_HALF_TILE_WIDTH (FFX_CACAO_TILE_WIDTH / 2)
#define FFX_CACAO_QUARTER_TILE_WIDTH (FFX_CACAO_TILE_WIDTH / 4)

#define FFX_CACAO_ARRAY_WIDTH  (FFX_CACAO_HALF_TILE_WIDTH  * FFX_CACAO_BLUR_WIDTH  + 2)
#define FFX_CACAO_ARRAY_HEIGHT (FFX_CACAO_TILE_HEIGHT * FFX_CACAO_BLUR_HEIGHT + 2)

#define FFX_CACAO_ITERS 4

FFX_GROUPSHARED FfxUInt32 s_FFX_CACAO_BlurF16Front_4[FFX_CACAO_ARRAY_WIDTH][FFX_CACAO_ARRAY_HEIGHT];
FFX_GROUPSHARED FfxUInt32 s_FFX_CACAO_BlurF16Back_4[FFX_CACAO_ARRAY_WIDTH][FFX_CACAO_ARRAY_HEIGHT];

#if FFX_HALF
struct FFX_CACAO_Edges_4
{
	FfxFloat16x4 left;
	FfxFloat16x4 right;
	FfxFloat16x4 top;
	FfxFloat16x4 bottom;
};

FFX_CACAO_Edges_4 FFX_CACAO_UnpackEdgesFloat16_4(FfxFloat16x4 _packedVal)
{
	FfxUInt32x4 packedVal = FfxUInt32x4(_packedVal * 255.5);
	FFX_CACAO_Edges_4 result;
	result.left   = FfxFloat16x4(ffxSaturate(FfxFloat16x4((packedVal >> 6) & 0x03) / 3.0 + InvSharpness()));
	result.right  = FfxFloat16x4(ffxSaturate(FfxFloat16x4((packedVal >> 4) & 0x03) / 3.0 + InvSharpness()));
	result.top    = FfxFloat16x4(ffxSaturate(FfxFloat16x4((packedVal >> 2) & 0x03) / 3.0 + InvSharpness()));
	result.bottom = FfxFloat16x4(ffxSaturate(FfxFloat16x4((packedVal >> 0) & 0x03) / 3.0 + InvSharpness()));

	return result;
}

FfxFloat16x4 FFX_CACAO_CalcBlurredSampleF16_4(FfxFloat16x4 packedEdges, FfxFloat16x4 centre, FfxFloat16x4 left, FfxFloat16x4 right, FfxFloat16x4 top, FfxFloat16x4 bottom)
{
	FfxFloat16x4 sum = centre * FfxFloat16(0.5f);
	FfxFloat16x4 weight = FfxFloat16x4(0.5f, 0.5f, 0.5f, 0.5f);
	FFX_CACAO_Edges_4 edges = FFX_CACAO_UnpackEdgesFloat16_4(packedEdges);

	sum += left * edges.left;
	weight += edges.left;
	sum += right * edges.right;
	weight += edges.right;
	sum += top * edges.top;
	weight += edges.top;
	sum += bottom * edges.bottom;
	weight += edges.bottom;

	return sum / weight;
}

void FFX_CACAO_LDSEdgeSensitiveBlur(const FfxUInt32 blurPasses, const FfxUInt32x2 tid, const FfxUInt32x2 gid, const FfxUInt32 layerId)
{
	FfxInt32x2 imageCoord = FfxInt32x2(gid) * (FfxInt32x2(FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH, FFX_CACAO_TILE_HEIGHT * FFX_CACAO_BLUR_HEIGHT) - FfxInt32(2*blurPasses)) + FfxInt32x2(FFX_CACAO_TILE_WIDTH, FFX_CACAO_TILE_HEIGHT) * FfxInt32x2(tid) - FfxInt32(blurPasses);
	FfxInt32x2 bufferCoord = FfxInt32x2(FFX_CACAO_HALF_TILE_WIDTH, FFX_CACAO_TILE_HEIGHT) * FfxInt32x2(tid) + 1;

	FfxFloat16x4 packedEdges[FFX_CACAO_QUARTER_TILE_WIDTH][FFX_CACAO_TILE_HEIGHT];
	{
		FfxFloat32x2 inputVal[FFX_CACAO_TILE_WIDTH][FFX_CACAO_TILE_HEIGHT];
		FfxInt32 y;
		FFX_UNROLL
		for (y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_TILE_WIDTH; ++x)
			{
				FfxFloat32x2 sampleUV = (FfxFloat32x2(imageCoord + FfxInt32x2(x, y)) + 0.5f) * SSAOBufferInverseDimensions();
				inputVal[x][y] = FFX_CACAO_EdgeSensitiveBlur_SampleInput(sampleUV, layerId);
			}
		}
		FFX_UNROLL
		for (y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
			{
				FfxFloat16x2 ssaoVals = FfxFloat16x2(inputVal[4 * x + 0][y].x, inputVal[4 * x + 1][y].x);
				s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + 2*x + 0][bufferCoord.y + y] = ffxPackF16(ssaoVals);
				ssaoVals = FfxFloat16x2(inputVal[4 * x + 2][y].x, inputVal[4 * x + 3][y].x);
				s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + 2*x + 1][bufferCoord.y + y] = ffxPackF16(ssaoVals);
				packedEdges[x][y] = FfxFloat16x4(inputVal[4 * x + 0][y].y, inputVal[4 * x + 1][y].y, inputVal[4 * x + 2][y].y, inputVal[4 * x + 3][y].y);
			}
		}
	}

	FFX_GROUP_MEMORY_BARRIER;

	FFX_UNROLL
	for (FfxUInt32 i = 0; i < (blurPasses + 1) / 2; ++i)
	{
		FFX_UNROLL
		for (FfxInt32 y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
			{
				FfxInt32x2 c = bufferCoord + FfxInt32x2(2*x, y);
				FfxFloat16x4 centre = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y + 0]), ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y + 0]));
				FfxFloat16x4 top    = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y - 1]), ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y - 1]));
				FfxFloat16x4 bottom = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y + 1]), ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y + 1]));

				FfxFloat16x2 tmp = ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x - 1][c.y + 0]);
				FfxFloat16x4 left = FfxFloat16x4(tmp.y, centre.xyz);
				tmp = ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[c.x + 2][c.y + 0]);
				FfxFloat16x4 right = FfxFloat16x4(centre.yzw, tmp.x);

				FfxFloat16x4 tmp_4 = FFX_CACAO_CalcBlurredSampleF16_4(packedEdges[x][y], centre, left, right, top, bottom);
				s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y] = ffxPackF16(tmp_4.xy);
				s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y] = ffxPackF16(tmp_4.zw);
			}
		}
		FFX_GROUP_MEMORY_BARRIER;

		if (2 * i + 1 < blurPasses)
		{
			FFX_UNROLL
			for (FfxInt32 y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
			{
				FFX_UNROLL
				for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
				{
					FfxInt32x2 c = bufferCoord + FfxInt32x2(2 * x, y);
					FfxFloat16x4 centre = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y + 0]), ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y + 0]));
					FfxFloat16x4 top    = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y - 1]), ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y - 1]));
					FfxFloat16x4 bottom = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y + 1]), ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y + 1]));

					FfxFloat16x2 tmp = ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x - 1][c.y + 0]);
					FfxFloat16x4 left = FfxFloat16x4(tmp.y, centre.xyz);
					tmp = ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[c.x + 2][c.y + 0]);
					FfxFloat16x4 right = FfxFloat16x4(centre.yzw, tmp.x);

					FfxFloat16x4 tmp_4 = FFX_CACAO_CalcBlurredSampleF16_4(packedEdges[x][y], centre, left, right, top, bottom);
					s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y] = ffxPackF16(tmp_4.xy);
					s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y] = ffxPackF16(tmp_4.zw);
				}
			}
			FFX_GROUP_MEMORY_BARRIER;
		}
	}

	FFX_UNROLL
	for (FfxUInt32 y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
	{
		FfxUInt32 outputY = FFX_CACAO_TILE_HEIGHT * tid.y + y;
		if (blurPasses <= outputY && outputY < FFX_CACAO_TILE_HEIGHT * FFX_CACAO_BLUR_HEIGHT - blurPasses)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
			{
				FfxUInt32 outputX = FFX_CACAO_TILE_WIDTH * tid.x + 4 * x;

				FfxFloat16x4 ssaoVal;
				if (blurPasses % 2 == 0)
				{
					ssaoVal = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + x][bufferCoord.y + y]), ffxUnpackF16(s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + x + 1][bufferCoord.y + y]));
				}
				else
				{
					ssaoVal = FfxFloat16x4(ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[bufferCoord.x + x][bufferCoord.y + y]), ffxUnpackF16(s_FFX_CACAO_BlurF16Back_4[bufferCoord.x + x + 1][bufferCoord.y + y]));
				}

				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x, y), FfxFloat32x2(ssaoVal.x, packedEdges[x][y].x), layerId);
				}
				outputX += 1;
				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x + 1, y), FfxFloat32x2(ssaoVal.y, packedEdges[x][y].y), layerId);
				}
				outputX += 1;
				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x + 2, y), FfxFloat32x2(ssaoVal.z, packedEdges[x][y].z), layerId);
				}
				outputX += 1;
				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x + 3, y), FfxFloat32x2(ssaoVal.w, packedEdges[x][y].w), layerId);
				}
			}
		}
	}
}
#else
struct FFX_CACAO_Edges_4
{
	FfxFloat32x4 left;
	FfxFloat32x4 right;
	FfxFloat32x4 top;
	FfxFloat32x4 bottom;
};

FFX_CACAO_Edges_4 FFX_CACAO_UnpackEdgesFloat32_4(FfxFloat32x4 _packedVal)
{
	FfxUInt32x4 packedVal = FfxUInt32x4(_packedVal * 255.5);
	FFX_CACAO_Edges_4 result;
	result.left   = ffxSaturate(((packedVal >> 6) & 0x03) / 3.0 + InvSharpness());
	result.right  = ffxSaturate(((packedVal >> 4) & 0x03) / 3.0 + InvSharpness());
	result.top    = ffxSaturate(((packedVal >> 2) & 0x03) / 3.0 + InvSharpness());
	result.bottom = ffxSaturate(((packedVal >> 0) & 0x03) / 3.0 + InvSharpness());

	return result;
}

FfxFloat32x4 FFX_CACAO_CalcBlurredSampleF32_4(FfxFloat32x4 packedEdges, FfxFloat32x4 centre, FfxFloat32x4 left, FfxFloat32x4 right, FfxFloat32x4 top, FfxFloat32x4 bottom)
{
	FfxFloat32x4 sum = centre * FfxFloat32(0.5f);
	FfxFloat32x4 weight = FfxFloat32x4(0.5f, 0.5f, 0.5f, 0.5f);
	FFX_CACAO_Edges_4 edges = FFX_CACAO_UnpackEdgesFloat32_4(packedEdges);

	sum += left * edges.left;
	weight += edges.left;
	sum += right * edges.right;
	weight += edges.right;
	sum += top * edges.top;
	weight += edges.top;
	sum += bottom * edges.bottom;
	weight += edges.bottom;

	return sum / weight;
}

void FFX_CACAO_LDSEdgeSensitiveBlur(const FfxUInt32 blurPasses, const FfxUInt32x2 tid, const FfxUInt32x2 gid, const FfxUInt32 layerId)
{
	FfxInt32x2 imageCoord = FfxInt32x2(gid) * (FfxInt32x2(FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH, FFX_CACAO_TILE_HEIGHT * FFX_CACAO_BLUR_HEIGHT) - FfxInt32(2*blurPasses)) + FfxInt32x2(FFX_CACAO_TILE_WIDTH, FFX_CACAO_TILE_HEIGHT) * FfxInt32x2(tid) - FfxInt32(blurPasses);
	FfxInt32x2 bufferCoord = FfxInt32x2(FFX_CACAO_HALF_TILE_WIDTH, FFX_CACAO_TILE_HEIGHT) * FfxInt32x2(tid) + 1;

	FfxFloat32x4 packedEdges[FFX_CACAO_QUARTER_TILE_WIDTH][FFX_CACAO_TILE_HEIGHT];
	{
		FfxFloat32x2 inputVal[FFX_CACAO_TILE_WIDTH][FFX_CACAO_TILE_HEIGHT];
		FfxInt32 y;
		FFX_UNROLL
		for (y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_TILE_WIDTH; ++x)
			{
				FfxFloat32x2 sampleUV = (FfxFloat32x2(imageCoord + FfxInt32x2(x, y)) + 0.5f) * SSAOBufferInverseDimensions();
				inputVal[x][y] = FFX_CACAO_EdgeSensitiveBlur_SampleInput(sampleUV, layerId);
			}
		}
		FFX_UNROLL
		for (y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
			{
				FfxFloat32x2 ssaoVals = FfxFloat32x2(inputVal[4 * x + 0][y].x, inputVal[4 * x + 1][y].x);
				s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + 2*x + 0][bufferCoord.y + y] = ffxPackF32(ssaoVals);
				ssaoVals = FfxFloat32x2(inputVal[4 * x + 2][y].x, inputVal[4 * x + 3][y].x);
				s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + 2*x + 1][bufferCoord.y + y] = ffxPackF32(ssaoVals);
				packedEdges[x][y] = FfxFloat32x4(inputVal[4 * x + 0][y].y, inputVal[4 * x + 1][y].y, inputVal[4 * x + 2][y].y, inputVal[4 * x + 3][y].y);
			}
		}
	}

	FFX_GROUP_MEMORY_BARRIER;

	FFX_UNROLL
	for (FfxUInt32 i = 0; i < (blurPasses + 1) / 2; ++i)
	{
		FFX_UNROLL
		for (FfxInt32 y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
			{
				FfxInt32x2 c = bufferCoord + FfxInt32x2(2*x, y);
				FfxFloat32x4 centre = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y + 0]), ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y + 0]));
				FfxFloat32x4 top    = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y - 1]), ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y - 1]));
				FfxFloat32x4 bottom = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y + 1]), ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y + 1]));

				FfxFloat32x2 tmp = ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x - 1][c.y + 0]);
				FfxFloat32x4 left = FfxFloat32x4(tmp.y, centre.xyz);
				tmp = ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[c.x + 2][c.y + 0]);
				FfxFloat32x4 right = FfxFloat32x4(centre.yzw, tmp.x);

				FfxFloat32x4 tmp_4 = FFX_CACAO_CalcBlurredSampleF32_4(packedEdges[x][y], centre, left, right, top, bottom);
				s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y] = ffxPackF32(tmp_4.xy);
				s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y] = ffxPackF32(tmp_4.zw);
			}
		}
		FFX_GROUP_MEMORY_BARRIER;

		if (2 * i + 1 < blurPasses)
		{
			FFX_UNROLL
			for (FfxInt32 y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
			{
				FFX_UNROLL
				for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
				{
					FfxInt32x2 c = bufferCoord + FfxInt32x2(2 * x, y);
					FfxFloat32x4 centre = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y + 0]), ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y + 0]));
					FfxFloat32x4 top    = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y - 1]), ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y - 1]));
					FfxFloat32x4 bottom = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 0][c.y + 1]), ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 1][c.y + 1]));

					FfxFloat32x2 tmp = ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x - 1][c.y + 0]);
					FfxFloat32x4 left = FfxFloat32x4(tmp.y, centre.xyz);
					tmp = ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[c.x + 2][c.y + 0]);
					FfxFloat32x4 right = FfxFloat32x4(centre.yzw, tmp.x);

					FfxFloat32x4 tmp_4 = FFX_CACAO_CalcBlurredSampleF32_4(packedEdges[x][y], centre, left, right, top, bottom);
					s_FFX_CACAO_BlurF16Front_4[c.x + 0][c.y] = ffxPackF32(tmp_4.xy);
					s_FFX_CACAO_BlurF16Front_4[c.x + 1][c.y] = ffxPackF32(tmp_4.zw);
				}
			}
			FFX_GROUP_MEMORY_BARRIER;
		}
	}

	FFX_UNROLL
	for (FfxUInt32 y = 0; y < FFX_CACAO_TILE_HEIGHT; ++y)
	{
		FfxUInt32 outputY = FFX_CACAO_TILE_HEIGHT * tid.y + y;
		if (blurPasses <= outputY && outputY < FFX_CACAO_TILE_HEIGHT * FFX_CACAO_BLUR_HEIGHT - blurPasses)
		{
			FFX_UNROLL
			for (FfxInt32 x = 0; x < FFX_CACAO_QUARTER_TILE_WIDTH; ++x)
			{
				FfxUInt32 outputX = FFX_CACAO_TILE_WIDTH * tid.x + 4 * x;

				FfxFloat32x4 ssaoVal;
				if (blurPasses % 2 == 0)
				{
					ssaoVal = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + x][bufferCoord.y + y]), ffxUnpackF32(s_FFX_CACAO_BlurF16Front_4[bufferCoord.x + x + 1][bufferCoord.y + y]));
				}
				else
				{
					ssaoVal = FfxFloat32x4(ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[bufferCoord.x + x][bufferCoord.y + y]), ffxUnpackF32(s_FFX_CACAO_BlurF16Back_4[bufferCoord.x + x + 1][bufferCoord.y + y]));
				}

				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x, y), FfxFloat32x2(ssaoVal.x, packedEdges[x][y].x), layerId);
				}
				outputX += 1;
				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x + 1, y), FfxFloat32x2(ssaoVal.y, packedEdges[x][y].y), layerId);
				}
				outputX += 1;
				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x + 2, y), FfxFloat32x2(ssaoVal.z, packedEdges[x][y].z), layerId);
				}
				outputX += 1;
				if (blurPasses <= outputX && outputX < FFX_CACAO_TILE_WIDTH * FFX_CACAO_BLUR_WIDTH - blurPasses)
				{
					FFX_CACAO_EdgeSensitiveBlur_StoreOutput(imageCoord + FfxInt32x2(4 * x + 3, y), FfxFloat32x2(ssaoVal.w, packedEdges[x][y].w), layerId);
				}
			}
		}
	}
}
#endif //FFX_HALF

void FFX_CACAO_EdgeSensitiveBlur1(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
    FFX_CACAO_LDSEdgeSensitiveBlur(1, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur2(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(2, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur3(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(3, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur4(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(4, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur5(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(5, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur6(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(6, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur7(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(7, tid, gid.xy, layerId);
}

void FFX_CACAO_EdgeSensitiveBlur8(FfxUInt32x2 tid, FfxUInt32x3 gid)
{
    FfxUInt32 layerId = gid.z;
    layerId     = (layerId == 1 && BlurNumPasses() == 2) ? 3 : layerId;
	FFX_CACAO_LDSEdgeSensitiveBlur(8, tid, gid.xy, layerId);
}

#undef FFX_CACAO_TILE_WIDTH
#undef FFX_CACAO_TILE_HEIGHT
#undef FFX_CACAO_HALF_TILE_WIDTH
#undef FFX_CACAO_QUARTER_TILE_WIDTH
#undef FFX_CACAO_ARRAY_WIDTH
#undef FFX_CACAO_ARRAY_HEIGHT
#undef FFX_CACAO_ITERS
