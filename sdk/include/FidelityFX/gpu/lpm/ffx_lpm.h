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

/// @defgroup FfxGPULpm FidelityFX LPM
/// FidelityFX Luma Preserving Mapper GPU documentation
///
/// @ingroup FfxGPUEffects

FFX_STATIC void LpmMatInv3x3(FFX_PARAMETER_OUT FfxFloat32x3 ox, FFX_PARAMETER_OUT FfxFloat32x3 oy, FFX_PARAMETER_OUT FfxFloat32x3 oz,
    FfxFloat32x3 ix, FfxFloat32x3 iy, FfxFloat32x3 iz)
{
    FfxFloat32 i = ffxReciprocal(ix[0] * (iy[1] * iz[2] - iz[1] * iy[2]) - ix[1] * (iy[0] * iz[2] - iy[2] * iz[0]) + ix[2] * (iy[0] * iz[1] - iy[1] * iz[0]));
    ox[0]        = (iy[1] * iz[2] - iz[1] * iy[2]) * i;
    ox[1]        = (ix[2] * iz[1] - ix[1] * iz[2]) * i;
    ox[2]        = (ix[1] * iy[2] - ix[2] * iy[1]) * i;
    oy[0]        = (iy[2] * iz[0] - iy[0] * iz[2]) * i;
    oy[1]        = (ix[0] * iz[2] - ix[2] * iz[0]) * i;
    oy[2]        = (iy[0] * ix[2] - ix[0] * iy[2]) * i;
    oz[0]        = (iy[0] * iz[1] - iz[0] * iy[1]) * i;
    oz[1]        = (iz[0] * ix[1] - ix[0] * iz[1]) * i;
    oz[2]        = (ix[0] * iy[1] - iy[0] * ix[1]) * i;
}

// Transpose.
FFX_STATIC void LpmMatTrn3x3(FFX_PARAMETER_OUT FfxFloat32x3 ox, FFX_PARAMETER_OUT FfxFloat32x3 oy, FFX_PARAMETER_OUT FfxFloat32x3 oz,
    FfxFloat32x3 ix, FfxFloat32x3 iy, FfxFloat32x3 iz)
{
    ox[0] = ix[0];
    ox[1] = iy[0];
    ox[2] = iz[0];
    oy[0] = ix[1];
    oy[1] = iy[1];
    oy[2] = iz[1];
    oz[0] = ix[2];
    oz[1] = iy[2];
    oz[2] = iz[2];
}

FFX_STATIC void LpmMatMul3x3(
    FFX_PARAMETER_OUT FfxFloat32x3 ox, FFX_PARAMETER_OUT FfxFloat32x3 oy, FFX_PARAMETER_OUT FfxFloat32x3 oz,
    FfxFloat32x3 ax, FfxFloat32x3 ay, FfxFloat32x3 az, FfxFloat32x3 bx, FfxFloat32x3 by, FfxFloat32x3 bz)
{
    FfxFloat32x3 bx2;
    FfxFloat32x3 by2;
    FfxFloat32x3 bz2;

    LpmMatTrn3x3(bx2, by2, bz2, bx, by, bz);
    ox[0] = ffxDot3(ax, bx2);
    ox[1] = ffxDot3(ax, by2);
    ox[2] = ffxDot3(ax, bz2);
    oy[0] = ffxDot3(ay, bx2);
    oy[1] = ffxDot3(ay, by2);
    oy[2] = ffxDot3(ay, bz2);
    oz[0] = ffxDot3(az, bx2);
    oz[1] = ffxDot3(az, by2);
    oz[2] = ffxDot3(az, bz2);
}

// D65 xy coordinates.
FFX_STATIC FfxFloat32x2 lpmColD65 = {FfxFloat32(0.3127), FfxFloat32(0.3290)};

// Rec709 xy coordinates, (D65 white point).
FFX_STATIC FfxFloat32x2 lpmCol709R = {FfxFloat32(0.64), FfxFloat32(0.33)};
FFX_STATIC FfxFloat32x2 lpmCol709G = {FfxFloat32(0.30), FfxFloat32(0.60)};
FFX_STATIC FfxFloat32x2 lpmCol709B = {FfxFloat32(0.15), FfxFloat32(0.06)};

// DCI-P3 xy coordinates, (D65 white point).
FFX_STATIC FfxFloat32x2 lpmColP3R = {FfxFloat32(0.680), FfxFloat32(0.320)};
FFX_STATIC FfxFloat32x2 lpmColP3G = {FfxFloat32(0.265), FfxFloat32(0.690)};
FFX_STATIC FfxFloat32x2 lpmColP3B = {FfxFloat32(0.150), FfxFloat32(0.060)};

// Rec2020 xy coordinates, (D65 white point).
FFX_STATIC FfxFloat32x2 lpmCol2020R = {FfxFloat32(0.708), FfxFloat32(0.292)};
FFX_STATIC FfxFloat32x2 lpmCol2020G = {FfxFloat32(0.170), FfxFloat32(0.797)};
FFX_STATIC FfxFloat32x2 lpmCol2020B = {FfxFloat32(0.131), FfxFloat32(0.046)};

// Computes z from xy, returns xyz.
FFX_STATIC void LpmColXyToZ(FFX_PARAMETER_OUT FfxFloat32x3 d, FfxFloat32x2 s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = FfxFloat32(1.0) - (s[0] + s[1]);
}

// Returns conversion matrix, rgbw inputs are xy chroma coordinates.
FFX_STATIC void LpmColRgbToXyz(FFX_PARAMETER_OUT FfxFloat32x3 ox, FFX_PARAMETER_OUT FfxFloat32x3 oy, FFX_PARAMETER_OUT FfxFloat32x3 oz,
    FfxFloat32x2 r, FfxFloat32x2 g, FfxFloat32x2 b, FfxFloat32x2 w)
{
    // Expand from xy to xyz.
    FfxFloat32x3 rz;
    FfxFloat32x3 gz;
    FfxFloat32x3 bz;
    LpmColXyToZ(rz, r);
    LpmColXyToZ(gz, g);
    LpmColXyToZ(bz, b);

    FfxFloat32x3 r3;
    FfxFloat32x3 g3;
    FfxFloat32x3 b3;
    LpmMatTrn3x3(r3, g3, b3, rz, gz, bz);

    // Convert white xyz to XYZ.
    FfxFloat32x3 w3;
    LpmColXyToZ(w3, w);
    ffxOpAMulOneF3(w3, w3, ffxReciprocal(w[1]));

    // Compute xyz to XYZ scalars for primaries.
    FfxFloat32x3 rv;
    FfxFloat32x3 gv;
    FfxFloat32x3 bv;
    LpmMatInv3x3(rv, gv, bv, r3, g3, b3);

    FfxFloat32x3 s;
    s[0] = ffxDot3(rv, w3);
    s[1] = ffxDot3(gv, w3);
    s[2] = ffxDot3(bv, w3);

    // Scale.
    ffxOpAMulF3(ox, r3, s);
    ffxOpAMulF3(oy, g3, s);
    ffxOpAMulF3(oz, b3, s);
}

#if defined(LPM_NO_SETUP)
FFX_STATIC void LpmSetupOut(FfxUInt32 i, FfxUInt32x4 v)
{
}
#endif  // #if defined(LPM_NO_SETUP)

/// Setup required constant values for LPM (works on CPU or GPU).
/// Output goes to the user-defined LpmSetupOut() function.
///
/// @param [in] shoulder             Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).
/// @param [in] con                  Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
/// @param [in] soft                 Use soft gamut mapping.
/// @param [in] con2                 Use last RGB conversion matrix.
/// @param [in] clip                 Use clipping in last conversion matrix.
/// @param [in] scaleOnly            Scale only for last conversion matrix (used for 709 HDR to scRGB).
/// @param [in] xyRedW               Red Chroma coordinates for working color space.
/// @param [in] xyGreenW             Green Chroma coordinates for working color space.
/// @param [in] xyBlueW              Blue Chroma coordinates for working color space.
/// @param [in] xyWhiteW             White Chroma coordinates for working color space.
/// @param [in] xyRedO               Red Chroma coordinates for output color space.
/// @param [in] xyGreenO             Green Chroma coordinates for output color space.
/// @param [in] xyBlueO              Blue Chroma coordinates for output color space.
/// @param [in] xyWhiteO             White Chroma coordinates for output color space.
/// @param [in] xyRedC               Red Chroma coordinates for output container or display colour space.
/// @param [in] xyGreenC             Green Chroma coordinates for output container or display color space.
/// @param [in] xyBlueC              Blue Chroma coordinates for output container or display color space.
/// @param [in] xyWhiteC             White Chroma coordinates for output container or display color space.
/// @param [in] scaleC               scale factor for PQ or scRGB adjustment
/// @param [in] softGap              Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.
/// @param [in] hdrMax               Maximum input value.
/// @param [in] exposure             Number of stops between 'hdrMax' and 18% mid-level on input.
/// @param [in] contrast             Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
/// @param [in] shoulderContrast     Shoulder shaping, 1.0 = no change (fast path).
/// @param [in] saturation           A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
/// @param [in] crosstalk            One channel must be 1.0, the rest can be <= 1.0 but not zero. Lenghtnes colours path to white by walking across gamut. Check documentation for usage
///
/// @ingroup FfxGPULpm
FFX_STATIC void FfxCalculateLpmConsts(
    // Path control.
    FfxBoolean shoulder,  // Use optional extra shoulderContrast tuning (set to false if shoulderContrast is 1.0).

    // Prefab start, "LPM_CONFIG_".
    FfxBoolean con,        // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
    FfxBoolean soft,       // Use soft gamut mapping.
    FfxBoolean con2,       // Use last RGB conversion matrix.
    FfxBoolean clip,       // Use clipping in last conversion matrix.
    FfxBoolean scaleOnly,  // Scale only for last conversion matrix (used for 709 HDR to scRGB).

    // Gamut control, "LPM_COLORS_".
    FfxFloat32x2 xyRedW,
    FfxFloat32x2 xyGreenW,
    FfxFloat32x2 xyBlueW,
    FfxFloat32x2 xyWhiteW,  // Chroma coordinates for working color space.
    FfxFloat32x2 xyRedO,
    FfxFloat32x2 xyGreenO,
    FfxFloat32x2 xyBlueO,
    FfxFloat32x2 xyWhiteO,  // For the output color space.
    FfxFloat32x2 xyRedC,
    FfxFloat32x2 xyGreenC,
    FfxFloat32x2 xyBlueC,
    FfxFloat32x2 xyWhiteC,
    FfxFloat32   scaleC,  // For the output container color space (if con2).

    // Prefab end.
    FfxFloat32 softGap,  // Range of 0 to a little over zero, controls how much feather region in out-of-gamut mapping, 0=clip.

    // Tonemapping control.
    FfxFloat32   hdrMax,            // Maximum input value.
    FfxFloat32   exposure,          // Number of stops between 'hdrMax' and 18% mid-level on input.
    FfxFloat32   contrast,          // Input range {0.0 (no extra contrast) to 1.0 (maximum contrast)}.
    FfxFloat32   shoulderContrast,  // Shoulder shaping, 1.0 = no change (fast path).
    FfxFloat32x3 saturation,        // A per channel adjustment, use <0 decrease, 0=no change, >0 increase.
    FfxFloat32x3 crosstalk)         // One channel must be 1.0, the rest can be <= 1.0 but not zero.
{
    // Contrast needs to be 1.0 based for no contrast.
    contrast += FfxFloat32(1.0);

    // Saturation is based on contrast.
    ffxOpAAddOneF3(saturation, saturation, contrast);

    // The 'softGap' must actually be above zero.
    softGap = ffxMax(softGap, FfxFloat32(1.0 / 1024.0));

    FfxFloat32 midIn  = hdrMax * FfxFloat32(0.18) * exp2(-exposure);
    FfxFloat32 midOut = FfxFloat32(0.18);

    FfxFloat32x2 toneScaleBias;
    FfxFloat32   cs  = contrast * shoulderContrast;
    FfxFloat32   z0  = -pow(midIn, contrast);
    FfxFloat32   z1  = pow(hdrMax, cs) * pow(midIn, contrast);
    FfxFloat32   z2  = pow(hdrMax, contrast) * pow(midIn, cs) * midOut;
    FfxFloat32   z3  = pow(hdrMax, cs) * midOut;
    FfxFloat32   z4  = pow(midIn, cs) * midOut;
    toneScaleBias[0] = -((z0 + (midOut * (z1 - z2)) * ffxReciprocal(z3 - z4)) * ffxReciprocal(z4));

    FfxFloat32 w0    = pow(hdrMax, cs) * pow(midIn, contrast);
    FfxFloat32 w1    = pow(hdrMax, contrast) * pow(midIn, cs) * midOut;
    FfxFloat32 w2    = pow(hdrMax, cs) * midOut;
    FfxFloat32 w3    = pow(midIn, cs) * midOut;
    toneScaleBias[1] = (w0 - w1) * ffxReciprocal(w2 - w3);

    FfxFloat32x3 lumaW;
    FfxFloat32x3 rgbToXyzXW;
    FfxFloat32x3 rgbToXyzYW;
    FfxFloat32x3 rgbToXyzZW;
    LpmColRgbToXyz(rgbToXyzXW, rgbToXyzYW, rgbToXyzZW, xyRedW, xyGreenW, xyBlueW, xyWhiteW);

    // Use the Y vector of the matrix for the associated luma coef.
    // For safety, make sure the vector sums to 1.0.
    ffxOpAMulOneF3(lumaW, rgbToXyzYW, ffxReciprocal(rgbToXyzYW[0] + rgbToXyzYW[1] + rgbToXyzYW[2]));

    // The 'lumaT' for crosstalk mapping is always based on the output color space, unless soft conversion is not used.
    FfxFloat32x3 lumaT;
    FfxFloat32x3 rgbToXyzXO;
    FfxFloat32x3 rgbToXyzYO;
    FfxFloat32x3 rgbToXyzZO;
    LpmColRgbToXyz(rgbToXyzXO, rgbToXyzYO, rgbToXyzZO, xyRedO, xyGreenO, xyBlueO, xyWhiteO);

    if (soft)
        ffxOpACpyF3(lumaT, rgbToXyzYO);
    else
        ffxOpACpyF3(lumaT, rgbToXyzYW);

    ffxOpAMulOneF3(lumaT, lumaT, ffxReciprocal(lumaT[0] + lumaT[1] + lumaT[2]));
    FfxFloat32x3 rcpLumaT;
    ffxOpARcpF3(rcpLumaT, lumaT);

    FfxFloat32x2 softGap2 = {0.0, 0.0};
    if (soft)
    {
        softGap2[0] = softGap;
        softGap2[1] = (FfxFloat32(1.0) - softGap) * ffxReciprocal(softGap * FfxFloat32(0.693147180559));
    }

    // First conversion is always working to output.
    FfxFloat32x3 conR = {0.0, 0.0, 0.0};
    FfxFloat32x3 conG = {0.0, 0.0, 0.0};
    FfxFloat32x3 conB = {0.0, 0.0, 0.0};

    if (con)
    {
        FfxFloat32x3 xyzToRgbRO;
        FfxFloat32x3 xyzToRgbGO;
        FfxFloat32x3 xyzToRgbBO;
        LpmMatInv3x3(xyzToRgbRO, xyzToRgbGO, xyzToRgbBO, rgbToXyzXO, rgbToXyzYO, rgbToXyzZO);
        LpmMatMul3x3(conR, conG, conB, xyzToRgbRO, xyzToRgbGO, xyzToRgbBO, rgbToXyzXW, rgbToXyzYW, rgbToXyzZW);
    }

    // The last conversion is always output to container.
    FfxFloat32x3 con2R = {0.0, 0.0, 0.0};
    FfxFloat32x3 con2G = {0.0, 0.0, 0.0};
    FfxFloat32x3 con2B = {0.0, 0.0, 0.0};

    if (con2)
    {
        FfxFloat32x3 rgbToXyzXC;
        FfxFloat32x3 rgbToXyzYC;
        FfxFloat32x3 rgbToXyzZC;
        LpmColRgbToXyz(rgbToXyzXC, rgbToXyzYC, rgbToXyzZC, xyRedC, xyGreenC, xyBlueC, xyWhiteC);

        FfxFloat32x3 xyzToRgbRC;
        FfxFloat32x3 xyzToRgbGC;
        FfxFloat32x3 xyzToRgbBC;
        LpmMatInv3x3(xyzToRgbRC, xyzToRgbGC, xyzToRgbBC, rgbToXyzXC, rgbToXyzYC, rgbToXyzZC);
        LpmMatMul3x3(con2R, con2G, con2B, xyzToRgbRC, xyzToRgbGC, xyzToRgbBC, rgbToXyzXO, rgbToXyzYO, rgbToXyzZO);
        ffxOpAMulOneF3(con2R, con2R, scaleC);
        ffxOpAMulOneF3(con2G, con2G, scaleC);
        ffxOpAMulOneF3(con2B, con2B, scaleC);
    }

    if (scaleOnly)
        con2R[0] = scaleC;

#if defined(FFX_GPU)
#if defined(LPM_DEBUG_FORCE_16BIT_PRECISION)
    // Debug force 16-bit precision for the 32-bit inputs, only works on the GPU.
    saturation       = FfxFloat32x3(FfxFloat16x3(saturation));
    contrast         = FfxFloat32(FfxFloat16(contrast));
    toneScaleBias    = FfxFloat32x2(FfxFloat16x2(toneScaleBias));
    lumaT            = FfxFloat32x3(FfxFloat16x3(lumaT));
    crosstalk        = FfxFloat32x3(FfxFloat16x3(crosstalk));
    rcpLumaT         = FfxFloat32x3(FfxFloat16x3(rcpLumaT));
    con2R            = FfxFloat32x3(FfxFloat16x3(con2R));
    con2G            = FfxFloat32x3(FfxFloat16x3(con2G));
    con2B            = FfxFloat32x3(FfxFloat16x3(con2B));
    shoulderContrast = FfxFloat32(FfxFloat16(shoulderContrast));
    lumaW            = FfxFloat32x3(FfxFloat16x3(lumaW));
    softGap2         = FfxFloat32x2(FfxFloat16x2(softGap2));
    conR             = FfxFloat32x3(FfxFloat16x3(conR));
    conG             = FfxFloat32x3(FfxFloat16x3(conG));
    conB             = FfxFloat32x3(FfxFloat16x3(conB));
#endif  // #if defined(LPM_DEBUG_FORCE_16BIT_PRECISION)
#endif  // #if defined(FFX_GPU)

    // Pack into control block.
    FfxUInt32x4 map0;
    map0[0] = ffxAsUInt32(saturation[0]);
    map0[1] = ffxAsUInt32(saturation[1]);
    map0[2] = ffxAsUInt32(saturation[2]);
    map0[3] = ffxAsUInt32(contrast);
    LpmSetupOut(0, map0);

    FfxUInt32x4 map1;
    map1[0] = ffxAsUInt32(toneScaleBias[0]);
    map1[1] = ffxAsUInt32(toneScaleBias[1]);
    map1[2] = ffxAsUInt32(lumaT[0]);
    map1[3] = ffxAsUInt32(lumaT[1]);
    LpmSetupOut(1, map1);

    FfxUInt32x4 map2;
    map2[0] = ffxAsUInt32(lumaT[2]);
    map2[1] = ffxAsUInt32(crosstalk[0]);
    map2[2] = ffxAsUInt32(crosstalk[1]);
    map2[3] = ffxAsUInt32(crosstalk[2]);
    LpmSetupOut(2, map2);

    FfxUInt32x4 map3;
    map3[0] = ffxAsUInt32(rcpLumaT[0]);
    map3[1] = ffxAsUInt32(rcpLumaT[1]);
    map3[2] = ffxAsUInt32(rcpLumaT[2]);
    map3[3] = ffxAsUInt32(con2R[0]);
    LpmSetupOut(3, map3);

    FfxUInt32x4 map4;
    map4[0] = ffxAsUInt32(con2R[1]);
    map4[1] = ffxAsUInt32(con2R[2]);
    map4[2] = ffxAsUInt32(con2G[0]);
    map4[3] = ffxAsUInt32(con2G[1]);
    LpmSetupOut(4, map4);

    FfxUInt32x4 map5;
    map5[0] = ffxAsUInt32(con2G[2]);
    map5[1] = ffxAsUInt32(con2B[0]);
    map5[2] = ffxAsUInt32(con2B[1]);
    map5[3] = ffxAsUInt32(con2B[2]);
    LpmSetupOut(5, map5);

    FfxUInt32x4 map6;
    map6[0] = ffxAsUInt32(shoulderContrast);
    map6[1] = ffxAsUInt32(lumaW[0]);
    map6[2] = ffxAsUInt32(lumaW[1]);
    map6[3] = ffxAsUInt32(lumaW[2]);
    LpmSetupOut(6, map6);

    FfxUInt32x4 map7;
    map7[0] = ffxAsUInt32(softGap2[0]);
    map7[1] = ffxAsUInt32(softGap2[1]);
    map7[2] = ffxAsUInt32(conR[0]);
    map7[3] = ffxAsUInt32(conR[1]);
    LpmSetupOut(7, map7);

    FfxUInt32x4 map8;
    map8[0] = ffxAsUInt32(conR[2]);
    map8[1] = ffxAsUInt32(conG[0]);
    map8[2] = ffxAsUInt32(conG[1]);
    map8[3] = ffxAsUInt32(conG[2]);
    LpmSetupOut(8, map8);

    FfxUInt32x4 map9;
    map9[0] = ffxAsUInt32(conB[0]);
    map9[1] = ffxAsUInt32(conB[1]);
    map9[2] = ffxAsUInt32(conB[2]);
    map9[3] = ffxAsUInt32(0);
    LpmSetupOut(9, map9);

    // Packed 16-bit part of control block.
    FfxUInt32x4  map16;
    FfxFloat32x2 map16x;
    FfxFloat32x2 map16y;
    FfxFloat32x2 map16z;
    FfxFloat32x2 map16w;
    map16x[0] = saturation[0];
    map16x[1] = saturation[1];
    map16y[0] = saturation[2];
    map16y[1] = contrast;
    map16z[0] = toneScaleBias[0];
    map16z[1] = toneScaleBias[1];
    map16w[0] = lumaT[0];
    map16w[1] = lumaT[1];
    map16[0]  = ffxPackHalf2x16(map16x);
    map16[1]  = ffxPackHalf2x16(map16y);
    map16[2]  = ffxPackHalf2x16(map16z);
    map16[3]  = ffxPackHalf2x16(map16w);
    LpmSetupOut(16, map16);

    FfxUInt32x4  map17;
    FfxFloat32x2 map17x;
    FfxFloat32x2 map17y;
    FfxFloat32x2 map17z;
    FfxFloat32x2 map17w;
    map17x[0] = lumaT[2];
    map17x[1] = crosstalk[0];
    map17y[0] = crosstalk[1];
    map17y[1] = crosstalk[2];
    map17z[0] = rcpLumaT[0];
    map17z[1] = rcpLumaT[1];
    map17w[0] = rcpLumaT[2];
    map17w[1] = con2R[0];
    map17[0]  = ffxPackHalf2x16(map17x);
    map17[1]  = ffxPackHalf2x16(map17y);
    map17[2]  = ffxPackHalf2x16(map17z);
    map17[3]  = ffxPackHalf2x16(map17w);
    LpmSetupOut(17, map17);

    FfxUInt32x4  map18;
    FfxFloat32x2 map18x;
    FfxFloat32x2 map18y;
    FfxFloat32x2 map18z;
    FfxFloat32x2 map18w;
    map18x[0] = con2R[1];
    map18x[1] = con2R[2];
    map18y[0] = con2G[0];
    map18y[1] = con2G[1];
    map18z[0] = con2G[2];
    map18z[1] = con2B[0];
    map18w[0] = con2B[1];
    map18w[1] = con2B[2];
    map18[0]  = ffxPackHalf2x16(map18x);
    map18[1]  = ffxPackHalf2x16(map18y);
    map18[2]  = ffxPackHalf2x16(map18z);
    map18[3]  = ffxPackHalf2x16(map18w);
    LpmSetupOut(18, map18);

    FfxUInt32x4  map19;
    FfxFloat32x2 map19x;
    FfxFloat32x2 map19y;
    FfxFloat32x2 map19z;
    FfxFloat32x2 map19w;
    map19x[0] = shoulderContrast;
    map19x[1] = lumaW[0];
    map19y[0] = lumaW[1];
    map19y[1] = lumaW[2];
    map19z[0] = softGap2[0];
    map19z[1] = softGap2[1];
    map19w[0] = conR[0];
    map19w[1] = conR[1];
    map19[0]  = ffxPackHalf2x16(map19x);
    map19[1]  = ffxPackHalf2x16(map19y);
    map19[2]  = ffxPackHalf2x16(map19z);
    map19[3]  = ffxPackHalf2x16(map19w);
    LpmSetupOut(19, map19);

    FfxUInt32x4  map20;
    FfxFloat32x2 map20x;
    FfxFloat32x2 map20y;
    FfxFloat32x2 map20z;
    FfxFloat32x2 map20w;
    map20x[0] = conR[2];
    map20x[1] = conG[0];
    map20y[0] = conG[1];
    map20y[1] = conG[2];
    map20z[0] = conB[0];
    map20z[1] = conB[1];
    map20w[0] = conB[2];
    map20w[1] = 0.0;
    map20[0]  = ffxPackHalf2x16(map20x);
    map20[1]  = ffxPackHalf2x16(map20y);
    map20[2]  = ffxPackHalf2x16(map20z);
    map20[3]  = ffxPackHalf2x16(map20w);
    LpmSetupOut(20, map20);
}

//==============================================================================================================================
//                                                 HDR10 RANGE LIMITING SCALAR
//------------------------------------------------------------------------------------------------------------------------------
// As of 2019, HDR10 supporting TVs typically have PQ tonal curves with near clipping long before getting to the peak 10K nits.
// Unfortunately this clipping point changes per TV (requires some amount of user calibration).
// Some examples,
//  https://youtu.be/M7OsbpU4oCQ?t=875
//  https://youtu.be/8mlTElC2z2A?t=1159
//  https://youtu.be/B5V5hCVXBAI?t=975
// For this reason it can be useful to manually limit peak HDR10 output to some point before the clipping point.
// The following functions are useful to compute the scaling factor 'hdr10S' to use with LpmSetup() to manually limit peak.
//==============================================================================================================================
// Compute 'hdr10S' for raw HDR10 output, pass in peak nits (typically somewhere around 1000.0 to 2000.0).
FFX_STATIC FfxFloat32 LpmHdr10RawScalar(FfxFloat32 peakNits)
{
    return peakNits * (FfxFloat32(1.0) / FfxFloat32(10000.0));
}

// Compute 'hdr10S' for scRGB based HDR10 output, pass in peak nits (typically somewhere around 1000.0 to 2000.0).
FFX_STATIC FfxFloat32 LpmHdr10ScrgbScalar(FfxFloat32 peakNits)
{
    return peakNits * (FfxFloat32(1.0) / FfxFloat32(10000.0)) * (FfxFloat32(10000.0) / FfxFloat32(80.0));
}

//==============================================================================================================================
//                                                   FREESYNC2 SCRGB SCALAR
//------------------------------------------------------------------------------------------------------------------------------
// The more expensive scRGB mode for FreeSync2 requires a complex scale factor based on display properties.
//==============================================================================================================================
// This computes the 'fs2S' factor used in LpmSetup().
// TODO: Is this correct????????????????????????????????????????????????????????????????????????????????????????????????????????
FFX_STATIC FfxFloat32 LpmFs2ScrgbScalar(FfxFloat32 minLuma, FfxFloat32 maxLuma)
{
    // Queried display properties.
    return ((maxLuma - minLuma) + minLuma) * (FfxFloat32(1.0) / FfxFloat32(80.0));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                   CONFIGURATION PREFABS
//------------------------------------------------------------------------------------------------------------------------------
// Use these to simplify some of the input(s) to the LpmSetup() and LpmFilter() functions.
// The 'LPM_CONFIG_<destination>_<source>' defines are used for the path control.
// The 'LPM_COLORS_<destination>_<source>' defines are used for the gamut control.
// This contains expected common configurations, anything else will need to be made by the user.
//------------------------------------------------------------------------------------------------------------------------------
//                WORKING COLOR SPACE
//                ===================
// 2020 ......... Rec.2020
// 709 .......... Rec.709
// P3 ........... DCI-P3 with D65 white-point
// --------------
//                OUTPUT COLOR SPACE
//                ==================
// FS2RAW ....... Faster 32-bit/pixel FreeSync2 raw gamma 2.2 output (native display primaries)
// FS2RAWPQ ..... Faster 32-bit/pixel FreeSync2 raw PQ output (native display primaries for gamut which are then converted to Rec.2020 primaries for transport)
// FS2SCRGB ..... Slower 64-bit/pixel FreeSync2 via the scRGB option (Rec.709 primaries with possible negative color)
// HDR10RAW ..... Faster 32-bit/pixel HDR10 raw (10:10:10:2 PQ output with Rec.2020 primaries)
// HDR10SCRGB ... Slower 64-bit/pixel scRGB (linear FP16, Rec.709 primaries with possible negative color)
// 709 .......... Rec.709, sRGB, Gamma 2.2, or traditional displays with Rec.709-like primaries
//------------------------------------------------------------------------------------------------------------------------------
// FREESYNC2 VARIABLES
// ===================
// fs2R ..... Queried xy coordinates for display red
// fs2G ..... Queried xy coordinates for display green
// fs2B ..... Queried xy coordinates for display blue
// fs2W ..... Queried xy coordinates for display white point
// fs2S ..... Computed by LpmFs2ScrgbScalar()
//------------------------------------------------------------------------------------------------------------------------------
// HDR10 VARIABLES
// ===============
// hdr10S ... Use LpmHdr10<Raw|Scrgb>Scalar() to compute this value
//==============================================================================================================================
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_709 FFX_FALSE, FFX_FALSE, FFX_TRUE, FFX_TRUE, FFX_FALSE
#define LPM_COLORS_FS2RAW_709 \
    lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2R, fs2G, fs2B, fs2W, FfxFloat32(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAWPQ_709 FFX_FALSE, FFX_FALSE, FFX_TRUE, FFX_TRUE, FFX_FALSE
#define LPM_COLORS_FS2RAWPQ_709 \
    lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// FreeSync2 min-spec is larger than sRGB, so using 709 primaries all the way through as an optimization.
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_709 FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_TRUE
#define LPM_COLORS_FS2SCRGB_709 \
    lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_709 FFX_FALSE, FFX_FALSE, FFX_TRUE, FFX_TRUE, FFX_FALSE
#define LPM_COLORS_HDR10RAW_709 \
    lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_709 FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_TRUE
#define LPM_COLORS_HDR10SCRGB_709 \
    lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_709 FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_709_709 \
    lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, FfxFloat32(1.0)
//==============================================================================================================================
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_P3 FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_FS2RAW_P3 lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, fs2R, fs2G, fs2B, fs2W, fs2R, fs2G, fs2B, fs2W, FfxFloat32(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAWPQ_P3 FFX_TRUE, FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_FS2RAWPQ_P3 lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// FreeSync2 gamut can be smaller than P3.
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_P3 FFX_TRUE, FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_FS2SCRGB_P3 lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_P3 FFX_FALSE, FFX_FALSE, FFX_TRUE, FFX_TRUE, FFX_FALSE
#define LPM_COLORS_HDR10RAW_P3 \
    lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_P3 FFX_FALSE, FFX_FALSE, FFX_TRUE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_HDR10SCRGB_P3 \
    lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_P3 FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_709_P3 \
    lpmColP3R, lpmColP3G, lpmColP3B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, FfxFloat32(1.0)
//==============================================================================================================================
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAW_2020 FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_FS2RAW_2020 lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, fs2R, fs2G, fs2B, fs2W, fs2R, fs2G, fs2B, fs2W, FfxFloat32(1.0)
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2RAWPQ_2020 FFX_TRUE, FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_FS2RAWPQ_2020 lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_FS2SCRGB_2020 FFX_TRUE, FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_FS2SCRGB_2020 lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, fs2R, fs2G, fs2B, fs2W, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, fs2S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10RAW_2020 FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_FALSE, FFX_TRUE
#define LPM_COLORS_HDR10RAW_2020 \
    lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_HDR10SCRGB_2020 FFX_FALSE, FFX_FALSE, FFX_TRUE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_HDR10SCRGB_2020 \
    lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, hdr10S
//------------------------------------------------------------------------------------------------------------------------------
// CON     SOFT    CON2    CLIP    SCALEONLY
#define LPM_CONFIG_709_2020 FFX_TRUE, FFX_TRUE, FFX_FALSE, FFX_FALSE, FFX_FALSE
#define LPM_COLORS_709_2020                                                                                                                         \
    lpmCol2020R, lpmCol2020G, lpmCol2020B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, lpmCol709R, lpmCol709G, lpmCol709B, lpmColD65, \
        FfxFloat32(1.0)

#if defined(FFX_GPU)
// Visualize difference between two values, by bits of precision.
// This is useful when doing approximation to reference comparisons.
FfxBoolean LpmD(FfxFloat32 a, FfxFloat32 b)
{
    return abs(a - b) < 1.0;
}

FfxFloat32 LpmC(FfxFloat32 a, FfxFloat32 b)
{
    FfxFloat32 c = 1.0;  // 6-bits or less (the color)
    if (LpmD(a * 127.0, b * 127.0))
        c = 0.875;  // 7-bits
    if (LpmD(a * 255.0, b * 255.0))
        c = 0.5;  // 8-bits
    if (LpmD(a * 512.0, b * 512.0))
        c = 0.125;  // 9-bits
    if (LpmD(a * 1024.0, b * 1024.0))
        c = 0.0;  // 10-bits or better (black)
    return c;
}

FfxFloat32x3 LpmViewDiff(FfxFloat32x3 a, FfxFloat32x3 b)
{
    return FfxFloat32x3(LpmC(a.r, b.r), LpmC(a.g, b.g), LpmC(a.b, b.b));
}

//==============================================================================================================================
//                                                           MAPPER
//------------------------------------------------------------------------------------------------------------------------------
// Do not call this directly, instead call the LpmFilter*() functions.
// This gets reconfigured based on inputs for all the various usage cases.
// Some of this has been explicitly ordered to increase precision.
//------------------------------------------------------------------------------------------------------------------------------
// IDEAS
// =====
//  - Use ffxMed3() for soft falloff and for [A] color conversions.
//  - Retry FP16 PQ conversion with different input range.
//  - Possibly skip some work if entire wave is in gamut.
//==============================================================================================================================
// Use LpmFilter() instead of this.
void LpmMap(inout FfxFloat32 colorR,
            inout FfxFloat32 colorG,
            inout FfxFloat32 colorB,            // Input and output color.
            FfxFloat32x3     lumaW,             // Luma coef for RGB working space.
            FfxFloat32x3     lumaT,             // Luma coef for crosstalk mapping (can be working or output color-space depending on usage case).
            FfxFloat32x3     rcpLumaT,          // 1/lumaT.
            FfxFloat32x3     saturation,        // Saturation powers.
            FfxFloat32       contrast,          // Contrast power.
            FfxBoolean       shoulder,          // Using shoulder tuning (should be a compile-time immediate).
            FfxFloat32       shoulderContrast,  // Shoulder power.
            FfxFloat32x2     toneScaleBias,     // Other tonemapping parameters.
            FfxFloat32x3     crosstalk,         // Crosstalk scaling for over-exposure color shaping.
            FfxBoolean       con,               // Use first RGB conversion matrix (should be a compile-time immediate), if 'soft' then 'con' must be true also.
            FfxFloat32x3     conR,
            FfxFloat32x3     conG,
            FfxFloat32x3     conB,       // RGB conversion matrix (working to output space conversion).
            FfxBoolean       soft,       // Use soft gamut mapping (should be a compile-time immediate).
            FfxFloat32x2     softGap,    // {x,(1-x)/(x*0.693147180559)}, where 'x' is gamut mapping soft fall-off amount.
            FfxBoolean       con2,       // Use last RGB conversion matrix (should be a compile-time immediate).
            FfxBoolean       clip,       // Use clipping on last conversion matrix.
            FfxBoolean       scaleOnly,  // Do scaling only (special case for 709 HDR to scRGB).
            FfxFloat32x3     con2R,
            FfxFloat32x3     con2G,
            FfxFloat32x3     con2B)
{
    // Secondary RGB conversion matrix.
    // Grab original RGB ratio (RCP, 3x MUL, MAX3).
    FfxFloat32 rcpMax = ffxReciprocal(ffxMax3(colorR, colorG, colorB));
    FfxFloat32 ratioR = colorR * rcpMax;
    FfxFloat32 ratioG = colorG * rcpMax;
    FfxFloat32 ratioB = colorB * rcpMax;

    // Apply saturation, ratio must be max 1.0 for this to work right (3x EXP2, 3x LOG2, 3x MUL).
    ratioR = pow(ratioR, FfxFloat32(saturation.r));
    ratioG = pow(ratioG, FfxFloat32(saturation.g));
    ratioB = pow(ratioB, FfxFloat32(saturation.b));

    // Tonemap luma, note this uses the original color, so saturation is luma preserving.
    // If not using 'con' this uses the output space luma directly to avoid needing extra constants.
    // Note 'soft' should be a compile-time immediate (so no branch) (3x MAD).
    FfxFloat32 luma;
    if (soft)
        luma = colorG * FfxFloat32(lumaW.g) + (colorR * FfxFloat32(lumaW.r) + (colorB * FfxFloat32(lumaW.b)));
    else
        luma = colorG * FfxFloat32(lumaT.g) + (colorR * FfxFloat32(lumaT.r) + (colorB * FfxFloat32(lumaT.b)));
    luma                    = pow(luma, FfxFloat32(contrast));                                                       // (EXP2, LOG2, MUL).
    FfxFloat32 lumaShoulder = shoulder ? pow(luma, FfxFloat32(shoulderContrast)) : luma;                             // Optional (EXP2, LOG2, MUL).
    luma                    = luma * ffxReciprocal(lumaShoulder * FfxFloat32(toneScaleBias.x) + FfxFloat32(toneScaleBias.y));  // (MAD, MUL, RCP).

    // If running soft clipping (this should be a compile-time immediate so branch will not exist).
    if (soft)
    {
        // The 'con' should be a compile-time immediate so branch will not exist.
        // Use of 'con' is implied if soft-falloff is enabled, but using the check here to make finding bugs easy.
        if (con)
        {
            // Converting ratio instead of color. Change of primaries (9x MAD).
            colorR = ratioR;
            colorG = ratioG;
            colorB = ratioB;
            ratioR = colorR * FfxFloat32(conR.r) + (colorG * FfxFloat32(conR.g) + (colorB * FfxFloat32(conR.b)));
            ratioG = colorG * FfxFloat32(conG.g) + (colorR * FfxFloat32(conG.r) + (colorB * FfxFloat32(conG.b)));
            ratioB = colorB * FfxFloat32(conB.b) + (colorG * FfxFloat32(conB.g) + (colorR * FfxFloat32(conB.r)));

            // Convert ratio to max 1 again (RCP, 3x MUL, MAX3).
            rcpMax = ffxReciprocal(ffxMax3(ratioR, ratioG, ratioB));
            ratioR *= rcpMax;
            ratioG *= rcpMax;
            ratioB *= rcpMax;
        }

        // Absolute gamut mapping converted to soft falloff (maintains max 1 property).
        //  g = gap {0 to g} used for {-inf to 0} input range
        //          {g to 1} used for {0 to 1} input range
        //  x >= 0 := y = x * (1-g) + g
        //  x < 0  := g * 2^(x*h)
        //  Where h=(1-g)/(g*log(2)) --- where log() is the natural log
        // The {g,h} above is passed in as softGap.
        // Soft falloff (3x MIN, 3x MAX, 9x MAD, 3x EXP2).
        ratioR = ffxMin(max(FfxFloat32(softGap.x), ffxSaturate(ratioR * FfxFloat32(-softGap.x) + ratioR)),
                        ffxSaturate(FfxFloat32(softGap.x) * exp2(ratioR * FfxFloat32(softGap.y))));
        ratioG = ffxMin(max(FfxFloat32(softGap.x), ffxSaturate(ratioG * FfxFloat32(-softGap.x) + ratioG)),
                        ffxSaturate(FfxFloat32(softGap.x) * exp2(ratioG * FfxFloat32(softGap.y))));
        ratioB = ffxMin(max(FfxFloat32(softGap.x), ffxSaturate(ratioB * FfxFloat32(-softGap.x) + ratioB)),
                        ffxSaturate(FfxFloat32(softGap.x) * exp2(ratioB * FfxFloat32(softGap.y))));
    }

    // Compute ratio scaler required to hit target luma (4x MAD, 1 RCP).
    FfxFloat32 lumaRatio = ratioR * FfxFloat32(lumaT.r) + ratioG * FfxFloat32(lumaT.g) + ratioB * FfxFloat32(lumaT.b);

    // This is limited to not clip.
    FfxFloat32 ratioScale = ffxSaturate(luma * ffxReciprocal(lumaRatio));

    // Assume in gamut, compute output color (3x MAD).
    colorR = ffxSaturate(ratioR * ratioScale);
    colorG = ffxSaturate(ratioG * ratioScale);
    colorB = ffxSaturate(ratioB * ratioScale);

    // Capability per channel to increase value (3x MAD).
    // This factors in crosstalk factor to avoid multiplies later.
    //  '(1.0-ratio)*crosstalk' optimized to '-crosstalk*ratio+crosstalk'
    FfxFloat32 capR = FfxFloat32(-crosstalk.r) * colorR + FfxFloat32(crosstalk.r);
    FfxFloat32 capG = FfxFloat32(-crosstalk.g) * colorG + FfxFloat32(crosstalk.g);
    FfxFloat32 capB = FfxFloat32(-crosstalk.b) * colorB + FfxFloat32(crosstalk.b);

    // Compute amount of luma needed to add to non-clipped channels to make up for clipping (3x MAD).
    FfxFloat32 lumaAdd = ffxSaturate((-colorB) * FfxFloat32(lumaT.b) + ((-colorR) * FfxFloat32(lumaT.r) + ((-colorG) * FfxFloat32(lumaT.g) + luma)));

    // Amount to increase keeping over-exposure ratios constant and possibly exceeding clipping point (4x MAD, 1 RCP).
    FfxFloat32 t = lumaAdd * ffxReciprocal(capG * FfxFloat32(lumaT.g) + (capR * FfxFloat32(lumaT.r) + (capB * FfxFloat32(lumaT.b))));

    // Add amounts to base color but clip (3x MAD).
    colorR = ffxSaturate(t * capR + colorR);
    colorG = ffxSaturate(t * capG + colorG);
    colorB = ffxSaturate(t * capB + colorB);

    // Compute amount of luma needed to add to non-clipped channel to make up for clipping (3x MAD).
    lumaAdd = ffxSaturate((-colorB) * FfxFloat32(lumaT.b) + ((-colorR) * FfxFloat32(lumaT.r) + ((-colorG) * FfxFloat32(lumaT.g) + luma)));

    // Add to last channel (3x MAD).
    colorR = ffxSaturate(lumaAdd * FfxFloat32(rcpLumaT.r) + colorR);
    colorG = ffxSaturate(lumaAdd * FfxFloat32(rcpLumaT.g) + colorG);
    colorB = ffxSaturate(lumaAdd * FfxFloat32(rcpLumaT.b) + colorB);

    // The 'con2' should be a compile-time immediate so branch will not exist.
    // Last optional place to convert from smaller to larger gamut (or do clipped conversion).
    // For the non-soft-falloff case, doing this after all other mapping saves intermediate re-scaling ratio to max 1.0.
    if (con2)
    {
        // Change of primaries (9x MAD).
        ratioR = colorR;
        ratioG = colorG;
        ratioB = colorB;

        if (clip)
        {
            colorR = ffxSaturate(ratioR * FfxFloat32(con2R.r) + (ratioG * FfxFloat32(con2R.g) + (ratioB * FfxFloat32(con2R.b))));
            colorG = ffxSaturate(ratioG * FfxFloat32(con2G.g) + (ratioR * FfxFloat32(con2G.r) + (ratioB * FfxFloat32(con2G.b))));
            colorB = ffxSaturate(ratioB * FfxFloat32(con2B.b) + (ratioG * FfxFloat32(con2B.g) + (ratioR * FfxFloat32(con2B.r))));
        }
        else
        {
            colorR = ratioR * FfxFloat32(con2R.r) + (ratioG * FfxFloat32(con2R.g) + (ratioB * FfxFloat32(con2R.b)));
            colorG = ratioG * FfxFloat32(con2G.g) + (ratioR * FfxFloat32(con2G.r) + (ratioB * FfxFloat32(con2G.b)));
            colorB = ratioB * FfxFloat32(con2B.b) + (ratioG * FfxFloat32(con2B.g) + (ratioR * FfxFloat32(con2B.r)));
        }
    }

    if (scaleOnly)
    {
        colorR *= FfxFloat32(con2R.r);
        colorG *= FfxFloat32(con2R.r);
        colorB *= FfxFloat32(con2R.r);
    }
}

#if (FFX_HALF == 1)
// Packed FP16 version, see non-packed version above for all comments.
// Use LpmFilterH() instead of this.
void LpmMapH(inout FfxFloat16x2 colorR,
             inout FfxFloat16x2 colorG,
             inout FfxFloat16x2 colorB,
             FfxFloat16x3       lumaW,
             FfxFloat16x3       lumaT,
             FfxFloat16x3       rcpLumaT,
             FfxFloat16x3       saturation,
             FfxFloat16         contrast,
             FfxBoolean         shoulder,
             FfxFloat16         shoulderContrast,
             FfxFloat16x2       toneScaleBias,
             FfxFloat16x3       crosstalk,
             FfxBoolean         con,
             FfxFloat16x3       conR,
             FfxFloat16x3       conG,
             FfxFloat16x3       conB,
             FfxBoolean         soft,
             FfxFloat16x2       softGap,
             FfxBoolean         con2,
             FfxBoolean         clip,
             FfxBoolean         scaleOnly,
             FfxFloat16x3       con2R,
             FfxFloat16x3       con2G,
             FfxFloat16x3       con2B)
{
    FfxFloat16x2 rcpMax = ffxReciprocalHalf(ffxMax3Half(colorR, colorG, colorB));
    FfxFloat16x2 ratioR = colorR * rcpMax;
    FfxFloat16x2 ratioG = colorG * rcpMax;
    FfxFloat16x2 ratioB = colorB * rcpMax;
    ratioR              = pow(ratioR, FFX_BROADCAST_FLOAT16X2(saturation.r));
    ratioG              = pow(ratioG, FFX_BROADCAST_FLOAT16X2(saturation.g));
    ratioB              = pow(ratioB, FFX_BROADCAST_FLOAT16X2(saturation.b));

    FfxFloat16x2 luma;
    if (soft)
        luma = colorG * FFX_BROADCAST_FLOAT16X2(lumaW.g) + (colorR * FFX_BROADCAST_FLOAT16X2(lumaW.r) + (colorB * FFX_BROADCAST_FLOAT16X2(lumaW.b)));
    else
        luma = colorG * FFX_BROADCAST_FLOAT16X2(lumaT.g) + (colorR * FFX_BROADCAST_FLOAT16X2(lumaT.r) + (colorB * FFX_BROADCAST_FLOAT16X2(lumaT.b)));
    luma                      = pow(luma, FFX_BROADCAST_FLOAT16X2(contrast));
    FfxFloat16x2 lumaShoulder = shoulder ? pow(luma, FFX_BROADCAST_FLOAT16X2(shoulderContrast)) : luma;
    luma                      = luma * ffxReciprocalHalf(lumaShoulder * FFX_BROADCAST_FLOAT16X2(toneScaleBias.x) + FFX_BROADCAST_FLOAT16X2(toneScaleBias.y));

    if (soft)
    {
        if (con)
        {
            colorR = ratioR;
            colorG = ratioG;
            colorB = ratioB;
            ratioR = colorR * FFX_BROADCAST_FLOAT16X2(conR.r) + (colorG * FFX_BROADCAST_FLOAT16X2(conR.g) + (colorB * FFX_BROADCAST_FLOAT16X2(conR.b)));
            ratioG = colorG * FFX_BROADCAST_FLOAT16X2(conG.g) + (colorR * FFX_BROADCAST_FLOAT16X2(conG.r) + (colorB * FFX_BROADCAST_FLOAT16X2(conG.b)));
            ratioB = colorB * FFX_BROADCAST_FLOAT16X2(conB.b) + (colorG * FFX_BROADCAST_FLOAT16X2(conB.g) + (colorR * FFX_BROADCAST_FLOAT16X2(conB.r)));
            rcpMax = ffxReciprocalHalf(ffxMax3Half(ratioR, ratioG, ratioB));
            ratioR *= rcpMax;
            ratioG *= rcpMax;
            ratioB *= rcpMax;
        }

        ratioR = min(max(FFX_BROADCAST_FLOAT16X2(softGap.x), ffxSaturate(ratioR * FFX_BROADCAST_FLOAT16X2(-softGap.x) + ratioR)),
                     ffxSaturate(FFX_BROADCAST_FLOAT16X2(softGap.x) * exp2(ratioR * FFX_BROADCAST_FLOAT16X2(softGap.y))));
        ratioG = min(max(FFX_BROADCAST_FLOAT16X2(softGap.x), ffxSaturate(ratioG * FFX_BROADCAST_FLOAT16X2(-softGap.x) + ratioG)),
                     ffxSaturate(FFX_BROADCAST_FLOAT16X2(softGap.x) * exp2(ratioG * FFX_BROADCAST_FLOAT16X2(softGap.y))));
        ratioB = min(max(FFX_BROADCAST_FLOAT16X2(softGap.x), ffxSaturate(ratioB * FFX_BROADCAST_FLOAT16X2(-softGap.x) + ratioB)),
                     ffxSaturate(FFX_BROADCAST_FLOAT16X2(softGap.x) * exp2(ratioB * FFX_BROADCAST_FLOAT16X2(softGap.y))));
    }

    FfxFloat16x2 lumaRatio  = ratioR * FFX_BROADCAST_FLOAT16X2(lumaT.r) + ratioG * FFX_BROADCAST_FLOAT16X2(lumaT.g) + ratioB * FFX_BROADCAST_FLOAT16X2(lumaT.b);
    FfxFloat16x2 ratioScale = ffxSaturate(luma * ffxReciprocalHalf(lumaRatio));
    colorR                  = ffxSaturate(ratioR * ratioScale);
    colorG                  = ffxSaturate(ratioG * ratioScale);
    colorB                  = ffxSaturate(ratioB * ratioScale);
    FfxFloat16x2 capR       = FFX_BROADCAST_FLOAT16X2(-crosstalk.r) * colorR + FFX_BROADCAST_FLOAT16X2(crosstalk.r);
    FfxFloat16x2 capG       = FFX_BROADCAST_FLOAT16X2(-crosstalk.g) * colorG + FFX_BROADCAST_FLOAT16X2(crosstalk.g);
    FfxFloat16x2 capB       = FFX_BROADCAST_FLOAT16X2(-crosstalk.b) * colorB + FFX_BROADCAST_FLOAT16X2(crosstalk.b);
    FfxFloat16x2 lumaAdd    = ffxSaturate((-colorB) * FFX_BROADCAST_FLOAT16X2(lumaT.b) +
                                           ((-colorR) * FFX_BROADCAST_FLOAT16X2(lumaT.r) + ((-colorG) * FFX_BROADCAST_FLOAT16X2(lumaT.g) + luma)));
    FfxFloat16x2 t          = lumaAdd * ffxReciprocalHalf(capG * FFX_BROADCAST_FLOAT16X2(lumaT.g) +
                                                 (capR * FFX_BROADCAST_FLOAT16X2(lumaT.r) + (capB * FFX_BROADCAST_FLOAT16X2(lumaT.b))));
    colorR                  = ffxSaturate(t * capR + colorR);
    colorG                  = ffxSaturate(t * capG + colorG);
    colorB                  = ffxSaturate(t * capB + colorB);
    lumaAdd                 = ffxSaturate((-colorB) * FFX_BROADCAST_FLOAT16X2(lumaT.b) +
                              ((-colorR) * FFX_BROADCAST_FLOAT16X2(lumaT.r) + ((-colorG) * FFX_BROADCAST_FLOAT16X2(lumaT.g) + luma)));
    colorR                  = ffxSaturate(lumaAdd * FFX_BROADCAST_FLOAT16X2(rcpLumaT.r) + colorR);
    colorG                  = ffxSaturate(lumaAdd * FFX_BROADCAST_FLOAT16X2(rcpLumaT.g) + colorG);
    colorB                  = ffxSaturate(lumaAdd * FFX_BROADCAST_FLOAT16X2(rcpLumaT.b) + colorB);

    if (con2)
    {
        ratioR = colorR;
        ratioG = colorG;
        ratioB = colorB;
        if (clip)
        {
            colorR = ffxSaturate(ratioR * FFX_BROADCAST_FLOAT16X2(con2R.r) +
                                     (ratioG * FFX_BROADCAST_FLOAT16X2(con2R.g) + (ratioB * FFX_BROADCAST_FLOAT16X2(con2R.b))));
            colorG = ffxSaturate(ratioG * FFX_BROADCAST_FLOAT16X2(con2G.g) +
                                     (ratioR * FFX_BROADCAST_FLOAT16X2(con2G.r) + (ratioB * FFX_BROADCAST_FLOAT16X2(con2G.b))));
            colorB = ffxSaturate(ratioB * FFX_BROADCAST_FLOAT16X2(con2B.b) +
                                     (ratioG * FFX_BROADCAST_FLOAT16X2(con2B.g) + (ratioR * FFX_BROADCAST_FLOAT16X2(con2B.r))));
        }
        else
        {
            colorR = ratioR * FFX_BROADCAST_FLOAT16X2(con2R.r) + (ratioG * FFX_BROADCAST_FLOAT16X2(con2R.g) + (ratioB * FFX_BROADCAST_FLOAT16X2(con2R.b)));
            colorG = ratioG * FFX_BROADCAST_FLOAT16X2(con2G.g) + (ratioR * FFX_BROADCAST_FLOAT16X2(con2G.r) + (ratioB * FFX_BROADCAST_FLOAT16X2(con2G.b)));
            colorB = ratioB * FFX_BROADCAST_FLOAT16X2(con2B.b) + (ratioG * FFX_BROADCAST_FLOAT16X2(con2B.g) + (ratioR * FFX_BROADCAST_FLOAT16X2(con2B.r)));
        }
    }

    if (scaleOnly)
    {
        colorR *= FFX_BROADCAST_FLOAT16X2(con2R.r);
        colorG *= FFX_BROADCAST_FLOAT16X2(con2R.r);
        colorB *= FFX_BROADCAST_FLOAT16X2(con2R.r);
    }
}
#endif  // #if (FFX_HALF == 1)

/// Filter call to tone and gamut map input pixel colour
///
/// @param [inout] colorR           Input of red value of pixel to be tone and gamut mapped and also where result will be stored.
/// @param [inout] colorG           Input of green value of pixel to be tone and gamut mapped and also where result will be stored.
/// @param [inout] colorB           Input of blue value of pixel to be tone and gamut mapped and also where result will be stored.
/// @param [in] shoulder            Boolean to enable shoulder tuning
/// @param [in] con                 Same as described in setup call
/// @param [in] soft                Same as described in setup call
/// @param [in] con2                Same as described in setup call
/// @param [in] clip                Same as described in setup call
/// @param [in] scaleOnly           Same as described in setup call
///
/// @ingroup FfxGPULpm
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//_____________________________________________________________/\_______________________________________________________________
//==============================================================================================================================
//                                                           FILTER
//------------------------------------------------------------------------------------------------------------------------------
// Requires user define: FfxUInt32x4 LpmFilterCtl(FfxUInt32 index){...} to load control block values.
// Entry point for per-pixel color tone+gamut mapping.
// Input is linear color {0 to hdrMax} ranged.
// Output is linear color {0 to 1} ranged, except for scRGB where outputs can end up negative and larger than one.
//==============================================================================================================================
// 32-bit entry point.
void LpmFilter(
    // Input and output color.
    inout FfxFloat32 colorR,
    inout FfxFloat32 colorG,
    inout FfxFloat32 colorB,
    // Path control should all be compile-time immediates.
    FfxBoolean shoulder,  // Using shoulder tuning.
    // Prefab "LPM_CONFIG_" start, use the same as used for LpmSetup().
    FfxBoolean con,        // Use first RGB conversion matrix, if 'soft' then 'con' must be true also.
    FfxBoolean soft,       // Use soft gamut mapping.
    FfxBoolean con2,       // Use last RGB conversion matrix.
    FfxBoolean clip,       // Use clipping in last conversion matrix.
    FfxBoolean scaleOnly)  // Scale only for last conversion matrix (used for 709 HDR to scRGB).
{
    // Grab control block, what is unused gets dead-code removal.
    FfxUInt32x4 map0 = LpmFilterCtl(0);
    FfxUInt32x4 map1 = LpmFilterCtl(1);
    FfxUInt32x4 map2 = LpmFilterCtl(2);
    FfxUInt32x4 map3 = LpmFilterCtl(3);
    FfxUInt32x4 map4 = LpmFilterCtl(4);
    FfxUInt32x4 map5 = LpmFilterCtl(5);
    FfxUInt32x4 map6 = LpmFilterCtl(6);
    FfxUInt32x4 map7 = LpmFilterCtl(7);
    FfxUInt32x4 map8 = LpmFilterCtl(8);
    FfxUInt32x4 map9 = LpmFilterCtl(9);
    FfxUInt32x4 mapA = LpmFilterCtl(10);
    FfxUInt32x4 mapB = LpmFilterCtl(11);
    FfxUInt32x4 mapC = LpmFilterCtl(12);
    FfxUInt32x4 mapD = LpmFilterCtl(13);
    FfxUInt32x4 mapE = LpmFilterCtl(14);
    FfxUInt32x4 mapF = LpmFilterCtl(15);
    FfxUInt32x4 mapG = LpmFilterCtl(16);
    FfxUInt32x4 mapH = LpmFilterCtl(17);
    FfxUInt32x4 mapI = LpmFilterCtl(18);
    FfxUInt32x4 mapJ = LpmFilterCtl(19);
    FfxUInt32x4 mapK = LpmFilterCtl(20);
    FfxUInt32x4 mapL = LpmFilterCtl(21);
    FfxUInt32x4 mapM = LpmFilterCtl(22);
    FfxUInt32x4 mapN = LpmFilterCtl(23);

    LpmMap(colorR,
           colorG,
           colorB,
           FfxFloat32x3(ffxAsFloat(map6).g, ffxAsFloat(map6).b, ffxAsFloat(map6).a),  // lumaW
           FfxFloat32x3(ffxAsFloat(map1).b, ffxAsFloat(map1).a, ffxAsFloat(map2).r),  // lumaT
           FfxFloat32x3(ffxAsFloat(map3).r, ffxAsFloat(map3).g, ffxAsFloat(map3).b),  // rcpLumaT
           FfxFloat32x3(ffxAsFloat(map0).r, ffxAsFloat(map0).g, ffxAsFloat(map0).b),  // saturation
           ffxAsFloat(map0).a,                                                        // contrast
           shoulder,
           ffxAsFloat(map6).r,                                                        // shoulderContrast
           FfxFloat32x2(ffxAsFloat(map1).r, ffxAsFloat(map1).g),                      // toneScaleBias
           FfxFloat32x3(ffxAsFloat(map2).g, ffxAsFloat(map2).b, ffxAsFloat(map2).a),  // crosstalk
           con,
           FfxFloat32x3(ffxAsFloat(map7).b, ffxAsFloat(map7).a, ffxAsFloat(map8).r),  // conR
           FfxFloat32x3(ffxAsFloat(map8).g, ffxAsFloat(map8).b, ffxAsFloat(map8).a),  // conG
           FfxFloat32x3(ffxAsFloat(map9).r, ffxAsFloat(map9).g, ffxAsFloat(map9).b),  // conB
           soft,
           FfxFloat32x2(ffxAsFloat(map7).r, ffxAsFloat(map7).g),  // softGap
           con2,
           clip,
           scaleOnly,
           FfxFloat32x3(ffxAsFloat(map3).a, ffxAsFloat(map4).r, ffxAsFloat(map4).g),   // con2R
           FfxFloat32x3(ffxAsFloat(map4).b, ffxAsFloat(map4).a, ffxAsFloat(map5).r),   // con2G
           FfxFloat32x3(ffxAsFloat(map5).g, ffxAsFloat(map5).b, ffxAsFloat(map5).a));  // con2B
}

#if (FFX_HALF == 1)
// Packed 16-bit entry point (maps 2 colors at the same time).
void LpmFilterH(inout FfxFloat16x2 colorR,
                inout FfxFloat16x2 colorG,
                inout FfxFloat16x2 colorB,
                FfxBoolean         shoulder,
                FfxBoolean         con,
                FfxBoolean         soft,
                FfxBoolean         con2,
                FfxBoolean         clip,
                FfxBoolean         scaleOnly)
{
    // Grab control block, what is unused gets dead-code removal.
    FfxUInt32x4 map0 = LpmFilterCtl(0);
    FfxUInt32x4 map1 = LpmFilterCtl(1);
    FfxUInt32x4 map2 = LpmFilterCtl(2);
    FfxUInt32x4 map3 = LpmFilterCtl(3);
    FfxUInt32x4 map4 = LpmFilterCtl(4);
    FfxUInt32x4 map5 = LpmFilterCtl(5);
    FfxUInt32x4 map6 = LpmFilterCtl(6);
    FfxUInt32x4 map7 = LpmFilterCtl(7);
    FfxUInt32x4 map8 = LpmFilterCtl(8);
    FfxUInt32x4 map9 = LpmFilterCtl(9);
    FfxUInt32x4 mapA = LpmFilterCtl(10);
    FfxUInt32x4 mapB = LpmFilterCtl(11);
    FfxUInt32x4 mapC = LpmFilterCtl(12);
    FfxUInt32x4 mapD = LpmFilterCtl(13);
    FfxUInt32x4 mapE = LpmFilterCtl(14);
    FfxUInt32x4 mapF = LpmFilterCtl(15);
    FfxUInt32x4 mapG = LpmFilterCtl(16);
    FfxUInt32x4 mapH = LpmFilterCtl(17);
    FfxUInt32x4 mapI = LpmFilterCtl(18);
    FfxUInt32x4 mapJ = LpmFilterCtl(19);
    FfxUInt32x4 mapK = LpmFilterCtl(20);
    FfxUInt32x4 mapL = LpmFilterCtl(21);
    FfxUInt32x4 mapM = LpmFilterCtl(22);
    FfxUInt32x4 mapN = LpmFilterCtl(23);

    // Pre-limit inputs to provide enough head-room for computation in FP16.
    // TODO: Document this better!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    colorR = min(colorR, FFX_BROADCAST_FLOAT16X2(4096.0));
    colorG = min(colorG, FFX_BROADCAST_FLOAT16X2(4096.0));
    colorB = min(colorB, FFX_BROADCAST_FLOAT16X2(4096.0));

    // Apply filter.
    LpmMapH(colorR,
            colorG,
            colorB,
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapJ.r).y, FFX_UINT32_TO_FLOAT16X2(mapJ.g).x, FFX_UINT32_TO_FLOAT16X2(mapJ.g).y),  // lumaW
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapG.a).x, FFX_UINT32_TO_FLOAT16X2(mapG.a).y, FFX_UINT32_TO_FLOAT16X2(mapH.r).x),  // lumaT
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapH.b).x, FFX_UINT32_TO_FLOAT16X2(mapH.b).y, FFX_UINT32_TO_FLOAT16X2(mapH.a).x),  // rcpLumaT
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapG.r).x, FFX_UINT32_TO_FLOAT16X2(mapG.r).y, FFX_UINT32_TO_FLOAT16X2(mapG.g).x),  // saturation
            FFX_UINT32_TO_FLOAT16X2(mapG.g).y,                                                                                      // contrast
            shoulder,
            FFX_UINT32_TO_FLOAT16X2(mapJ.r).x,                                                                                      // shoulderContrast
            FFX_UINT32_TO_FLOAT16X2(mapG.b),                                                                                        // toneScaleBias
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapH.r).y, FFX_UINT32_TO_FLOAT16X2(mapH.g).x, FFX_UINT32_TO_FLOAT16X2(mapH.g).y),  // crosstalk
            con,
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapJ.a).x, FFX_UINT32_TO_FLOAT16X2(mapJ.a).y, FFX_UINT32_TO_FLOAT16X2(mapK.r).x),  // conR
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapK.r).y, FFX_UINT32_TO_FLOAT16X2(mapK.g).x, FFX_UINT32_TO_FLOAT16X2(mapK.g).y),  // conG
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapK.b).x, FFX_UINT32_TO_FLOAT16X2(mapK.b).y, FFX_UINT32_TO_FLOAT16X2(mapK.a).x),  // conB
            soft,
            FFX_UINT32_TO_FLOAT16X2(mapJ.b),  // softGap
            con2,
            clip,
            scaleOnly,
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapH.a).y, FFX_UINT32_TO_FLOAT16X2(mapI.r).x, FFX_UINT32_TO_FLOAT16X2(mapI.r).y),   // con2R
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapI.g).x, FFX_UINT32_TO_FLOAT16X2(mapI.g).y, FFX_UINT32_TO_FLOAT16X2(mapI.b).x),   // con2G
            FfxFloat16x3(FFX_UINT32_TO_FLOAT16X2(mapI.b).y, FFX_UINT32_TO_FLOAT16X2(mapI.a).x, FFX_UINT32_TO_FLOAT16X2(mapI.a).y));  // con2B
}
#endif  // #if (FFX_HALF == 1)
#endif  // #if defined(FFX_GPU)
