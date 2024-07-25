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

#include "surfacerendercommon.h"

//////////////////////////////////////////////////////////////////////////
// Resources
//////////////////////////////////////////////////////////////////////////

Texture2D    AllTextures[] : register(t0);
SamplerState AllSamplers[] : register(s0);

cbuffer CBSceneInformation : register(b0)
{
    SceneInformation SceneInfo;
};
cbuffer CBInstanceInformation : register(b1)
{
    InstanceInformation InstanceInfo;
};
cbuffer CBTextureIndices : register(b2)
{
    TextureIndices Textures;
}

void DiscardPixelIfAlphaCutOff(VS_SURFACE_OUTPUT Input)
{
#if defined(DEF_alphaMode_MASK) && defined(DEF_alphaCutoff)
    float4 BaseColorAlpha = GetBaseColorAlpha(Input, InstanceInfo.MaterialInfo, Textures, AllTextures, AllSamplers, SceneInfo.MipLODBias);
    if (BaseColorAlpha.a < DEF_alphaCutoff)
        discard;
#endif
}

void MainPS(VS_SURFACE_OUTPUT SurfaceInput)
{
    DiscardPixelIfAlphaCutOff(SurfaceInput);
}
