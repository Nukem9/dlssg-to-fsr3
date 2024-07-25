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

#ifndef FFX_DOF_COMMON_H
#define FFX_DOF_COMMON_H

/// @defgroup FfxGPUDof FidelityFX DOF
/// FidelityFX Depth of Field GPU documentation
/// 
/// @ingroup FfxGPUEffects

// Constants - these are fixed magic numbers. Changes likely require changing code.

/// The width/height of tiles storing per-tile min and max CoC. The blur pass needs to run
/// at a compatible tile size, so that the assumption that min/max CoC is uniform for all threads in
/// a thread group holds.
///
/// @ingroup FfxGPUDof
FFX_STATIC const FfxUInt32 FFX_DOF_DEPTH_TILE_SIZE = 8;

// Default settings
#ifndef FFX_DOF_OPTION_MAX_MIP
/// The number of mips that should be generated in the color downsample pass
///
/// @ingroup FfxGPUDof
#define FFX_DOF_OPTION_MAX_MIP 4
#endif

#ifndef FFX_DOF_OPTION_MAX_RING_MERGE_LOG
/// The base-2 logarithm of the number of blur kernel rings that may be merged, if possible.
///
/// Setting this to zero disables ring merging.
///
/// Values above 1 are technically valid, but are known to cause a visible quality drop.
///
/// @ingroup FfxGPUDof
#define FFX_DOF_OPTION_MAX_RING_MERGE_LOG 1
#endif

#define FFX_DOF_MAX_RING_MERGE (1 << FFX_DOF_OPTION_MAX_RING_MERGE_LOG)

// optional 16-bit types
#if FFX_HLSL
#if FFX_HALF
typedef FfxFloat16   FfxHalfOpt;
typedef FfxFloat16x2 FfxHalfOpt2;
typedef FfxFloat16x3 FfxHalfOpt3;
typedef FfxFloat16x4 FfxHalfOpt4;
#else
typedef FfxFloat32   FfxHalfOpt;
typedef FfxFloat32x2 FfxHalfOpt2;
typedef FfxFloat32x3 FfxHalfOpt3;
typedef FfxFloat32x4 FfxHalfOpt4;
#endif
#elif FFX_GLSL
#if FFX_HALF
#define FfxHalfOpt FfxFloat16  
#define FfxHalfOpt2 FfxFloat16x2
#define FfxHalfOpt3 FfxFloat16x3
#define FfxHalfOpt4 FfxFloat16x4
#else
#define FfxHalfOpt FfxFloat32  
#define FfxHalfOpt2 FfxFloat32x2
#define FfxHalfOpt3 FfxFloat32x3
#define FfxHalfOpt4 FfxFloat32x4
#endif
#endif

// unroll macros with count argument
#if FFX_HLSL
#define FFX_DOF_UNROLL_N(_n) [unroll(_n)]
#define FFX_DOF_UNROLL [unroll]
#elif FFX_GLSL // #if FFX_HLSL
#define FFX_DOF_UNROLL_N(_n)
// enable unrolling loops
#extension GL_EXT_control_flow_attributes : enable

#if GL_EXT_control_flow_attributes
#define FFX_DOF_UNROLL [[unroll]]
#else
#define FFX_DOF_UNROLL
#endif // #if GL_EXT_control_flow_attributes
#endif // #if FFX_HLSL #elif FFX_GLSL

// ReadLaneFirst
#if FFX_HLSL
#define ffxWaveReadLaneFirst WaveReadLaneFirst
#elif FFX_GLSL
#define ffxWaveReadLaneFirst subgroupBroadcastFirst
#endif // #if FFX_GLSL

// Callback declarations
FfxFloat32 FfxDofGetCoc(FfxFloat32 depth); // returns circle of confusion radius in pixels (at half-res). Sign indicates near-field (positive) or far-field (negative).
void FfxDofStoreDilatedRadius(FfxUInt32x2 coord, FfxFloat32x2 dilatedMinMax);
FfxFloat32x2 FfxDofSampleDilatedRadius(FfxUInt32x2 coord); // returns the dilated min and max depth for a pixel coordinate. Should be bilinearly interpolated!
FfxFloat32x2 FfxDofLoadDilatedRadius(FfxUInt32x2 coord); // returns the dilated depth for a *tile* coordinate without interpolation
FfxHalfOpt4 FfxDofLoadInput(FfxUInt32x2 coord); // returns input rgb and (in alpha) CoC in pixels from the most detailed mip (which is half-res)
FfxHalfOpt4 FfxDofSampleInput(FfxFloat32x2 coord, FfxUInt32 mip); // returns input rgb and (in alpha) CoC in pixels.
FfxFloat32x4 FfxDofGatherDepth(FfxFloat32x2 coord); // returns 4 input depth values in a quad, must use sampler with coordinate clamping
void FfxDofStoreNear(FfxUInt32x2 coord, FfxHalfOpt4 color); // stores the near color output from the blur pass
void FfxDofStoreFar(FfxUInt32x2 coord, FfxHalfOpt4 color); // stores the far color output from the blur pass
FfxHalfOpt4 FfxDofLoadNear(FfxUInt32x2 coord); // loads the near color value
FfxHalfOpt4 FfxDofLoadFar(FfxUInt32x2 coord); // loads the far color value
void FfxDofAccumMaxTileRadius(FfxUInt32 radius); // stores the global maximum dilation radius using an atomic max operation
FfxUInt32 FfxDofGetMaxTileRadius(); // gets the global maximum dilation radius
void FfxDofResetMaxTileRadius(); // sets max dilation radius back to zero
void FfxDofStoreTileRadius(FfxUInt32x2 tile, FfxFloat32x2 radius); // stores the min/max signed radius for a tile in the tile map
FfxFloat32x2 FfxDofLoadTileRadius(FfxUInt32x2 tile); // loads the min/max depth for a tile
FfxFloat32x4 FfxDofLoadFullInput(FfxUInt32x2 coord); // returns full resultion input rgba (alpha is ignored)
FfxFloat32 FfxDofLoadFullDepth(FfxUInt32x2 coord); // returns full resolution depth
void FfxDofStoreOutput(FfxUInt32x2 coord, FfxFloat32x4 color); // writes the final output

// Common helper functions
FfxUInt32x2 FfxDofPxRadToTiles(FfxFloat32x2 rPx)
{
	return FfxUInt32x2(ceil(rPx / (FFX_DOF_DEPTH_TILE_SIZE / 2))) + 1; // +1 to handle diagonals & bilinear
}

FfxFloat32x2 FfxDofGetCoc2(FfxFloat32x2 depths)
{
	return FfxFloat32x2(FfxDofGetCoc(depths.x), FfxDofGetCoc(depths.y));
}
#endif // #ifndef FFX_DOF_COMMON_H
