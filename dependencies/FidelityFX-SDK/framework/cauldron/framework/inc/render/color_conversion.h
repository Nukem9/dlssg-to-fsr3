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

#pragma once

#include "misc/math.h"

namespace cauldron
{
    /// An enumeration for color space to use for attached monitor display
    ///
    /// @ingroup CauldronRender
    enum ColorSpace
    {
        ColorSpace_REC709,  ///< Display using Rec. 709 (also known as Rec.709, BT.709, and ITU 709).
        ColorSpace_P3,      ///< Display using DCI-P3 sRGB color space.
        ColorSpace_REC2020, ///< Display using Rec. 2020 (also known as Rec. 2020 or BT.2020).
        ColorSpace_Display  ///< Current display color space id.
    };

    /// An enumeration representing the red, green, blue and white points for above colour spaces' gamut triangle
    ///
    /// @ingroup CauldronRender
    enum ColorPrimaries
    {
        ColorPrimaries_WHITE,   ///< White
        ColorPrimaries_RED,     ///< Red
        ColorPrimaries_GREEN,   ///< Green
        ColorPrimaries_BLUE     ///< Blue
    };

    /// An enumeration for storing xy triangle gamut points for above red, green, blue and white color primairies
    ///
    /// @ingroup CauldronRender
    enum ColorPrimariesCoordinates
    {
        ColorPrimariesCoordinates_X,    ///< The x coordinate
        ColorPrimariesCoordinates_Y     ///< The y coordinate
    };

    extern float ColorSpacePrimaries[4][4][2];

    /// Fills the ColorSpace_Display entry with the provided primaries
    ///
    /// @ingroup CauldronRender
    void FillDisplaySpecificPrimaries(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb);

    Mat4 CalculateRGBToXYZMatrix(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb, bool scaleLumaFlag);
    Mat4 CalculateXYZToRGBMatrix(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb, bool scaleLumaFlag);

    /// Calculates conversion matrix to to transform RGB values from one gamut to another.
    /// gamutIn is colorspace to convert from and gamutOut is target colourspace to convert to
    /// @ingroup CauldronRender
    void SetupGamutMapperMatrices(ColorSpace gamutIn, ColorSpace gamutOut, Mat4* inputToOutputRecMatrix);
}  // namespace cauldron
