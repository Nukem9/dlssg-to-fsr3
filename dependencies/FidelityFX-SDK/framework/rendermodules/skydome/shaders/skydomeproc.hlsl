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

// MIT License
// Copyright (c) 2011 Simon Wallner
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

// Based on "A Practical Analytic Model for Daylight"
// aka The Preetham Model, the de facto standard analytic skydome model
// https://courses.cs.duke.edu/cps124/spring08/assign/07_papers/p91-preetham.pdf

#include "fullscreen.hlsl"
#include "skydomecommon.h"
#include "upscaler.h"

// Output
#ifdef ENVIRONMENT_CUBE
RWTexture2DArray<float4> EnvironmentCube : register(u0);
#endif

#ifdef IRRADIANCE_CUBE
TextureCube              EnvironmentCube : register(t0);
RWTexture2DArray<float4> IrradianceCube : register(u0);
SamplerState             SamLinearWrap : register(s0);
#endif

#ifdef PREFILTERED_CUBE
TextureCube              EnvironmentCube : register(t0);
StructuredBuffer<float4> SampleDirections : register(t1);
RWTexture2DArray<float4> PrefilteredCube : register(u0);
SamplerState             SamLinearWrap : register(s0);
#endif

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------

//const float3 up = float3( 0.0, 1.0, 0.0 );

// constants for atmospheric scattering
static const float e = 2.71828182845904523536028747135266249775724709369995957;
//const float pi = 3.141592653589793238462643383279502884197169;

// wavelength of used primaries, according to preetham
static const float3 lambda = float3(680E-9, 550E-9, 450E-9);
// this pre-calcuation replaces older TotalRayleigh(float3 lambda) function:
// (8.0 * pow(pi, 3.0) * pow(pow(n, 2.0) - 1.0, 2.0) * (6.0 + 3.0 * pn)) / (3.0 * N * pow(lambda, float3(4.0)) * (6.0 - 7.0 * pn))
static const float3 totalRayleigh = float3(5.804542996261093E-6, 1.3562911419845635E-5, 3.0265902468824876E-5);

// mie stuff
// K coefficient for the primaries
static const float  v = 4.0;
static const float3 K = float3(0.686, 0.678, 0.666);
// MieConst = pi * pow( ( 2.0 * pi ) / lambda, float3( v - 2.0 ) ) * K
static const float3 MieConst = float3(1.8399918514433978E14, 2.7798023919660528E14, 4.0790479543861094E14);

// earth shadow hack
// cutoffAngle = pi / 1.95;
static const float cutoffAngle = 1.6110731556870734;
static const float steepness   = 1.5;
static const float EE          = 1000.0;

/////////////////

static const float3 cameraPos = float3(0.0, 0.0, 0.0);

// constants for atmospheric scattering
static const float pi = 3.141592653589793238462643383279502884197169;

static const float n = 1.0003;    // refractive index of air
static const float N = 2.545E25;  // number of molecules per unit volume for air at
                                  // 288.15K and 1013mb (sea level -45 celsius)

// optical length at zenith for molecules
static const float  rayleighZenithLength = 8.4E3;
static const float  mieZenithLength      = 1.25E3;
static const float3 up                   = float3(0.0, 1.0, 0.0);
// 66 arc seconds -> degrees, and the cosine of that
static const float sunAngularDiameterCos = 0.999956676946448443553574619906976478926848692873900859324;

// 3.0 / ( 16.0 * pi )
static const float THREE_OVER_SIXTEENPI = 0.05968310365946075;
// 1.0 / ( 4.0 * pi )
static const float ONE_OVER_FOURPI = 0.07957747154594767;

//--------------------------------------------------------------------------------------
// Code
//--------------------------------------------------------------------------------------

float sunIntensity(float zenithAngleCos)
{
    zenithAngleCos = clamp(zenithAngleCos, -1.0, 1.0);
    return EE * max(0.0, 1.0 - pow(e, -((cutoffAngle - acos(zenithAngleCos)) / steepness)));
}

float3 totalMie(float t)
{
    float c = (0.2 * t) * 10E-18;
    return 0.434 * c * MieConst;
}

float rayleighPhase(float cosTheta)
{
    return THREE_OVER_SIXTEENPI * (1.0 + pow(cosTheta, 2.0));
}

float hgPhase(float cosTheta, float g)
{
    float g2      = pow(g, 2.0);
    float inverse = 1.0 / pow(abs(1.0 - 2.0 * g * cosTheta + g2), 1.5);
    return ONE_OVER_FOURPI * ((1.0 - g2) * inverse);
}

float3 getDirection(uint face, float x, float y)
{
    float3 direction = 0.0;

    switch (face)
    {
    case 0:
        direction = float3(1.0, y, -x);
        break;
    case 1:
        direction = float3(-1.0, y, x);
        break;
    case 2:
        direction = float3(x, 1.0, -y);
        break;
    case 3:
        direction = float3(x, -1.0, y);
        break;
    case 4:
        direction = float3(x, y, 1.0);
        break;
    case 5:
        direction = float3(-x, y, -1.0);
        break;
    }

    direction = normalize(direction);

    return direction;
}

float3 getSkyColor(float3 direction)
{
    //this can be done in a VS

    float vSunE = sunIntensity(dot(SunDirection.xyz, up));

    float vSunfade = 1.0 - clamp(1.0 - exp((SunDirection.y)), 0.0, 1.0);

    float rayleighCoefficient = Rayleigh - (1.0 * (1.0 - vSunfade));

    // extinction (absorbtion + out scattering)
    // rayleigh coefficients
    float3 vBetaR = totalRayleigh * rayleighCoefficient;

    // mie coefficients
    float3 vBetaM = totalMie(Turbidity) * MieCoefficient;

    // PS stuff

    // optical length
    // cutoff angle at 90 to avoid singularity in next formula.
    float zenithAngle = acos(max(0.0, dot(up, direction)));
    float inverse     = 1.0 / (cos(zenithAngle) + 0.15 * pow(abs(93.885 - ((zenithAngle * 180.0) / pi)), -1.253));
    float sR          = rayleighZenithLength * inverse;
    float sM          = mieZenithLength * inverse;

    // combined extinction factor
    float3 Fex = exp(-(vBetaR * sR + vBetaM * sM));

    // in scattering
    float cosTheta = dot(direction, SunDirection.xyz);

    float  rPhase     = rayleighPhase(cosTheta * 0.5 + 0.5);
    float3 betaRTheta = vBetaR * rPhase;

    float  mPhase     = hgPhase(cosTheta, MieDirectionalG);
    float3 betaMTheta = vBetaM * mPhase;

    float3 Lin = pow(abs(vSunE * ((betaRTheta + betaMTheta) / (vBetaR + vBetaM)) * (1.0 - Fex)), float3(1.5, 1.5, 1.5));
    Lin *= lerp(float3(1.0, 1.0, 1.0),
                pow(vSunE * ((betaRTheta + betaMTheta) / (vBetaR + vBetaM)) * Fex, float3(1.0 / 2.0, 1.0 / 2.0, 1.0 / 2.0)),
                clamp(pow(1.0 - dot(up, SunDirection.xyz), 5.0), 0.0, 1.0));

    // nightsky
    float theta = acos(direction.y);                // elevation --> y-axis, [-pi/2, pi/2]',
    float phi   = atan2(direction.z, direction.x);  // azimuth --> x-axis [-pi/2, pi/2]',
    //float2 uv = float2( phi, theta ) / float2( 2.0 * pi, pi ) + float2( 0.5, 0.0 );
    float3 L0 = float3(0.1, 0.1, 0.1) * Fex;

    // composition + solar disc
    float sundisk = smoothstep(sunAngularDiameterCos, sunAngularDiameterCos + 0.00002, cosTheta);
    L0 += (vSunE * 19000.0 * Fex) * sundisk;

    float3 texColor = (Lin + L0) * 0.04 + float3(0.0, 0.0003, 0.00075);

    //no tonemapped
    float3 color = (log2(2.0 / pow(Luminance, 4.0))) * texColor;

    float3 retColor = pow(abs(color), (1.0 / (1.2 + (1.2 * vSunfade))));

    return retColor;
}

#if defined(IRRADIANCE_CUBE) || defined(PREFILTERED_CUBE)
float4 sampleEnvironmentCube(float3 n)
{
    return EnvironmentCube.SampleLevel(SamLinearWrap, n, 0);
}
#endif

#ifdef IRRADIANCE_CUBE
float3 getIrradiance(float3 direction)
{
    float3 irradiance = 0.0;

    float3 up    = float3(0.0, 1.0, 0.0);
    float3 right = cross(up, direction);
    up           = cross(direction, right);

    float sampleDelta = 0.025;
    float nrSamples   = 0.0;
    for (float phi = 0.0; phi < 2.0 * pi; phi += sampleDelta)
    {
        for (float theta = 0.0; theta < 0.5 * pi; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            float3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * direction;

            irradiance += sampleEnvironmentCube(sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = pi * irradiance * (1.0 / float(nrSamples));

    return irradiance;
}
#endif

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(NUM_THREAD_X, NUM_THREAD_Y, 1)]
void MainCS(uint3 dtID : SV_DispatchThreadID)
{
#ifdef ENVIRONMENT_CUBE
    const float2 uv = GetUV(dtID.xy, 1.f / float2(ENVIRONMENT_CUBE_X, ENVIRONMENT_CUBE_Y));

    float3 dir = getDirection(dtID.z, 2 * uv.x - 1, 1 - 2 * uv.y);

    EnvironmentCube[uint3(dtID.x, dtID.y, dtID.z)] = float4(getSkyColor(dir), 1.0);
#endif

#ifdef IRRADIANCE_CUBE
    const float2 uv = GetUV(dtID.xy, 1.f / float2(IRRADIANCE_CUBE_X, IRRADIANCE_CUBE_Y));

    float3 dir = getDirection(dtID.z, 2 * uv.x - 1, 1 - 2 * uv.y);

    IrradianceCube[uint3(dtID.x, dtID.y, dtID.z)] = float4(getIrradiance(dir), 1.0);
#endif

#ifdef PREFILTERED_CUBE
    const float2 uv = GetUV(dtID.xy, 1.f / float2(MIP_WIDTH, MIP_WIDTH));

    float3 N = getDirection(dtID.z, 2 * uv.x - 1, 1 - 2 * uv.y);

    // make the simplyfying assumption that V equals R equals the normal
    float3 R = N;
    float3 V = R;

    float3 prefilteredColor = 0.0;
    float  totalWeight      = 0.0;

    // Compute a matrix to rotate the samples
    float3 up        = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
    float3 tangent   = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3x3 tangentToWorld = float3x3(tangent, bitangent, N);

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        // generates a sample vector that's biased towards the preferred alignment direction (importance sampling).
        float3 H = mul(SampleDirections[i].xyz, tangentToWorld);
        float3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);

        if (NdotL > 0.0)
        {
            prefilteredColor += sampleEnvironmentCube(L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor = prefilteredColor / totalWeight;

    PrefilteredCube[uint3(dtID.x, dtID.y, dtID.z)] = float4(prefilteredColor, 1.0);
#endif
}
