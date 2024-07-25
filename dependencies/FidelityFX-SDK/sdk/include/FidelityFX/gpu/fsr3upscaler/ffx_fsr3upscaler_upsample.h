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

void Deringing(RectificationBox clippingBox, FFX_PARAMETER_INOUT FfxFloat32x3 fColor)
{
    fColor = clamp(fColor, clippingBox.aabbMin, clippingBox.aabbMax);
}

#ifndef FFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE
#define FFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE 2 // Approximate
#endif

FfxFloat32 GetUpsampleLanczosWeight(FfxFloat32x2 fSrcSampleOffset, FfxFloat32 fKernelWeight)
{
    FfxFloat32x2 fSrcSampleOffsetBiased = fSrcSampleOffset * fKernelWeight.xx;
#if FFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE == 0 // LANCZOS_TYPE_REFERENCE
    FfxFloat32 fSampleWeight = Lanczos2(length(fSrcSampleOffsetBiased));
#elif FFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE == 1 // LANCZOS_TYPE_LUT
    FfxFloat32 fSampleWeight = Lanczos2_UseLUT(length(fSrcSampleOffsetBiased));
#elif FFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE == 2 // LANCZOS_TYPE_APPROXIMATE
    FfxFloat32 fSampleWeight = Lanczos2ApproxSq(dot(fSrcSampleOffsetBiased, fSrcSampleOffsetBiased));
#else
#error "Invalid Lanczos type"
#endif
    return fSampleWeight;
}

FfxFloat32 ComputeMaxKernelWeight(const AccumulationPassCommonParams params, FFX_PARAMETER_INOUT AccumulationPassData data) {

    const FfxFloat32 fKernelSizeBias = 1.0f + (1.0f / FfxFloat32x2(DownscaleFactor()) - 1.0f).x;

    return ffxMin(FfxFloat32(1.99f), fKernelSizeBias);
}

FfxFloat32x3 LoadPreparedColor(FfxInt32x2 iSamplePos)
{
    const FfxFloat32x3 fRgb             = ffxMax(FfxFloat32x3(0, 0, 0), LoadInputColor(iSamplePos)) * Exposure();
    const FfxFloat32x3 fPreparedYCoCg   = RGBToYCoCg(fRgb);

    return fPreparedYCoCg;
}

void ComputeUpsampledColorAndWeight(const AccumulationPassCommonParams params, FFX_PARAMETER_INOUT AccumulationPassData data)
{
    // We compute a sliced lanczos filter with 2 lobes (other slices are accumulated temporaly)
    const FfxFloat32x2 fDstOutputPos        = FfxFloat32x2(params.iPxHrPos) + FFX_BROADCAST_FLOAT32X2(0.5f);
    const FfxFloat32x2 fSrcOutputPos        = fDstOutputPos * DownscaleFactor();
    const FfxInt32x2   iSrcInputPos         = FfxInt32x2(floor(fSrcOutputPos));
    const FfxFloat32x2 fSrcUnjitteredPos    = (FfxFloat32x2(iSrcInputPos) + FfxFloat32x2(0.5f, 0.5f)) - Jitter(); // This is the un-jittered position of the sample at offset 0,0
    const FfxFloat32x2 fBaseSampleOffset    = FfxFloat32x2(fSrcUnjitteredPos - fSrcOutputPos);

    FfxInt32x2 offsetTL;
    offsetTL.x = (fSrcUnjitteredPos.x > fSrcOutputPos.x) ? FfxInt32(-2) : FfxInt32(-1);
    offsetTL.y = (fSrcUnjitteredPos.y > fSrcOutputPos.y) ? FfxInt32(-2) : FfxInt32(-1);

    //Load samples
    // If fSrcUnjitteredPos.y > fSrcOutputPos.y, indicates offsetTL.y = -2, sample offset Y will be [-2, 1], clipbox will be rows [1, 3].
    // Flip row# for sampling offset in this case, so first 0~2 rows in the sampled array can always be used for computing the clipbox.
    // This reduces branch or cmove on sampled colors, but moving this overhead to sample position / weight calculation time which apply to less values.
    const FfxBoolean bFlipRow = fSrcUnjitteredPos.y > fSrcOutputPos.y;
    const FfxBoolean bFlipCol = fSrcUnjitteredPos.x > fSrcOutputPos.x;
    const FfxFloat32x2 fOffsetTL = FfxFloat32x2(offsetTL);

    const FfxBoolean bIsInitialSample = (params.fAccumulation == 0.0f);

    FfxFloat32x3 fSamples[9];
    FfxInt32 iSampleIndex = 0;

    FFX_UNROLL
    for (FfxInt32 row = 0; row < 3; row++) {
        FFX_UNROLL
        for (FfxInt32 col = 0; col < 3; col++) {
            const FfxInt32x2 iSampleColRow = FfxInt32x2(bFlipCol ? (3 - col) : col, bFlipRow ? (3 - row) : row);
            const FfxInt32x2 iSrcSamplePos = FfxInt32x2(iSrcInputPos) + offsetTL + iSampleColRow;
            const FfxInt32x2 iSampleCoord = ClampLoad(iSrcSamplePos, FfxInt32x2(0, 0), FfxInt32x2(RenderSize()));

            fSamples[iSampleIndex] = LoadPreparedColor(iSampleCoord);

            ++iSampleIndex;
        }
    }

#if FFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT
    if (bIsInitialSample)
    {
        for (iSampleIndex = 0; iSampleIndex < 9; ++iSampleIndex)
        {
            //YCoCg -> RGB -> Tonemap -> YCoCg (Use RGB tonemapper to avoid color desaturation)
            fSamples[iSampleIndex] = RGBToYCoCg(Tonemap(YCoCgToRGB(fSamples[iSampleIndex])));
        }
    }
#endif

    // Identify how much of each upsampled color to be used for this frame
    const FfxFloat32 fKernelBiasMax          = ComputeMaxKernelWeight(params, data);
    const FfxFloat32 fKernelBiasMin          = ffxMax(1.0f, ((1.0f + fKernelBiasMax) * 0.3f));

    const FfxFloat32 fKernelBiasWeight =
        ffxMin(1.0f - params.fDisocclusion * 0.5f,
        ffxMin(1.0f - params.fShadingChange,
        ffxSaturate(data.fHistoryWeight * 5.0f)
        ));

    const FfxFloat32 fKernelBias             = ffxLerp(fKernelBiasMin, fKernelBiasMax, fKernelBiasWeight);
    

    iSampleIndex = 0;

    FFX_UNROLL
    for (FfxInt32 row = 0; row < 3; row++)
    {
        FFX_UNROLL
        for (FfxInt32 col = 0; col < 3; col++)
        {
            const FfxInt32x2   sampleColRow     = FfxInt32x2(bFlipCol ? (3 - col) : col, bFlipRow ? (3 - row) : row);
            const FfxFloat32x2 fOffset          = fOffsetTL + FfxFloat32x2(sampleColRow);
            const FfxFloat32x2 fSrcSampleOffset = fBaseSampleOffset + fOffset;

            const FfxInt32x2 iSrcSamplePos   = FfxInt32x2(iSrcInputPos) + FfxInt32x2(offsetTL) + sampleColRow;
            const FfxFloat32 fOnScreenFactor = FfxFloat32(IsOnScreen(FfxInt32x2(iSrcSamplePos), FfxInt32x2(RenderSize())));

            if (!bIsInitialSample)
            {
                const FfxFloat32 fSampleWeight = fOnScreenFactor * FfxFloat32(GetUpsampleLanczosWeight(fSrcSampleOffset, fKernelBias));

                data.fUpsampledColor += fSamples[iSampleIndex] * fSampleWeight;
                data.fUpsampledWeight += fSampleWeight;
            }

            // Update rectification box
            {
                const FfxFloat32 fRectificationCurveBias = -2.3f;
                const FfxFloat32 fSrcSampleOffsetSq = dot(fSrcSampleOffset, fSrcSampleOffset);
                const FfxFloat32 fBoxSampleWeight   = exp(fRectificationCurveBias * fSrcSampleOffsetSq) * fOnScreenFactor;

                const FfxBoolean bInitialSample = (row == 0) && (col == 0);
                RectificationBoxAddSample(bInitialSample, data.clippingBox, fSamples[iSampleIndex], fBoxSampleWeight);
            }
            ++iSampleIndex;
        }
    }

    RectificationBoxComputeVarianceBoxData(data.clippingBox);

    data.fUpsampledWeight *= FfxFloat32(data.fUpsampledWeight > FSR3UPSCALER_EPSILON);

    if (data.fUpsampledWeight > FSR3UPSCALER_EPSILON) {
        // Normalize for deringing (we need to compare colors)
        data.fUpsampledColor = data.fUpsampledColor / data.fUpsampledWeight;
        data.fUpsampledWeight *= fAverageLanczosWeightPerFrame;

        Deringing(data.clippingBox, data.fUpsampledColor);
    }

    // Initial samples using tonemapped upsampling
    if (bIsInitialSample) {
#if FFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT
        data.fUpsampledColor  = RGBToYCoCg(InverseTonemap(YCoCgToRGB(data.clippingBox.boxCenter)));
#else
        data.fUpsampledColor  = data.clippingBox.boxCenter;
#endif
        data.fUpsampledWeight = 1.0f;
        data.fHistoryWeight   = 0.0f;
    }
}
