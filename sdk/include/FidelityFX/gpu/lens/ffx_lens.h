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

/// @defgroup FfxGPULens FidelityFX Lens
/// FidelityFX Lens GPU documentation
///
/// @ingroup FfxGPUEffects

// Noise function used as basis for film grain effect
FfxUInt32x3 pcg3d16(FfxUInt32x3 v)
{
    v = v * 12829u + 47989u;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;
    v >>= 16u;
    return v;
}

// Simplex noise, transforms given position onto triangle grid
// This logic should be kept at 32-bit floating point precision. 16 bits causes artifacting.
FfxFloat32x2 simplex(const in FfxFloat32x2 P)
{
    // Skew and unskew factors are a bit hairy for 2D, so define them as constants
    const FfxFloat32 F2 = (sqrt(3.0) - 1.0) / 2.0;  // 0.36602540378
    const FfxFloat32 G2 = (3.0 - sqrt(3.0)) / 6.0;  // 0.2113248654

    // Skew the (x,y) space to determine which cell of 2 simplices we're in
    FfxFloat32   u   = (P.x + P.y) * F2;
    FfxFloat32x2 Pi  = round(P + u);
    FfxFloat32   v   = (Pi.x + Pi.y) * G2;
    FfxFloat32x2 P0  = Pi - v;  // Unskew the cell origin back to (x,y) space
    FfxFloat32x2 Pf0 = P - P0;  // The x,y distances from the cell origin

    return FfxFloat32x2(Pf0);
}

#if FFX_HALF

FfxFloat16x2 toFloat16(FfxUInt32x2 inputVal)
{
    return FfxFloat16x2(inputVal * (1.0 / 65536.0) - 0.5);
}

FfxFloat16x3 toFloat16(FfxUInt32x3 inputVal)
{
    return FfxFloat16x3(inputVal * (1.0 / 65536.0) - 0.5);
}

/// Function call to calculate the red and green wavelength/channel sample offset values.
///
/// @param chromAbIntensity Intensity constant value for the chromatic aberration effect.
/// @return FfxFloat32x2 containing the red and green wavelength/channel magnitude values
/// @ingroup FfxGPULens
FfxFloat16x2 FfxLensGetRGMag(FfxFloat16 chromAbIntensity)
{
    const FfxFloat16 A = FfxFloat16(1.5220);
    const FfxFloat16 B = FfxFloat16(0.00459) * chromAbIntensity;  // um^2

    const FfxFloat16 redWaveLengthUM   = FfxFloat16(0.612 * 0.612);
    const FfxFloat16 greenWaveLengthUM = FfxFloat16(0.549 * 0.549);
    const FfxFloat16 blueWaveLengthUM  = FfxFloat16(0.464 * 0.464);

    const FfxFloat16 redIdxRefraction   = A + B / (redWaveLengthUM);
    const FfxFloat16 greenIdxRefraction = A + B / (greenWaveLengthUM);
    const FfxFloat16 blueIdxRefraction  = A + B / (blueWaveLengthUM);

    const FfxFloat16 redMag   = (redIdxRefraction - FfxFloat16(1.0)) / (blueIdxRefraction - FfxFloat16(1.0));
    const FfxFloat16 greenMag = (greenIdxRefraction - FfxFloat16(1.0)) / (blueIdxRefraction - FfxFloat16(1.0));

    return FfxFloat16x2(redMag, greenMag);
}

/// Function call to apply chromatic aberration effect when sampling the color input texture.
///
/// @param coord The input window coordinate [0, widthPixels), [0, heightPixels).
/// @param centerCoord The center window coordinate of the screen.
/// @param redMag Magnitude value for the offset calculation of the red wavelength (texture channel).
/// @param greenMag Magnitude value for the offset calculation of the green wavelength (texture channel).
/// @return The final sampled RGB color.
/// @ingroup FfxGPULens
FfxFloat16x3 FfxLensSampleWithChromaticAberration(FfxInt32x2 coord, FfxInt32x2 centerCoord, FfxFloat16 redMag, FfxFloat16 greenMag)
{
    FfxFloat16x2 coordfp16 = FfxFloat16x2(coord);
    FfxFloat16x2 centerCoordfp16 = FfxFloat16x2(centerCoord);
    FfxFloat16x2 redShift        = (coordfp16 - centerCoordfp16) * redMag + centerCoordfp16 + FfxFloat16x2(0.5, 0.5);
    redShift *= FfxFloat16x2(ffxReciprocal(FfxFloat16(2.0) * centerCoordfp16));
    FfxFloat16x2 greenShift = (coordfp16 - centerCoordfp16) * greenMag + centerCoordfp16 + FfxFloat16x2(0.5, 0.5);
    greenShift *= FfxFloat16x2(ffxReciprocal(FfxFloat16(2.0) * centerCoordfp16));

    FfxFloat16 red   = FfxLensSampleR(redShift);
    FfxFloat16 green = FfxLensSampleG(greenShift);
    FfxFloat16 blue  = FfxLensSampleB(coordfp16 * ffxReciprocal(FfxFloat16(2.0) * centerCoordfp16));

    return FfxFloat16x3(red, green, blue);
}

/// Function call to apply film grain effect to inout color. This call could be skipped entirely as the choice to use the film grain is optional.
///
/// @param coord The input window coordinate [0, widthPixels), [0, heightPixels).
/// @param color The current running color, or more clearly, the sampled input color texture color after being modified by chromatic aberration function.
/// @param grainScaleVal Scaling constant value for the grain's noise frequency.
/// @param grainAmountVal Intensity constant value of the grain effect.
/// @param grainSeedVal Seed value for the grain noise, for example, to change how the noise functions effect the grain frame to frame.
/// @ingroup FfxGPULens
void FfxLensApplyFilmGrain(FfxInt32x2 coord, inout FfxFloat16x3 color, FfxFloat16 grainScaleVal, FfxFloat16 grainAmountVal, uint grainSeedVal)
{
    FfxFloat32x2     randomNumberFine = toFloat16(pcg3d16(FfxUInt32x3(FfxFloat32x2(coord) / (FfxFloat32(grainScaleVal / 8.0)), grainSeedVal)).xy).xy;
    FfxFloat16x2     simplexP         = FfxFloat16x2(simplex(FfxFloat32x2(coord) / FfxFloat32(grainScaleVal) + randomNumberFine));
    const FfxFloat16 grainShape       = FfxFloat16(3.0);

    FfxFloat16 grain = FfxFloat16(1.0) - FfxFloat16(2.0) * exp2(-length(simplexP) * grainShape);

    color += grain * min(color, FfxFloat16x3(1.0, 1.0, 1.0) - color) * grainAmountVal;
}

/// Function call to apply vignette effect to inout color. This call could be skipped entirely as the choice to use the vignette is optional.
///
/// @param coord The input window coordinate [0, widthPixels), [0, heightPixels).
/// @param centerCoord The center window coordinate of the screen.
/// @param color The current running color, or more clearly, the sampled input color texture color after being modified by chromatic aberration and film grain functions.
/// @param vignetteAmount Intensity constant value of the vignette effect.
/// @ingroup FfxGPULens
void FfxLensApplyVignette(FfxInt32x2 coord, FfxInt32x2 centerCoord, inout FfxFloat16x3 color, FfxFloat16 vignetteAmount)
{
    FfxFloat16x2 vignetteMask    = FfxFloat16x2(0.0, 0.0);
    FfxFloat16x2 coordFromCenter = FfxFloat16x2(abs(coord - centerCoord)) / FfxFloat16x2(centerCoord);

    const FfxFloat16 piOver4 = FfxFloat16(FFX_PI * 0.25);
    vignetteMask = FfxFloat16x2(cos(coordFromCenter * vignetteAmount * piOver4));
    vignetteMask = vignetteMask * vignetteMask;
    vignetteMask = vignetteMask * vignetteMask;

    FfxFloat16 vignetteMaskClamped = FfxFloat16(clamp(vignetteMask.x * vignetteMask.y, 0, 1));
    color *= FfxFloat16x3(vignetteMaskClamped, vignetteMaskClamped, vignetteMaskClamped);
}

#else // FFX_HALF

FfxFloat32x2 toFloat16(FfxUInt32x2 inputVal)
{
    return FfxFloat32x2(inputVal * (1.0 / 65536.0) - 0.5);
}

FfxFloat32x3 toFloat16(FfxUInt32x3 inputVal)
{
    return FfxFloat32x3(inputVal * (1.0 / 65536.0) - 0.5);
}

/// Function call to calculate the red and green wavelength/channel sample offset values.
///
/// @param chromAbIntensity Intensity constant value for the chromatic aberration effect.
/// @return FfxFloat32x2 containing the red and green wavelength/channel magnitude values
/// @ingroup FfxGPULens
FfxFloat32x2 FfxLensGetRGMag(FfxFloat32 chromAbIntensity)
{
    const FfxFloat32 A = 1.5220;
    const FfxFloat32 B = 0.00459 * chromAbIntensity;  // um^2

    const FfxFloat32 redWaveLengthUM   = 0.612;
    const FfxFloat32 greenWaveLengthUM = 0.549;
    const FfxFloat32 blueWaveLengthUM  = 0.464;

    const FfxFloat32 redIdxRefraction   = A + B / (redWaveLengthUM * redWaveLengthUM);
    const FfxFloat32 greenIdxRefraction = A + B / (greenWaveLengthUM * greenWaveLengthUM);
    const FfxFloat32 blueIdxRefraction  = A + B / (blueWaveLengthUM * blueWaveLengthUM);

    const FfxFloat32 redMag   = (redIdxRefraction - 1) / (blueIdxRefraction - 1);
    const FfxFloat32 greenMag = (greenIdxRefraction - 1) / (blueIdxRefraction - 1);

    return FfxFloat32x2(redMag, greenMag);
}

/// Function call to apply chromatic aberration effect when sampling the color input texture.
///
/// @param coord The input window coordinate [0, widthPixels), [0, heightPixels).
/// @param centerCoord The center window coordinate of the screen.
/// @param redMag Magnitude value for the offset calculation of the red wavelength (texture channel).
/// @param greenMag Magnitude value for the offset calculation of the green wavelength (texture channel).
/// @return The final sampled RGB color.
/// @ingroup FfxGPULens
FfxFloat32x3 FfxLensSampleWithChromaticAberration(FfxInt32x2 coord, FfxInt32x2 centerCoord, FfxFloat32 redMag, FfxFloat32 greenMag)
{
    FfxFloat32x2 redShift = (coord - centerCoord) * redMag + centerCoord + 0.5;
    redShift *= ffxReciprocal(2 * centerCoord);
    FfxFloat32x2 greenShift = (coord - centerCoord) * greenMag + centerCoord + 0.5;
    greenShift *= ffxReciprocal(2 * centerCoord);

    FfxFloat32 red   = FfxLensSampleR(redShift);
    FfxFloat32 green = FfxLensSampleG(greenShift);
    FfxFloat32 blue  = FfxLensSampleB(coord * ffxReciprocal(2 * centerCoord));

    return FfxFloat32x3(red, green, blue);
}

/// Function call to apply film grain effect to inout color. This call could be skipped entirely as the choice to use the film grain is optional.
///
/// @param coord The input window coordinate [0, widthPixels), [0, heightPixels).
/// @param color The current running color, or more clearly, the sampled input color texture color after being modified by chromatic aberration function.
/// @param grainScaleVal Scaling constant value for the grain's noise frequency.
/// @param grainAmountVal Intensity constant value of the grain effect.
/// @param grainSeedVal Seed value for the grain noise, for example, to change how the noise functions effect the grain frame to frame.
/// @ingroup FfxGPULens
void FfxLensApplyFilmGrain(FfxInt32x2 coord, inout FfxFloat32x3 color, FfxFloat32 grainScaleVal, FfxFloat32 grainAmountVal, uint grainSeedVal)
{
    FfxFloat32x2     randomNumberFine = toFloat16(pcg3d16(FfxUInt32x3(coord / (grainScaleVal / 8), grainSeedVal)).xy).xy;
    FfxFloat32x2     simplexP         = simplex(coord / grainScaleVal + randomNumberFine);
    const FfxFloat32 grainShape       = 3;

    FfxFloat32 grain = 1 - 2 * exp2(-length(simplexP) * grainShape);

    color += grain * min(color, 1 - color) * grainAmountVal;
}

/// Function call to apply vignette effect to inout color. This call could be skipped entirely as the choice to use the vignette is optional.
///
/// @param coord The input window coordinate [0, widthPixels), [0, heightPixels).
/// @param centerCoord The center window coordinate of the screen.
/// @param color The current running color, or more clearly, the sampled input color texture color after being modified by chromatic aberration and film grain functions.
/// @param vignetteAmount Intensity constant value of the vignette effect.
/// @ingroup FfxGPULens
void FfxLensApplyVignette(FfxInt32x2 coord, FfxInt32x2 centerCoord, inout FfxFloat32x3 color, FfxFloat32 vignetteAmount)
{
    FfxFloat32x2 vignetteMask    = FfxFloat32x2(0.0, 0.0);
    FfxFloat32x2 coordFromCenter = abs(coord - centerCoord) / FfxFloat32x2(centerCoord);

    const FfxFloat32 piOver4 = FFX_PI * 0.25;
    vignetteMask = cos(coordFromCenter * vignetteAmount * piOver4);
    vignetteMask = vignetteMask * vignetteMask;
    vignetteMask = vignetteMask * vignetteMask;

    color *= clamp(vignetteMask.x * vignetteMask.y, 0, 1);
}

#endif

/// Lens pass entry point.
///
/// @param Gtid Thread index within thread group (SV_GroupThreadID).
/// @param Gidx Group index of thread (SV_GroupID).
/// @ingroup FfxGPULens
void FfxLens(FfxUInt32 Gtid, FfxUInt32x2 Gidx)
{
    // Do remapping of local xy in workgroup for a more PS-like swizzle pattern.
    // Assumes 64,1,1 threadgroup size and an 8x8 api dispatch
    FfxInt32x2 Coord = FfxInt32x2(ffxRemapForWaveReduction(Gtid) + FfxUInt32x2(Gidx.x << 3u, Gidx.y << 3u));

    // Run Lens
#if FFX_HALF
    FfxFloat16x2 RGMag = FfxLensGetRGMag(FfxFloat16(ChromAb()));
    FfxFloat16x3 Color = FfxLensSampleWithChromaticAberration(Coord, FfxInt32x2(Center()), RGMag.r, RGMag.g);
    FfxLensApplyVignette(Coord, FfxInt32x2(Center()), Color, FfxFloat16(Vignette()));
    FfxLensApplyFilmGrain(Coord, Color, FfxFloat16(GrainScale()), FfxFloat16(GrainAmount()), GrainSeed());
#else // FFX_HALF
    FfxFloat32x2 RGMag = FfxLensGetRGMag(ChromAb());
    FfxFloat32x3 Color = FfxLensSampleWithChromaticAberration(Coord, FfxInt32x2(Center()), RGMag.r, RGMag.g);
    FfxLensApplyVignette(Coord, FfxInt32x2(Center()), Color, Vignette());
    FfxLensApplyFilmGrain(Coord, Color, GrainScale(), GrainAmount(), GrainSeed());
#endif

    StoreLensOutput(Coord, Color);
}
