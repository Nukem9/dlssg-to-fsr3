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

#include "uicommon.h"
#include "fullscreen.hlsl"
#include "upscaler.h"
#include "transferFunction.h"

//--------------------------------------------------------------------------------------
// Input structures
//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer hdrparams : register(b1)
{
    HDRCBData HDRCB;
}

cbuffer magnifierparams : register(b1)
{
    MagnifierCBData MagnifierCB;
};

cbuffer vertexBuffer : register(b0)
{
    float4x4 ProjectionMatrix;
};

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
RWTexture2D<float4> ColorTargetDst : register(u0);
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);


//--------------------------------------------------------------------------------------
// Main functions
//--------------------------------------------------------------------------------------
[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void MagnifierCS(uint3 dtID : SV_DispatchThreadID)
{
    const float2 uv = GetUV(dtID.xy, 1.f / UpscalerInfo.FullScreenScaleRatio.xy);
    float4       outColor = texture0[dtID.xy];

    const float2 imageSize = float2(MagnifierCB.ImageWidth, MagnifierCB.ImageHeight);
    const float  aspectRatio = imageSize.x / imageSize.y;
    const float  aspectRatioInv = imageSize.y / imageSize.x;
    const float2 magnifierAreaCenter = float2(MagnifierCB.MousePosX, MagnifierCB.MousePosY);
    const float2 magnifierOnScreenLocation = float2(MagnifierCB.MousePosX + MagnifierCB.MagnifierOffsetX, MagnifierCB.MousePosY + MagnifierCB.MagnifierOffsetY);

    const float2 currPixel = uv * imageSize;
    const float2 uvMagnifiedArea = float2(magnifierAreaCenter / imageSize);
    const float2 uvMagnifierOnScreen = float2(magnifierOnScreenLocation / imageSize);

    const float magnifiedAreaR = MagnifierCB.MagnifierScreenRadius / MagnifierCB.MagnificationAmount;
    const float magnifierOnScreenR = MagnifierCB.MagnifierScreenRadius;

    float2 uvRArea = uv - uvMagnifiedArea;
    if (imageSize.x > imageSize.y)
        uvRArea.x *= aspectRatio;
    else
        uvRArea.y *= aspectRatioInv;

    float2 uvROnScreen = uv - uvMagnifierOnScreen;
    if (imageSize.x > imageSize.y)
        uvROnScreen.x *= aspectRatio;
    else
        uvROnScreen.y *= aspectRatioInv;

    const float BORDER_THICKNESS_OFFSET       = 0.0025f;
    const float BORDER_THICKNESS_OFFSET_QUARTER  = BORDER_THICKNESS_OFFSET * 0.25f;
    const float BORDER_THICKNESS_MAX_EXTENT = BORDER_THICKNESS_OFFSET * 2.0f;
    const float RADIUS_BORDER_WITH_LINE_FUDGE = BORDER_THICKNESS_OFFSET * 1.1f;

    const float BigMagnifierCircle   = length(uvROnScreen) - magnifierOnScreenR;
    const float SmallMagnifierCircle = length(uvRArea) - magnifiedAreaR;

    const bool bAreWeInMagnifierOnScreenRegion = length(uvROnScreen) < (magnifierOnScreenR + BORDER_THICKNESS_MAX_EXTENT);

    float3 borderColor    = float3(MagnifierCB.BorderColorRGB[0], MagnifierCB.BorderColorRGB[1], MagnifierCB.BorderColorRGB[2]);
    float3 magnifiedColor = float3(0, 0, 0);

    // Apply border to small circle 
    float3 blendedOrigToBorderColor = lerp(outColor.rgb, borderColor, smoothstep(0, BORDER_THICKNESS_OFFSET, SmallMagnifierCircle));
    float3 blendedOrigToOuterColor =
        lerp(blendedOrigToBorderColor, outColor.rgb, smoothstep(BORDER_THICKNESS_OFFSET, BORDER_THICKNESS_MAX_EXTENT, SmallMagnifierCircle));

    // Magnify pixels from smaller circle into larger circle
    if (bAreWeInMagnifierOnScreenRegion)
    {
        float2 sampleUVOffsetFromCenter = uv - uvMagnifierOnScreen;
        sampleUVOffsetFromCenter /= MagnifierCB.MagnificationAmount;
        const float2 magnifierUV = uvMagnifiedArea + sampleUVOffsetFromCenter;

        int2 screenCoordinates = GetScreenCoordinates(magnifierUV, UpscalerInfo.FullScreenScaleRatio.xy);
        
        // Indexing out of bounds will return zero
        magnifiedColor = texture0[screenCoordinates].rgb;
    }

    // Blend border to large circle
    float3 blendedMagnifierToBorderColor = lerp(magnifiedColor, borderColor, smoothstep(0, BORDER_THICKNESS_OFFSET, BigMagnifierCircle));
    float3 blendedMagnifierColorFinal =
        lerp(blendedMagnifierToBorderColor, blendedOrigToOuterColor.rgb, smoothstep(BORDER_THICKNESS_OFFSET, BORDER_THICKNESS_MAX_EXTENT, BigMagnifierCircle));
    
    // Draw lines tangent to both circles
    float3 blendedMagnifierCirclesWithLines = blendedMagnifierColorFinal; // initialize to running blended color

    float2 currentToCenter = uv - uvMagnifiedArea;
    float2 centerToCenterLine = uvMagnifierOnScreen - uvMagnifiedArea;

    // coorect for aspect ration
    if (imageSize.x > imageSize.y)
    {
        currentToCenter.x *= aspectRatio;
        centerToCenterLine.x *= aspectRatio;
    }
    else
    {
        currentToCenter.y *= aspectRatio;
        centerToCenterLine.y *= aspectRatio;
    }

    float centerToCenterLength = length(centerToCenterLine);
    centerToCenterLine /= centerToCenterLength;

    float distanceOnCenterline = dot(currentToCenter, centerToCenterLine);
    float2 projOnCenterline = distanceOnCenterline * centerToCenterLine;
    float2 perpToCenterline = currentToCenter - projOnCenterline;

    // Cosine of angle between center line and tangent radius line
    float magnifiedAreaREnlarged     = magnifiedAreaR + RADIUS_BORDER_WITH_LINE_FUDGE;
    float magnifierOnScreenREnlarged = magnifierOnScreenR + RADIUS_BORDER_WITH_LINE_FUDGE;

    float alphaFactor                   = (magnifierOnScreenREnlarged - magnifiedAreaREnlarged) / centerToCenterLength;
    float centerToCenterPlusAlphaLength = centerToCenterLength - alphaFactor * (magnifierOnScreenREnlarged - magnifiedAreaREnlarged);
    distanceOnCenterline += alphaFactor * magnifiedAreaREnlarged;

    // Blend lines in with circles
    float t = distanceOnCenterline / centerToCenterPlusAlphaLength;

    // The tangent line is also tangent to a circle of a radius that is a linear interpolation of the two circles and the alpha factor
    float radiusAtPoint = ((1 - t) * magnifiedAreaREnlarged + t * magnifierOnScreenREnlarged) * sqrt(1 - alphaFactor * alphaFactor);
    float lineDist = abs(length(perpToCenterline) - radiusAtPoint);

    blendedMagnifierCirclesWithLines =
        lerp(borderColor, blendedMagnifierColorFinal, smoothstep(BORDER_THICKNESS_OFFSET_QUARTER, BORDER_THICKNESS_OFFSET, lineDist));
    blendedMagnifierCirclesWithLines =
        lerp(blendedMagnifierCirclesWithLines, blendedMagnifierColorFinal, smoothstep(BORDER_THICKNESS_OFFSET_QUARTER, 0, distanceOnCenterline));
    blendedMagnifierCirclesWithLines =
        lerp(blendedMagnifierCirclesWithLines, blendedMagnifierColorFinal,
             smoothstep(centerToCenterPlusAlphaLength, centerToCenterPlusAlphaLength + BORDER_THICKNESS_OFFSET, distanceOnCenterline));
    
    ColorTargetDst[dtID.xy].rgb = blendedMagnifierCirclesWithLines;
}

//--------------------------------------------------------------------------------------
// For ImGui Render
PS_INPUT uiVS(VS_INPUT input)
{
    PS_INPUT output;
    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, NEAR_DEPTH, 1.f));
    output.col = input.col;
    output.uv = input.uv;
    return output;
}

float4 uiPS(PS_INPUT input) : SV_Target
{
    float4 out_col = input.col * texture0.Sample(sampler0, input.uv);

    switch (HDRCB.MonitorDisplayMode)
    {
        case DisplayMode::DISPLAYMODE_LDR:
            out_col.xyz = ApplyGamma(out_col.xyz);
            break;

        case DisplayMode::DISPLAYMODE_HDR10_SCRGB:
        case DisplayMode::DISPLAYMODE_FSHDR_SCRGB:
            out_col.xyz = ApplyscRGBScale(out_col.xyz, 0.0f, HDRCB.DisplayMaxLuminance / 80.0f);
            break;

        case DisplayMode::DISPLAYMODE_HDR10_2084:
        case DisplayMode::DISPLAYMODE_FSHDR_2084:
            // Convert to rec2020 colour space
            float3 col = mul(HDRCB.ContentToMonitorRecMatrix, out_col).xyz;

            // DisplayMaxLuminancePerNits = max nits set / 80
            // ST2084 convention when they define PQ formula is 1 = 10,000 nits.
            // MS/driver convention is 1 = 80 nits, so when we apply the formula, we need to scale by 80/10000 to get us on the same page as ST2084 spec
            // So, scaling should be color *= (nits_when_color_is_1 / 10,000).
            // Since we want our color which is normalized so that 1.0 = max nits in above cbuffer to be mapped on the display, we need to do the below calc
            // Lets look at whole calculation with example: max nits = 1000, color = 1
            // 1 * ((1000 / 80) * (80 / 10000)) = 1 / 10 = 0.1
            // For simplcity we are getting rid of conversion to per 80 nit division factor and directly dividing max luminance set by 10000 nits
            col *= (HDRCB.DisplayMaxLuminance / 10000.0f);
            col = ApplyPQ(col);
            out_col = float4(col, out_col.a);
            break;
    }

    return out_col;
}
