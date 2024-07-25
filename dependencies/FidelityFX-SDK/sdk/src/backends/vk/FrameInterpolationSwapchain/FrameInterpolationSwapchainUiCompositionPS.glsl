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

#version 450

#extension GL_EXT_samplerless_texture_functions : require

layout(set = 0, binding = 0) uniform texture2D r_currBB;
layout(set = 0, binding = 1) uniform texture2D r_uiTexture;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = texelFetch(r_currBB, ivec2(gl_FragCoord.xy), 0).rgb;
    vec4 guiColor = texelFetch(r_uiTexture, ivec2(gl_FragCoord.xy), 0);

#if FFX_UI_PREMUL
    outColor = vec4((1.0 - guiColor.a) * color + guiColor.rgb, 1.0); // premul
#else
    outColor = vec4(mix(color, guiColor.rgb, guiColor.a), 1.0); // blend (original)
#endif
    
}