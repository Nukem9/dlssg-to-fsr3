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

#include "render/color_conversion.h"

namespace cauldron
{
    float ColorSpacePrimaries[4][4][2] = {
        //Rec709
        {
            0.3127f,0.3290f, // White point
            0.64f,0.33f, // Red point
            0.30f,0.60f, // Green point
            0.15f,0.06f, // Blue point
        },
        //P3
        {
            0.3127f,0.3290f, // White point
            0.680f,0.320f, // Red point
            0.265f,0.690f, // Green point
            0.150f,0.060f, // Blue point
        },
        //Rec2020
        {
            0.3127f,0.3290f, // White point
            0.708f,0.292f, // Red point
            0.170f,0.797f, // Green point
            0.131f,0.046f, // Blue point
        },
        //Display Specific zeroed out now Please fill them in once you query them and want to use them for matrix calculations
        {
            0.0f,0.0f, // White point
            0.0f,0.0f, // Red point
            0.0f,0.0f, // Green point
            0.0f,0.0f // Blue point
        }
    };

    Mat4 CalculateRGBToXYZMatrix(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb, bool scaleLumaFlag)
    {
        float Xw = xw / yw;
        float Yw = 1;
        float Zw = (1 - xw - yw) / yw;

        float Xr = xr / yr;
        float Yr = 1;
        float Zr = (1 - xr - yr) / yr;

        float Xg = xg / yg;
        float Yg = 1;
        float Zg = (1 - xg - yg) / yg;

        float Xb = xb / yb;
        float Yb = 1;
        float Zb = (1 - xb - yb) / yb;

        Mat4 XRGB = Mat4(
            Vec4(Xr, Xg, Xb, 0),
            Vec4(Yr, Yg, Yb, 0),
            Vec4(Zr, Zg, Zb, 0),
            Vec4(0, 0, 0, 1));
        Mat4 XRGBInverse = InverseMatrix(XRGB);

        Vec4 referenceWhite = Vec4(Xw, Yw, Zw, 0);
        Vec4 SRGB = TransposeMatrix(XRGBInverse) * referenceWhite;

        Mat4 RegularResult = Mat4(
            Vec4(SRGB.getX() * Xr, SRGB.getY() * Xg, SRGB.getZ() * Xb, 0),
            Vec4(SRGB.getX() * Yr, SRGB.getY() * Yg, SRGB.getZ() * Yb, 0),
            Vec4(SRGB.getX() * Zr, SRGB.getY() * Zg, SRGB.getZ() * Zb, 0),
            Vec4(0, 0, 0, 1));

        if (!scaleLumaFlag)
            return RegularResult;

        Mat4 Scale = Mat4::scale(Vec3(100, 100, 100));
        Mat4 Result = RegularResult * Scale;
        return Result;
    }

    Mat4 CalculateXYZToRGBMatrix(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb, bool scaleLumaFlag)
    {
        auto RGBToXYZ = CalculateRGBToXYZMatrix(xw, yw, xr, yr, xg, yg, xb, yb, scaleLumaFlag);
        return InverseMatrix(RGBToXYZ);
    }

    void FillDisplaySpecificPrimaries(float xw, float yw, float xr, float yr, float xg, float yg, float xb, float yb)
    {
        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_WHITE][ColorPrimariesCoordinates_X] = xw;
        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_WHITE][ColorPrimariesCoordinates_Y] = yw;

        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_RED][ColorPrimariesCoordinates_X] = xr;
        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_RED][ColorPrimariesCoordinates_Y] = yr;

        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_GREEN][ColorPrimariesCoordinates_X] = xg;
        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_GREEN][ColorPrimariesCoordinates_Y] = yg;

        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_BLUE][ColorPrimariesCoordinates_X] = xb;
        ColorSpacePrimaries[ColorSpace_Display][ColorPrimaries_BLUE][ColorPrimariesCoordinates_Y] = yb;
    }

    void SetupGamutMapperMatrices(ColorSpace gamutIn, ColorSpace gamutOut, Mat4* inputToOutputRecMatrix)
    {
        Mat4 intputGamut_To_XYZ = CalculateRGBToXYZMatrix(
            ColorSpacePrimaries[gamutIn][ColorPrimaries_WHITE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_WHITE][ColorPrimariesCoordinates_Y],
            ColorSpacePrimaries[gamutIn][ColorPrimaries_RED][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_RED][ColorPrimariesCoordinates_Y],
            ColorSpacePrimaries[gamutIn][ColorPrimaries_GREEN][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_GREEN][ColorPrimariesCoordinates_Y],
            ColorSpacePrimaries[gamutIn][ColorPrimaries_BLUE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutIn][ColorPrimaries_BLUE][ColorPrimariesCoordinates_Y],
            false);

        Mat4 XYZ_To_OutputGamut = CalculateXYZToRGBMatrix(
            ColorSpacePrimaries[gamutOut][ColorPrimaries_WHITE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_WHITE][ColorPrimariesCoordinates_Y],
            ColorSpacePrimaries[gamutOut][ColorPrimaries_RED][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_RED][ColorPrimariesCoordinates_Y],
            ColorSpacePrimaries[gamutOut][ColorPrimaries_GREEN][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_GREEN][ColorPrimariesCoordinates_Y],
            ColorSpacePrimaries[gamutOut][ColorPrimaries_BLUE][ColorPrimariesCoordinates_X], ColorSpacePrimaries[gamutOut][ColorPrimaries_BLUE][ColorPrimariesCoordinates_Y],
            false);

        Mat4 intputGamut_To_OutputGamut = intputGamut_To_XYZ * XYZ_To_OutputGamut;
        *inputToOutputRecMatrix = TransposeMatrix(intputGamut_To_OutputGamut);
    }
}  // namespace cauldron
