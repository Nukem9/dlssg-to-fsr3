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

#ifndef FFX_BLUR_H
#define FFX_BLUR_H
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//
//                                                 [FFX BLUR] Blur
//
//==============================================================================================================================
//
/// @defgroup FfxGPUBlur FidelityFX BLUR
/// FidelityFX Blur GPU documentation
///
/// @ingroup FfxGPUEffects

//------------------------------------------------------------------------------------------------------------------------------
//
// ABOUT
// =====
// AMD FidelityFX Blur is a collection of blurring effects implemented on compute shaders, hand-optimized for maximum performance.
// FFX-Blur includes
// - Gaussian Blur w/ large kernel support (up to 21x21)
//
//==============================================================================================================================

//==============================================================================================================================
//                                                     BLUR SETUP
//==============================================================================================================================


/// FFX_BLUR_TILE_SIZE_Y: Tile Y dimensions that the local threadgroup will work on.
/// Note: each threadgroup is responsible for blurring a tile of the input image.
///
/// @ingroup FfxGPUBlur
#ifndef FFX_BLUR_TILE_SIZE_Y
#define FFX_BLUR_TILE_SIZE_Y 8
#endif

/// FFX_BLUR_TILE_SIZE_X: Tile X dimensions that the local threadgroup will work on.
/// Note: each threadgroup is responsible for blurring a tile of the input image.
///
/// @ingroup FfxGPUBlur
#ifndef FFX_BLUR_TILE_SIZE_X
#define FFX_BLUR_TILE_SIZE_X 8
#endif

/// FFX_BLUR_DISPATCH_Y: Y dimension of the Blur compute dispatch.
/// The compute dispatch on the CPU side uses this value when invoking Dispatch.
///
/// @ingroup FfxGPUBlur
#define FFX_BLUR_DISPATCH_Y  8

#ifndef FFX_CPU

/// FFX_BLUR_OPTION_KERNEL_DIMENSION needs to be defined by the client application
/// App should define e.g the following for 5x5 blur:
/// #define FFX_BLUR_OPTION_KERNEL_DIMENSION 5
///
/// @ingroup FfxGPUBlur
#ifndef FFX_BLUR_OPTION_KERNEL_DIMENSION
#error Please define FFX_BLUR_OPTION_KERNEL_DIMENSION
#endif

/// FFX_BLUR_KERNEL_RANGE is defined relative to FFX_BLUR_OPTION_KERNEL_DIMENSION
/// See ffx_blur_callbacks_*.h for details.
///
/// @ingroup FfxGPUBlur
#ifndef FFX_BLUR_KERNEL_RANGE
#error Please define FFX_BLUR_KERNEL_RANGE
#endif

//--------------------------------------------------------------------------------------
// BLUR CONFIG
//--------------------------------------------------------------------------------------
// hardcoded variants
#define BLUR_DEBUG_PREFILL_OUTPUT_CACHE_WITH_COLOR 0
#define BLUR_GROUPSHARED_MEMORY_SOA                0 // [Deprecated] improves LDS but too high traffic still
#define BLUR_GROUPSHARED_MEMORY_HALF               0 // [Deprecated] LOTS of LDS traffic (1x ds_read per channel), need to pack with FfxUInt32
#define BLUR_GROUPSHARED_MEMORY_PK_UINT            1 // 1x ds_read2st64_b32 for all three channels
#define BLUR_FP16_KERNEL_LOOPS                     1 // use fp16 for kernel loop counters and lds indexing (increases VGPR due to sdwa)
#define BLUR_FP16_CLAMP                            1 // ensure fp16 min/max is used for clamp()
// cpu-driven variants
#ifndef BLUR_ENABLE_INPUT_CACHE
#define BLUR_ENABLE_INPUT_CACHE                    0 // currently only slows the algorithm :(
#endif
#ifndef BLUR_DISABLE_CLAMP
#define BLUR_DISABLE_CLAMP                         0 // Generates incorrect image at the image borders (no clamp), for testing theoretical speed 
#endif
#ifndef BLUR_OPTIMIZED_CLAMP
#define BLUR_OPTIMIZED_CLAMP                       0 // [Experimental] Testing a new optimized clamp ISA
#endif

// constants
#define BLUR_TILE_SIZE_Y_INV  (1.0 / FFX_BLUR_TILE_SIZE_Y)

//--------------------------------------------------------------------------------------
// GROUPSHARED MEMORY
//--------------------------------------------------------------------------------------
// Define CacheTypes<FP16, SOA> 
#if BLUR_GROUPSHARED_MEMORY_SOA
    #if BLUR_GROUPSHARED_MEMORY_HALF
        #ifdef FFX_HLSL
            #define BLUR_GROUPSHARED_MEMORY_TYPE groupshared FfxFloat16
        #else
            #define BLUR_GROUPSHARED_MEMORY_TYPE shared FfxFloat16
        #endif
    #else 
        #ifdef FFX_HLSL
            #define BLUR_GROUPSHARED_MEMORY_TYPE groupshared FfxFloat32
        #else
            #define BLUR_GROUPSHARED_MEMORY_TYPE shared FfxFloat32
        #endif
    #endif 
#else // BLUR_GROUPSHARED_MEMORY_SOA
    #if BLUR_GROUPSHARED_MEMORY_HALF
        #ifdef FFX_HLSL
            #define BLUR_GROUPSHARED_MEMORY_TYPE groupshared FfxFloat16x3
        #else
            #define BLUR_GROUPSHARED_MEMORY_TYPE shared FfxFloat16x3
        #endif
    #else 
        #ifdef FFX_HLSL
            #define BLUR_GROUPSHARED_MEMORY_TYPE groupshared FfxFloat32x3
        #else
            #define BLUR_GROUPSHARED_MEMORY_TYPE shared FfxFloat32x3
        #endif
    #endif 
#endif // BLUR_GROUPSHARED_MEMORY_SOA

//==============================================================================================================================
//                                                     MATH HELPERS
//==============================================================================================================================
#define DIV_AND_ROUND_UP(x, y) (((x) + ((y)-1)) / ((y)))
// Fast modulo operator for powers of two values for Y: x % y == x & (y-1)
#define FAST_MOD(x, y) ((x) & (y - 1))
#if FFX_HALF
#define FAST_MOD16(x, y) ((x) & (y - FfxInt16(1)))
#endif

// OUTPUT CACHE ########################################
/*
# Notes from Jordan's Presentation
src: https://gpuopen.com/gdc-presentations/2019/gdc-2019-s5-blend-of-gcn-optimization-and-color-processing.pdf
Use 2^n tiles to use bitwise AND in place of the more ALU-expensive % operator, see #define FAST_MOD above
MinTiles     -> Ceil(HalfKernel / TileSize) * 2 + 1
*/
#if FFX_BLUR_OPTION_KERNEL_DIMENSION > 7
#define NUM_TILES_OUTPUT_CACHE 8
#else
#define NUM_TILES_OUTPUT_CACHE 4
#endif

#define NUM_PIXELS_OUTPUT_CACHE (FFX_BLUR_TILE_SIZE_Y * FFX_BLUR_TILE_SIZE_X * NUM_TILES_OUTPUT_CACHE)


#if BLUR_GROUPSHARED_MEMORY_PK_UINT
#ifdef FFX_HLSL
    groupshared FfxUInt32  OutputCacheRG[NUM_PIXELS_OUTPUT_CACHE]; // RG: 2x fp16's are packed into 32bit unsigned int
    groupshared FfxFloat32 OutputCacheB [NUM_PIXELS_OUTPUT_CACHE]; // B : don't use fp16 for B to avoid bank conflicts
#else
    shared FfxUInt32  OutputCacheRG[NUM_PIXELS_OUTPUT_CACHE]; // RG: 2x fp16's are packed into 32bit unsigned int
    shared FfxFloat32 OutputCacheB [NUM_PIXELS_OUTPUT_CACHE]; // B : don't use fp16 for B to avoid bank conflicts
#endif
#else 
    #if BLUR_GROUPSHARED_MEMORY_SOA
        BLUR_GROUPSHARED_MEMORY_TYPE OutputCache[NUM_PIXELS_OUTPUT_CACHE * 3]; // stores rrrrrr...ggggggg...bbbbb...
    #else
        BLUR_GROUPSHARED_MEMORY_TYPE OutputCache[NUM_PIXELS_OUTPUT_CACHE]; // stores rgbrgbrgbrgbrgbrgbrgbrgb...
    #endif // BLUR_GROUPSHARED_MEMORY_SOA
#endif // BLUR_GROUPSHARED_MEMORY_PK_UINT

// Based on the FFX_BLUR_OPTION_KERNEL_DIMENSION, we will need to pre-fill a number of tiles.
// e.g. TILE_SIZE_Y=8
// -----------------------------------------
// kernel = 3  | NUM_PREFILL_TILES_OUTPUT_CACHE = 1 |
// kernel = 5  | NUM_PREFILL_TILES_OUTPUT_CACHE = 1 |
// kernel = 7  | NUM_PREFILL_TILES_OUTPUT_CACHE = 1 |
// kernel = 9* | NUM_PREFILL_TILES_OUTPUT_CACHE = 2*|
// kernel = 11 | NUM_PREFILL_TILES_OUTPUT_CACHE = 2 |
// kernel = 13 | NUM_PREFILL_TILES_OUTPUT_CACHE = 2 |
// kernel = 15 | NUM_PREFILL_TILES_OUTPUT_CACHE = 2 |
// kernel = 17*| NUM_PREFILL_TILES_OUTPUT_CACHE = 3*|
// kernel = 19 | NUM_PREFILL_TILES_OUTPUT_CACHE = 3 |
// kernel = 21 | NUM_PREFILL_TILES_OUTPUT_CACHE = 3 |
// kernel = 23 | NUM_PREFILL_TILES_OUTPUT_CACHE = 3 |
// -----------------------------------------
#define NUM_PREFILL_TILES_OUTPUT_CACHE  DIV_AND_ROUND_UP(FFX_BLUR_OPTION_KERNEL_DIMENSION, FFX_BLUR_TILE_SIZE_Y)


// INPUT CACHE ########################################
#if BLUR_ENABLE_INPUT_CACHE
    #define INPUT_CACHE_TILE_SIZE_X (FFX_BLUR_TILE_SIZE_X + FFX_BLUR_OPTION_KERNEL_DIMENSION - 1)
    #define NUM_TILES_INPUT_CACHE  1
    #define NUM_PIXELS_INPUT_CACHE ((INPUT_CACHE_TILE_SIZE_X * FFX_BLUR_TILE_SIZE_Y) * NUM_TILES_INPUT_CACHE)

    #if BLUR_GROUPSHARED_MEMORY_PK_UINT
        #ifdef FFX_HLSL
            groupshared FfxUInt32  InputCacheRG[NUM_PIXELS_INPUT_CACHE]; // RG: 2x fp16's are packed into 32bit unsigned int
            groupshared FfxFloat32 InputCacheB [NUM_PIXELS_INPUT_CACHE]; // B : don't use fp16 for B to avoid bank conflicts
        #else
            shared FfxUInt32  InputCacheRG[NUM_PIXELS_INPUT_CACHE]; // RG: 2x fp16's are packed into 32bit unsigned int
            shared FfxFloat32 InputCacheB [NUM_PIXELS_INPUT_CACHE]; // B : don't use fp16 for B to avoid bank conflicts
        #endif
    #else 
        #if BLUR_GROUPSHARED_MEMORY_SOA
            BLUR_GROUPSHARED_MEMORY_TYPE InputCache[NUM_PIXELS_INPUT_CACHE * 3]; // stores rrrrrr...ggggggg...bbbbb...
        #else
            BLUR_GROUPSHARED_MEMORY_TYPE InputCache[NUM_PIXELS_INPUT_CACHE];     // stores rgbrgbrgbrgbrgbrgbrgbrgb...
        #endif // BLUR_GROUPSHARED_MEMORY_SOA
    #endif //BLUR_GROUPSHARED_MEMORY_PK_UINT
#endif // BLUR_ENABLE_INPUT_CACHE

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//==============================================================================================================================
//                                                GROUPSHARED MEMORY MAPPING FUNCTIONS
//==============================================================================================================================
// LDS ops ----------------------------------------------------------------------------------
// LDS TILES : FFX_BLUR_TILE_SIZE_X * FFX_BLUR_TILE_SIZE_Y sized tiles
// e.g. FFX_BLUR_TILE_SIZE_X = 8
// 
// <------------ FFX_BLUR_TILE_SIZE_X -------------->
// ^ OutputCache[0-7] 
// | OutputCache[8-15]
// |
// |                                                        // TILE #1
// |
// |
// v OutputCache[56-63]
// <------------------------------------------------>
//                          |
// <------------ FFX_BLUR_TILE_SIZE_X -------------->
// ^ OutputCache[64-71]
// |
// |
// |                                                        // TILE #2
// |
// |
// v
// <------------------------------------------------>
//                          |
//...
FfxUInt32 PackF2(FfxFloat32x2 c) { return (ffxF32ToF16(c.r) << 16) | ffxF32ToF16(c.g); }
FfxFloat32x2 UnpackToF2(FfxUInt32 packedRG)
{
#ifdef FFX_HLSL
    return f16tof32(FfxUInt32x2(packedRG >> 16, packedRG & 0xFFFF));
#else
    return unpackHalf2x16(packedRG).yx;
#endif
}
#if FFX_HALF
FfxUInt32 PackH2(FfxFloat16x2 c) { return (ffxF32ToF16(FfxFloat32(c.r)) << 16) | ffxF32ToF16(FfxFloat32(c.g)); } // TODO: is there a cast fp16->FfxUInt32  and skip fp16->fp32 promotion?
FfxFloat16x2 UnpackToH2(FfxUInt32 packedRG){ return FfxFloat16x2(UnpackToF2(packedRG)); }
#endif

#ifdef FFX_HLSL
inline FfxUInt32 FlattenIndex(FfxInt32x2 Index, FfxInt32 ElementStride)
#else
FfxUInt32 FlattenIndex(FfxInt32x2 Index, FfxInt32 ElementStride)
#endif
{
    return Index.x + Index.y * ElementStride;
}

#if BLUR_GROUPSHARED_MEMORY_SOA
    void SetOutputCache(FfxInt32x2 index, FfxFloat32x3 value)
    { 
        FfxInt32 iLDS = index.x + index.y * BLUR_TILE_SIZE_X;
        OutputCache[iLDS + NUM_PIXELS_OUTPUT_CACHE * 0] = value.r;
        OutputCache[iLDS + NUM_PIXELS_OUTPUT_CACHE * 1] = value.g;
        OutputCache[iLDS + NUM_PIXELS_OUTPUT_CACHE * 2] = value.b;
    }
    FfxFloat32x3 GetOutputCache(FfxInt32x2 index)
    { 
        FfxInt32 iLDS = index.x + index.y * FFX_BLUR_TILE_SIZE_X;
        FfxFloat32x3 c;
        c.r = OutputCache[iLDS + NUM_PIXELS_OUTPUT_CACHE * 0];
        c.g = OutputCache[iLDS + NUM_PIXELS_OUTPUT_CACHE * 1];
        c.b = OutputCache[iLDS + NUM_PIXELS_OUTPUT_CACHE * 2];
        return c;
    }
    #if BLUR_ENABLE_INPUT_CACHE
    void SetInputCache(FfxInt32x2 index, FfxFloat32x3 value) 
    {
        FfxInt32 iLDS = index.x + index.y * INPUT_CACHE_TILE_SIZE_X;
        InputCache[iLDS + NUM_PIXELS_INPUT_CACHE * 0] = value.r;
        InputCache[iLDS + NUM_PIXELS_INPUT_CACHE * 1] = value.g;
        InputCache[iLDS + NUM_PIXELS_INPUT_CACHE * 2] = value.b;
    }
    FfxFloat32x3 GetInputCache(FfxInt32x2 index)
    { 
        FfxInt32 iLDS = index.x + index.y * INPUT_CACHE_TILE_SIZE_X;
        FfxFloat32x3 c;
        c.r = InputCache[iLDS + NUM_PIXELS_INPUT_CACHE * 0];
        c.g = InputCache[iLDS + NUM_PIXELS_INPUT_CACHE * 1];
        c.b = InputCache[iLDS + NUM_PIXELS_INPUT_CACHE * 2];
        return c;
    }
    #endif // BLUR_ENABLE_INPUT_CACHE
#else
    #if BLUR_GROUPSHARED_MEMORY_PK_UINT
        #if FFX_HALF
        void SetOutputCache(FfxInt32x2 index, FfxFloat16x3 value)
        {
            FfxInt32 iLDS = index.x + index.y * FFX_BLUR_TILE_SIZE_X;
            OutputCacheRG[iLDS] = PackH2(value.rg);
            OutputCacheB[iLDS] = value.b;
        }
        FfxFloat16x3 GetOutputCache(FfxInt32x2 index)
        {
            FfxInt32 iLDS = index.x + index.y * FFX_BLUR_TILE_SIZE_X;
            FfxFloat16x2 RG = UnpackToH2(OutputCacheRG[iLDS]);
            return FfxFloat16x3(RG.r, RG.g, OutputCacheB[iLDS]);
        }
        #else
        void SetOutputCache(FfxInt32x2 index, FfxFloat32x3 value)
        {
            FfxInt32 iLDS = index.x + index.y * FFX_BLUR_TILE_SIZE_X;
            OutputCacheRG[iLDS] = PackF2(value.rg);
            OutputCacheB[iLDS] = value.b;
        }
        FfxFloat32x3 GetOutputCache(FfxInt32x2 index)
        { 
            FfxInt32 iLDS = index.x + index.y * FFX_BLUR_TILE_SIZE_X;
            FfxFloat32x2 RG = UnpackToF2(OutputCacheRG[iLDS]);
            return FfxFloat32x3(RG.r, RG.g, OutputCacheB[iLDS]);
        }
        #endif // FFX_HALF
    #else
        #if FFX_HALF
        void SetOutputCache(FfxInt32x2 index, FfxFloat16x3 value)
        {
            const FfxUInt32 iLDS = FlattenIndex(index, FFX_BLUR_TILE_SIZE_X);
            OutputCache[iLDS] = value;
        }
        FfxFloat16x3 GetOutputCache(FfxInt32x2 index)
        {
            const FfxUInt32 iLDS = FlattenIndex(index, FFX_BLUR_TILE_SIZE_X);
            return OutputCache[iLDS];
        }
        #else
        void SetOutputCache(FfxInt32x2 index, FfxFloat32x3 value)
        {
            const FfxUInt32 iLDS = FlattenIndex(index, FFX_BLUR_TILE_SIZE_X);
            OutputCache[iLDS] = value;
        }
        FfxFloat32x3 GetOutputCache(FfxInt32x2 index)
        {
            const FfxUInt32 iLDS = FlattenIndex(index, FFX_BLUR_TILE_SIZE_X);
            return OutputCache[iLDS];
        }
        #endif // FFX_HALF
    #endif // BLUR_GROUPSHARED_MEMORY_PK_UINT
    

    #if BLUR_ENABLE_INPUT_CACHE
        #if BLUR_GROUPSHARED_MEMORY_PK_UINT
            #if FFX_HALF
                void SetInputCache(FfxInt32x2 index, FfxFloat16x3 value)
                {
                    FfxInt32 iLDS = FlattenIndex(index, INPUT_CACHE_TILE_SIZE_X);
                    InputCacheRG[iLDS] = PackH2(value.rg);
                    InputCacheB[iLDS] = value.b;
                }
                FfxFloat16x3 GetInputCache(FfxInt32x2 index)
                {
                    FfxInt32 iLDS = FlattenIndex(index, INPUT_CACHE_TILE_SIZE_X);
                    FfxFloat16x2 RG = UnpackToH2(InputCacheRG[iLDS]);
                    return FfxFloat16x3(RG.r, RG.g, InputCacheB[iLDS]);
                }
            #else
                void SetInputCache(FfxInt32x2 index, FfxFloat32x3 value)
                {
                    //FfxInt32 iLDS = FlattenIndex(index, INPUT_CACHE_TILE_SIZE_X);
                    FfxInt32 iLDS = index.x + index.y * INPUT_CACHE_TILE_SIZE_X;
                    InputCacheRG[iLDS] = PackF2(value.rg);
                    InputCacheB[iLDS] = value.b;
                }
                FfxFloat32x3 GetInputCache(FfxInt32x2 index)
                {
                    //FfxInt32 iLDS = FlattenIndex(index, INPUT_CACHE_TILE_SIZE_X);
                    FfxInt32 iLDS = index.x + index.y * INPUT_CACHE_TILE_SIZE_X;
                    
                    FfxFloat32x2 RG = UnpackToF2(InputCacheRG[iLDS]);
                    return FfxFloat32x3(RG.r, RG.g, InputCacheB[iLDS]);
                }
            #endif // FFX_HALF
        #else
            void SetInputCache(FfxInt32x2 index, FfxFloat32x3 value) { InputCache[index.x + index.y * INPUT_CACHE_TILE_SIZE_X].rgb = value; }
            FfxFloat32x3  GetInputCache(FfxInt32x2 index)            { return InputCache[index.x + index.y * INPUT_CACHE_TILE_SIZE_X].rgb; }
        #endif // BLUR_GROUPSHARED_MEMORY_PK_UINT
    #endif // BLUR_ENABLE_INPUT_CACHE

#endif // BLUR_GROUPSHARED_MEMORY_SOA


void LDSBarrier()
{
    FFX_GROUP_MEMORY_BARRIER;
}

// index of the LDS tile in the ring buffer
#if FFX_HALF
    #if BLUR_FP16_KERNEL_LOOPS
    FfxInt16 GetOutputCacheTile(FfxInt16 iTile) { return FAST_MOD16(iTile, FfxInt16(NUM_TILES_OUTPUT_CACHE)); }
    void CacheInOutputTile(FfxInt32x2 threadID, FfxInt16 iTile, FfxFloat16x3 color)
    {
        FfxInt16 iLDSTile = GetOutputCacheTile(iTile);
        FfxInt16x2 TileOffset = FfxInt16x2(0, FfxInt16(FFX_BLUR_TILE_SIZE_Y) * iLDSTile);
        FfxInt16x2 iLDS = FfxInt16x2(threadID) + TileOffset;
        SetOutputCache(iLDS, color);
    }
    FfxFloat16x3 LoadFromCachedOutputTile(FfxInt32x2 threadID, FfxInt16 iTile)
    {
        FfxInt16 iLDSTile = GetOutputCacheTile(iTile);
        FfxInt16x2 TileOffset = FfxInt16x2(0, FfxInt16(FFX_BLUR_TILE_SIZE_Y) * iLDSTile);
        FfxInt16x2 iLDS = FfxInt16x2(threadID) + TileOffset;
        return GetOutputCache(iLDS);
    }
    #else // BLUR_FP16_KERNEL_LOOPS
    FfxInt32 GetOutputCacheTile(FfxInt32 iTile) { return FAST_MOD(iTile, NUM_TILES_OUTPUT_CACHE); }
    void CacheInOutputTile(FfxInt32x2 threadID, FfxInt32 iTile, FfxFloat16x3 color)
    {
        FfxInt32 iLDSTile = GetOutputCacheTile(iTile);
        FfxInt32x2 TileOffset = FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iLDSTile);
        FfxInt32x2 iLDS = threadID + TileOffset;
        SetOutputCache(iLDS, color);
    }
    FfxFloat32x3 LoadFromCachedOutputTile(FfxInt32x2 threadID, FfxInt32 iTile)
    {
        FfxInt32 iLDSTile = GetOutputCacheTile(iTile);
        FfxInt32x2 TileOffset = FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iLDSTile);
        FfxInt32x2 iLDS = threadID + TileOffset;
        return GetOutputCache(iLDS);
    }
    #endif // BLUR_FP16_KERNEL_LOOPS

#else
    FfxInt32 GetOutputCacheTile(FfxInt32 iTile) { return FAST_MOD(iTile, NUM_TILES_OUTPUT_CACHE); }
    void CacheInOutputTile(FfxInt32x2 threadID, FfxInt32 iTile, FfxFloat32x3 color)
    {
        FfxInt32 iLDSTile = GetOutputCacheTile(iTile); // map image tile to LDS ring buffered tiles
        FfxInt32x2 TileOffset = FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iLDSTile); // pixel offset for this tile
        FfxInt32x2 iLDS = threadID + TileOffset; // 2D LDS coord based on local thread ID
        SetOutputCache(iLDS, color);
    }
    FfxFloat32x3 LoadFromCachedOutputTile(FfxInt32x2 threadID, FfxInt32 iTile)
    {
        FfxInt32 iLDSTile = GetOutputCacheTile(iTile); // map image tile to LDS ring buffered tiles
        FfxInt32x2 TileOffset = FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iLDSTile); // pixel offset for this tile
        FfxInt32x2 iLDS = threadID + TileOffset; // 2D LDS coord based on local thread ID
        return GetOutputCache(iLDS);
    }
#endif // FFX_HALF

#if BLUR_ENABLE_INPUT_CACHE
    #if FFX_HALF
        void CacheInInputTile(FfxInt32x2 threadID, FfxInt32 KernelOffset, FfxFloat16x3 c)
    #else
        void CacheInInputTile(FfxInt32x2 threadID, FfxInt32 KernelOffset, FfxFloat32x3 c)
    #endif
        {
            FfxInt32x2 InputCacheCoord = threadID + FfxInt32x2(KernelOffset, 0) + FfxInt32x2(FFX_BLUR_KERNEL_RANGE-1, 0);
            SetInputCache(InputCacheCoord, c);
        }

    #if FFX_HALF
        FfxFloat16x3 LoadFromCachedInputTile(FfxInt32x2 threadID, FfxInt32 KernelOffset)
    #else
        FfxFloat32x3 LoadFromCachedInputTile(FfxInt32x2 threadID, FfxInt32 KernelOffset)
    #endif
        {
            FfxInt32x2 InputCacheCoord = threadID + FfxInt32x2(KernelOffset,0) + FfxInt32x2(FFX_BLUR_KERNEL_RANGE-1, 0);
            return GetInputCache(InputCacheCoord);
        }
#endif // BLUR_ENABLE_INPUT_CACHE


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//==============================================================================================================================
//                                                    BLUR FUNCTIONS
//==============================================================================================================================
#if FFX_HALF
FfxFloat16x3 HorizontalBlurFromTexture(FfxInt32x2 CenterPixelLocation, FfxInt32x2 ImageSize)
{
#if BLUR_FP16_CLAMP // this uses 4 less VGPRs but not faster
    const FfxInt16 ImageSizeClampValueX = FfxInt16(ImageSize.x - 1);
    FfxFloat16x3 BlurredImage = BlurLoadInput(FfxInt16x2(CenterPixelLocation)) * BlurLoadKernelWeight(0);
    for (FfxInt32 i = 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
    {
        FfxInt32x2 Offset = FfxInt32x2(i, 0);
        FfxInt16x2 SampleCoordXX = FfxInt16x2(CenterPixelLocation.x + i, CenterPixelLocation.x - i);
#if !BLUR_DISABLE_CLAMP
        SampleCoordXX = clamp(SampleCoordXX, FfxInt16x2(0, 0), FfxInt16x2(ImageSizeClampValueX, ImageSizeClampValueX));
#endif
        BlurredImage += BlurLoadInput(FfxInt16x2(SampleCoordXX[0], CenterPixelLocation.y)) * BlurLoadKernelWeight(i);
        BlurredImage += BlurLoadInput(FfxInt16x2(SampleCoordXX[1], CenterPixelLocation.y)) * BlurLoadKernelWeight(i);
    }
#else
    FfxFloat16x3 BlurredImage = FfxFloat16x3(0.f, 0.f, 0.f);
    for (FfxInt32 i = -FFX_BLUR_KERNEL_RANGE + 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
    {
        FfxInt32x2 SampleCoord = CenterPixelLocation + FfxInt32x2(i, 0);  // horizontal blur
#if !BLUR_DISABLE_CLAMP
        SampleCoord.x = clamp(SampleCoord.x, 0, ImageSize.x-1); // clamp
#endif
        FfxFloat16x3 c = BlurLoadInput(SampleCoord);
        BlurredImage += c * BlurLoadKernelWeight(abs(i));
    }
#endif // BLUR_FP16_CLAMP
    return BlurredImage;
}
#else // FFX_HALF
FfxFloat32x3 HorizontalBlurFromTexture(FfxInt32x2 CenterPixelLocation, FfxInt32x2 ImageSize)
{
    FfxFloat32x3 BlurredImage = FfxFloat32x3(0.f, 0.f, 0.f);
    for (FfxInt32 i = -FFX_BLUR_KERNEL_RANGE + 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
    {
        FfxInt32x2 SampleCoord = CenterPixelLocation + FfxInt32x2(i, 0);  // horizontal blur
#if !BLUR_DISABLE_CLAMP
        SampleCoord.x = clamp(SampleCoord.x, 0, ImageSize.x-1); // clamp
#endif
        FfxFloat32x3 c = BlurLoadInput(SampleCoord);
        BlurredImage += c * BlurLoadKernelWeight(abs(i));
    }
    return BlurredImage;
}
#endif // FFX_HALF

#if BLUR_ENABLE_INPUT_CACHE
#if FFX_HALF
FfxFloat16x3 HorizontalBlurFromCachedInput(FfxInt32x2 threadID)
#else
FfxFloat32x3 HorizontalBlurFromCachedInput(FfxInt32x2 threadID)
#endif
{
    #if FFX_HALF
    FfxFloat16x3 BlurredImage = FfxFloat16x3(0.f, 0.f, 0.f);
    #else
    FfxFloat32x3 BlurredImage = FfxFloat32x3(0.f, 0.f, 0.f);
    #endif // FFX_HALF

    for (FfxInt32 i = -FFX_BLUR_KERNEL_RANGE + 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
    {
        BlurredImage += LoadFromCachedInputTile(threadID, i) * BlurLoadKernelWeight(abs(i));
    }
    return BlurredImage;
}
#endif // BLUR_ENABLE_INPUT_CACHE

#if FFX_HALF
    #if BLUR_FP16_KERNEL_LOOPS
    FfxFloat16x3 VerticalBlurFromCachedOutput(FfxInt32x2 ThreadID, FfxInt32x2 WorkGroupID,FfxInt16x2 CenterPixelLocation, FfxInt16x2 ImageSize)
    {
        const FfxInt16x2 ImageSizeClampValueXY = ImageSize.xy - FfxInt16x2(1, 1);
        const FfxUInt32 iTileCount = DIV_AND_ROUND_UP(FfxUInt32(ImageSize.y), FFX_BLUR_TILE_SIZE_Y * FFX_BLUR_DISPATCH_Y);
        FfxFloat16x3 value = FfxFloat16x3(0, 0, 0);
        #ifndef FFX_HLSL
        // For some reason using 16 bit integer for this loop in glsl does not work. It seems to be due to the use of
        // a negative value as a starting value that is compared to a positive value, which seems to incorrectly cause
        // the condition to always be false.
        for (FfxInt32 i = (FfxInt32(-FFX_BLUR_KERNEL_RANGE) + FfxInt32(1)); i < FfxInt32(FFX_BLUR_KERNEL_RANGE); ++i)
        #else
        for (FfxInt16 i = (FfxInt16(-FFX_BLUR_KERNEL_RANGE) + FfxInt16(1)); i < FfxInt16(FFX_BLUR_KERNEL_RANGE); ++i)
        #endif
        {
            FfxInt16x2 KernelSampleLocation = CenterPixelLocation + FfxInt16x2(0, i);
#if !BLUR_DISABLE_CLAMP
            KernelSampleLocation.xy = clamp(KernelSampleLocation.xy, FfxInt16x2(0, 0), ImageSizeClampValueXY);
#endif

            const FfxInt16 iTile_ImageSpace = FfxInt16(KernelSampleLocation.y * BLUR_TILE_SIZE_Y_INV);
            const FfxInt16 iTile = FfxInt16(iTile_ImageSpace - iTileCount * WorkGroupID.y);
            FfxInt16x2 TileThreadID = FfxInt16x2(ThreadID.x, FAST_MOD16(KernelSampleLocation.y, FfxInt16(FFX_BLUR_TILE_SIZE_Y)));
    
            FfxFloat16x3 c = LoadFromCachedOutputTile(TileThreadID, iTile);
            value += c * BlurLoadKernelWeight(abs(i));
        }
        return value;
    }
    #else
    FfxFloat16x3 VerticalBlurFromCachedOutput(FfxInt32x2 ThreadID, FfxInt32x2 WorkGroupID, FfxInt32x2 CenterPixelLocation, FfxInt32x2 ImageSize)
    {
        const FfxUInt32 iTileCount = DIV_AND_ROUND_UP(ImageSize.y, FFX_BLUR_TILE_SIZE_Y * FFX_BLUR_DISPATCH_Y);
        FfxFloat16x3 value = FfxFloat16x3(0, 0, 0);
        #if BLUR_FP16_CLAMP
        const FfxInt16x2 ClampUpperLimitXY = ImageSize.xy - 1;
        for (FfxInt16 i = -FFX_BLUR_KERNEL_RANGE + 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
        {
            FfxInt16x2 KernelSampleLocation = CenterPixelLocation + FfxInt16x2(0, i);
#if !BLUR_DISABLE_CLAMP
#if BLUR_OPTIMIZED_CLAMP
            bool bNegative = firstbithigh(KernelSampleLocation.x) == 31;
            KernelSampleLocation.y = bNegative ? 0 : KernelSampleLocation.y;
#else
            KernelSampleLocation.xy = clamp(KernelSampleLocation.xy, FfxInt16x2(0, 0), ClampUpperLimitXY);
#endif // BLUR_OPTIMIZED_CLAMP
#endif
            const FfxInt16 iTile_ImageSpace = FfxInt16(KernelSampleLocation.y * FFX_BLUR_TILE_SIZE_Y_INV);
            const FfxInt16 iTile = iTile_ImageSpace - iTileCount * WorkGroupID.y;
            FfxInt16x2 TileThreadID = FfxInt16x2(ThreadID.x, FAST_MOD16(KernelSampleLocation.y, FfxInt16(FFX_BLUR_TILE_SIZE_Y)));

            FfxFloat16x3 c = LoadFromCachedOutputTile(TileThreadID, iTile);
            value += c * BlurLoadKernelWeight(abs(i));
        }
        #else
        for (FfxInt32 i = -FFX_BLUR_KERNEL_RANGE + 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
        {
            FfxInt32x2 KernelSampleLocation = CenterPixelLocation + FfxInt32x2(0, i);
#if !BLUR_DISABLE_CLAMP
#if BLUR_OPTIMIZED_CLAMP
            bool bNegative = firstbithigh(KernelSampleLocation.x) == 31;
            KernelSampleLocation.y = bNegative ? 0 : KernelSampleLocation.y;
#else
            KernelSampleLocation.xy = clamp(KernelSampleLocation.xy, 0, ImageSize.xy-1);
#endif // BLUR_OPTIMIZED_CLAMP
#endif

            const FfxInt32 iTile_ImageSpace = FfxInt32(KernelSampleLocation.y * FFX_BLUR_TILE_SIZE_Y_INV);
            const FfxInt32 iTile = iTile_ImageSpace - (iTileCountPerWorkgroup * WorkGroupID.y);

            FfxInt32x2 TileThreadID = FfxInt32x2(ThreadID.x, FAST_MOD(KernelSampleLocation.y, FFX_BLUR_TILE_SIZE_Y));
    
            FfxFloat16x3 c = LoadFromCachedOutputTile(TileThreadID, iTile);
            value += c * BlurLoadKernelWeight(abs(i));
        }
        #endif // BLUR_FP16_CLAMP
        return value;
    }
    #endif // BLUR_FP16_KERNEL_LOOPS
#else // FFX_HALF
    FfxFloat32x3 VerticalBlurFromCachedOutput(FfxInt32x2 ThreadID, FfxInt32x2 WorkGroupID,FfxInt32x2 CenterPixelLocation, FfxInt32x2 ImageSize)
    {
        const FfxInt32 iTileCountPerWorkgroup = DIV_AND_ROUND_UP(ImageSize.y, FFX_BLUR_TILE_SIZE_Y * FFX_BLUR_DISPATCH_Y);

        FfxFloat32x3 value = FfxFloat32x3(0,0,0);
        for (FfxInt32 i = -FFX_BLUR_KERNEL_RANGE + 1; i < FFX_BLUR_KERNEL_RANGE; ++i)
        {
            FfxInt32x2 KernelSampleLocation = CenterPixelLocation + FfxInt32x2(0, i);
#if !BLUR_DISABLE_CLAMP
            KernelSampleLocation.xy = clamp(KernelSampleLocation.xy, FfxInt32x2(0, 0), ImageSize.xy-1);
#endif

            // which 'global' tile in the image space
            const FfxInt32 iTile_ImageSpace = FfxInt32(KernelSampleLocation.y * BLUR_TILE_SIZE_Y_INV);

            // local tile in this workgroup - apply the offset to convert to local space tile coordinates
            // this is needed for workgroups that have WorkgroupID.y > 0: the previous workgroup's
            // tile mapping doesn't have to align with the current one's depending on the FFX_BLUR_TILE_SIZE_XY.
            // e.g. WorkGroupID=1's first tile will map to 0 in local space, but could be some non-0 index 
            // in the local space of the previous workgroup (WorkGroupID=0). 
            // Not correcting for this mapping will result in a chopped image on the workgroup borders.
            const FfxInt32 iTile = iTile_ImageSpace - (iTileCountPerWorkgroup * WorkGroupID.y);

            FfxInt32x2 TileThreadID = FfxInt32x2(ThreadID.x, FAST_MOD(KernelSampleLocation.y, FFX_BLUR_TILE_SIZE_Y));
            //FfxInt32x2 TileThreadID = FfxInt32x2(ThreadID.x, KernelSampleLocation.y % BLUR_TILE_SIZE_Y);

            value += LoadFromCachedOutputTile(TileThreadID, iTile) * BlurLoadKernelWeight(abs(i));
        }
        return value;
    }
#endif // FFX_HALF
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//==============================================================================================================================
//                                               INPUT/OUTPUT CACHE HELPERS
//==============================================================================================================================
//     
#if BLUR_ENABLE_INPUT_CACHE
// Fills the input cache with the corresponding rgion from the image + kernel extents
    void FillInputCache(in FfxInt32x2 lxy, FfxInt32x2 CenterPixelLocation, FfxInt32x2 ImageSize)
    {
#if FFX_HALF
        FfxFloat16x3 c = FfxFloat16x3(0, 0, 0);
#else
        FfxFloat32x3 c = FfxFloat32x3(0, 0, 0);
#endif

        // slide the thread group over the InputCache
        const FfxInt32 iNumLoops = DIV_AND_ROUND_UP(INPUT_CACHE_TILE_SIZE_X, FFX_BLUR_TILE_SIZE_X);
        for (FfxInt32 i = 0; i < iNumLoops; ++i)
        {
            FfxInt32x2 LDSCoord = lxy - FfxInt32x2(FFX_BLUR_KERNEL_RANGE - 1, 0) + FfxInt32x2(i * FFX_BLUR_TILE_SIZE_X, 0);
            FfxInt32x2 SamplePosition = CenterPixelLocation - FfxInt32x2(FFX_BLUR_KERNEL_RANGE - 1, 0) + FfxInt32x2(i * FFX_BLUR_TILE_SIZE_X, 0);
#if !BLUR_DISABLE_CLAMP
            SamplePosition.x = clamp(SamplePosition.x, 0, ImageSize.x - 1);
#endif
            c = BlurLoadInput(SamplePosition);

            // clamp to LDS bounds if we're on the last iteration of the loop
            if (i == iNumLoops - 1)
            {
                // faster than 'if (LDSCoord.x < INPUT_CACHE_TILE_SIZE_X)', avoids a vmem sync at the cost of some ALU
                LDSCoord.x = clamp(LDSCoord.x, -FfxInt32x2(FFX_BLUR_KERNEL_RANGE - 1, 0), (INPUT_CACHE_TILE_SIZE_X - 1) - FfxInt32x2(FFX_BLUR_KERNEL_RANGE - 1, 0));
            }
            CacheInInputTile(LDSCoord, 0, c);
        }
        LDSBarrier();
    }
#endif // BLUR_ENABLE_INPUT_CACHE
// Fills the output cache with the horizontally-blurred image.
void PreFillOutputCache(in FfxInt32x2 gxy, in FfxInt32x2 lxy, in FfxInt32x2 WorkGroupID, FfxInt32x2 ImageSize)
{
#if BLUR_DEBUG_PREFILL_OUTPUT_CACHE_WITH_COLOR
    FfxFloat32x3 FillColor = FfxFloat32x3(0, 0, 0); // black border color
    [unroll]
    for (FfxInt32 iTile = 0; iTile < NUM_TILES_OUTPUT_CACHE; ++iTile)
    {
        CacheInOutputTile(lxy, iTile, FillColor);
    }
    LDSBarrier();
#endif

    // load from VMEM the first NUM_PREFILL_TILES_OUTPUT_CACHE tiles 
    // while doing the horizontal blur, going top down
#if FFX_HALF
    for (FfxInt16 j = FfxInt16(0); j < FfxInt16(NUM_PREFILL_TILES_OUTPUT_CACHE); ++j)
#else
    for (FfxInt32 j = 0; j < NUM_PREFILL_TILES_OUTPUT_CACHE; ++j)
#endif
    {
        const FfxInt32x2 ImageCoordinate = gxy + FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * j);
#if FFX_HALF
        FfxFloat16x3 c = HorizontalBlurFromTexture(ImageCoordinate, ImageSize);
#else
        FfxFloat32x3 c = HorizontalBlurFromTexture(ImageCoordinate, ImageSize);
#endif
        CacheInOutputTile(lxy, j, c);
    }

#if FFX_BLUR_DISPATCH_Y != 1
    // for any workgroup that doesn't start frop the top of the image,
    // fill the cache from the tail, going upwards in the image space
    if (WorkGroupID.y != 0)
    {

#if FFX_HALF
        FfxFloat16x3 c = FfxFloat16x3(0, 0, 0);
#else
        FfxFloat32x3 c = FfxFloat32x3(0, 0, 0);
#endif
#if FFX_HALF
        for (FfxInt16 j = FfxInt16(1); j < FfxInt16(NUM_PREFILL_TILES_OUTPUT_CACHE+1 + (DIV_AND_ROUND_UP(FFX_BLUR_KERNEL_RANGE, FFX_BLUR_TILE_SIZE_Y))); ++j)
#else
        for (FfxInt32 j = 1; j < NUM_PREFILL_TILES_OUTPUT_CACHE+1 + (DIV_AND_ROUND_UP(FFX_BLUR_KERNEL_RANGE, FFX_BLUR_TILE_SIZE_Y)); ++j)
#endif
        {
            const FfxInt32x2 ImageCoordinate = gxy - FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * j);

        #if BLUR_ENABLE_INPUT_CACHE
            FillInputCache(lxy, ImageCoordinate, ImageSize);
            c = HorizontalBlurFromCachedInput(lxy);
        #else
            c = HorizontalBlurFromTexture(ImageCoordinate, ImageSize);
        #endif

#if FFX_HALF
            CacheInOutputTile(lxy, (FfxInt16(NUM_TILES_OUTPUT_CACHE) - j), c);
#else
            CacheInOutputTile(lxy, (NUM_TILES_OUTPUT_CACHE - j), c);
#endif
        }
    }
#endif

    LDSBarrier();  // OutputCache Sync: Read -> Write  =========================================
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//==============================================================================================================================
//                                                    BLUR GAUSSIAN BLUR ALGORITHM
//==============================================================================================================================

/// ffxBlur: The main idea of the algorithm is to utilize a number of tiles (8x8) that are cached on the groupshared memory
/// in a ring-buffer fashion to speed up texture lookups in a hand-optimized compute shader.
/// The tiles are defined by the FFX_BLUR_TILE_SIZE_X and FFX_BLUR_TILE_SIZE_Y defines, and are typically 8x8 pixels.
/// The image is horizontally blurred while being cached on the groupshared memory, 
/// and when all the groupshared tiles are filled, a vertical blur pass is done on the groupshared memory
/// and the result is stored in the UAV as the final destination.
///
/// The algorithm is as follows:
/// - Pre-fill LDS with 8x8 tiles, storing vertical tiles, containing horizontally blurred color
/// - Loop until the entire image is covered:
///   - Run a vertical blur pass on the LDS and output to final destination UAV
///   - Re-fill LDS with horizontally-blurred data
/// - Finish off the remaining last row/section of the image
///
/// @param [in] GlobalThreadID           The SV_DispatchThreadID.xy or gl_GlobalInvocationID.xy.
/// @param [in] WorkGroupLocalThreadID   The SV_GroupThreadID.xy or gl_LocalInvocationID.xy.
/// @param [in] WorkGroupID              The SV_GroupID.xy or gl_WorkGroupID.xy.
/// @param [in] ImageSize                The two dimensional size of the input and output image.
///
/// @ingroup FfxGPUBlur
void ffxBlur(
    in FfxInt32x2 GlobalThreadID,
    in FfxInt32x2 WorkGroupLocalThreadID,
    in FfxInt32x2 WorkGroupID,
    FfxInt32x2 ImageSize)
{
    // Each threadgroup processes a number of tiles of size FFX_BLUR_TILE_SIZE_Y * FFX_BLUR_TILE_SIZE_X
    // This number depends on the image height and the vertical dimension (_Y) of the FFX_BLUR_TILE_SIZE
    //const FfxUInt32 iTileCount = DIV_AND_ROUND_UP(ImageSize.y, FFX_BLUR_TILE_SIZE_Y);
    const FfxUInt32 iTileCount =
        DIV_AND_ROUND_UP(
            ImageSize.y,
            FFX_BLUR_TILE_SIZE_Y * FFX_BLUR_DISPATCH_Y);
    
    FfxInt32x2 gxy = FfxInt32x2(
        WorkGroupID.x * FFX_BLUR_TILE_SIZE_X + WorkGroupLocalThreadID.x
        , WorkGroupLocalThreadID.y + WorkGroupID.y * iTileCount * FFX_BLUR_TILE_SIZE_Y
    );
    FfxInt32x2 lxy = WorkGroupLocalThreadID;

    if (gxy.x >= ImageSize.x)
        return;

    //-------------------------------------------------------------------------------------------------
    // STEP #1
    //------------------------------------------------------------------------------------------------- 
    // Pre-fill the output cache with a few tiles of horizontally blurred image.
    // The tile count to pre-fill is a function of kernel width and TileSizeY.
    PreFillOutputCache(gxy, lxy, WorkGroupID, ImageSize); // doesn't sync waves


    //-------------------------------------------------------------------------------------------------
    // STEP #2
    //-------------------------------------------------------------------------------------------------
    // loop through the tiles and write out to UAV as we go from top to down
    FfxInt32 iTileOutput = 0;
    
#if FFX_HALF
    FfxFloat16x3 c = FfxFloat16x3(0, 0, 0);
#else
    FfxFloat32x3 c = FfxFloat32x3(0, 0, 0);
#endif
    for (; iTileOutput < iTileCount - NUM_PREFILL_TILES_OUTPUT_CACHE; ++iTileOutput)
    {
        // index of next tile that we'll cache the output to
        // It runs ahead of the tile we will be writing out to UAV by NUM_PREFILL_TILES_OUTPUT_CACHE tiles
        FfxInt32 iNextTileOutputCache = iTileOutput + NUM_PREFILL_TILES_OUTPUT_CACHE;
        const FfxInt32x2 HorizontalBlurInputCoord = gxy + FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iNextTileOutputCache);

        // run horizontal blur & cache the next output tile
        #if BLUR_ENABLE_INPUT_CACHE
            FillInputCache(lxy, HorizontalBlurInputCoord, ImageSize);
            c = HorizontalBlurFromCachedInput(lxy);
        #else
            // Number of image_load instructions will scale with FFX_BLUR_OPTION_KERNEL_DIMENSION.
            c = HorizontalBlurFromTexture(HorizontalBlurInputCoord, ImageSize);
        #endif // BLUR_ENABLE_INPUT_CACHE

#if FFX_HALF
        CacheInOutputTile(lxy, FfxInt16(iNextTileOutputCache), c);
#else
        CacheInOutputTile(lxy, iNextTileOutputCache, c);
#endif
        
        LDSBarrier(); // OutputCache Sync: Write -> Read =========================================

        // Start writing out the pixel value which has its final value
        // convolved from the pixels aready in the LDS section.
        const FfxInt32x2 OutputCoord = gxy + FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iTileOutput);
#if FFX_HALF
        c = VerticalBlurFromCachedOutput(lxy, WorkGroupID,  FfxInt16x2(OutputCoord), FfxInt16x2(ImageSize));
#else
        c = VerticalBlurFromCachedOutput(lxy, WorkGroupID,  OutputCoord, ImageSize);
#endif
        BlurStoreOutput(OutputCoord, c);
        LDSBarrier();  // OutputCache Sync: Read -> Write  =========================================
    }


    //-------------------------------------------------------------------------------------------------
    // STEP #3
    //-------------------------------------------------------------------------------------------------
    // fill in the remaining last tiles (= loop for NUM_PREFILL_TILES_OUTPUT_CACHE)
    for (; iTileOutput < iTileCount; ++iTileOutput)
    {
        const FfxInt32x2 OutputCoord = gxy + FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iTileOutput);
        if (iTileOutput >= iTileCount - NUM_PREFILL_TILES_OUTPUT_CACHE)
        {
            FfxInt32 iNextTileOutputCache = iTileOutput + NUM_PREFILL_TILES_OUTPUT_CACHE;
            const FfxInt32x2 HorizontalBlurInputCoord = gxy + FfxInt32x2(0, FFX_BLUR_TILE_SIZE_Y * iNextTileOutputCache);

            // run horizontal blur & cache the next output tile
#if BLUR_ENABLE_INPUT_CACHE
            FillInputCache(lxy, HorizontalBlurInputCoord, ImageSize);
            c = HorizontalBlurFromCachedInput(lxy);
#else
    // Number of image_load instructions will scale with FFX_BLUR_OPTION_KERNEL_DIMENSION.
            c = HorizontalBlurFromTexture(HorizontalBlurInputCoord, ImageSize);
#endif // BLUR_ENABLE_INPUT_CACHE

#if FFX_HALF
            CacheInOutputTile(lxy, FfxInt16(iNextTileOutputCache), c);
#else
            CacheInOutputTile(lxy, iNextTileOutputCache, c);
#endif

            LDSBarrier(); // OutputCache Sync: Write -> Read =========================================
        }
#if FFX_HALF
        c = VerticalBlurFromCachedOutput(lxy, WorkGroupID, FfxInt16x2(OutputCoord), FfxInt16x2(ImageSize));
#else
        c = VerticalBlurFromCachedOutput(lxy, WorkGroupID, OutputCoord, ImageSize);
#endif
        BlurStoreOutput(OutputCoord, c);
    }
}

#endif // !FFX_CPU
#endif // FFX_BLUR_H
