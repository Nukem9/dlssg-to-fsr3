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

#ifndef FFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE
#define FFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE 0 // Reference
#endif

FfxFloat32x4 WrapHistory(FfxInt32x2 iPxSample)
{
    return LoadHistory(iPxSample);
}

DeclareCustomFetchBicubicSamples(FetchHistorySamples, WrapHistory)
DeclareCustomTextureSample(HistorySample, FFX_FSR3UPSCALER_GET_LANCZOS_SAMPLER1D(FFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE), FetchHistorySamples)

FfxFloat32x2 GetMotionVector(FfxInt32x2 iPxHrPos, FfxFloat32x2 fHrUv)
{
#if FFX_FSR3UPSCALER_OPTION_LOW_RESOLUTION_MOTION_VECTORS
    const FfxFloat32x2 fDilatedMotionVector = LoadDilatedMotionVector(FFX_MIN16_I2(fHrUv * RenderSize()));
#else
    const FfxFloat32x2 fDilatedMotionVector = LoadInputMotionVector(iPxHrPos);
#endif

    return fDilatedMotionVector;
}

void ComputeReprojectedUVs(FFX_PARAMETER_INOUT AccumulationPassCommonParams params)
{
    params.fReprojectedHrUv = params.fHrUv + params.fMotionVector;

    params.bIsExistingSample = IsUvInside(params.fReprojectedHrUv);
}

void ReprojectHistoryColor(const AccumulationPassCommonParams params, FFX_PARAMETER_INOUT AccumulationPassData data)
{
    const FfxFloat32x4 fReprojectedHistory = HistorySample(params.fReprojectedHrUv, PreviousFrameUpscaleSize());

    data.fHistoryColor = fReprojectedHistory.rgb;
    data.fHistoryColor *= DeltaPreExposure();
    data.fHistoryColor *= Exposure();

    data.fHistoryColor = RGBToYCoCg(data.fHistoryColor);

    data.fLock = fReprojectedHistory.w;
}
