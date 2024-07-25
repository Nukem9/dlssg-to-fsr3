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

#include "misc/math.h"

namespace cauldron
{
    Vec4 PolarToVector(float yaw, float pitch)
    {
        return math::Vector4(std::sinf(yaw) * std::cosf(pitch), std::sinf(pitch), std::cosf(yaw) * std::cosf(pitch), 0);
    }

    const Mat4 Perspective(float fovyRadians, float aspect, float zNear, float zFar, bool invertedDepth)
    {
        if (invertedDepth)
        {
            math::Matrix4 perspMatrix;
            const float cotHalfFovY = cosf(0.5f * fovyRadians) / sinf(0.5f * fovyRadians);
            const float m00 = cotHalfFovY / aspect;
            const float m11 = cotHalfFovY;

            math::Vector4 c0(m00, 0.f, 0.f, 0.f);
            math::Vector4 c1(0.f, m11, 0.f, 0.f);
            math::Vector4 c2(0.f, 0.f, 0.f, -1.f);
            math::Vector4 c3(0.f, 0.f, zNear, 0.f);

            perspMatrix.setCol0(c0);
            perspMatrix.setCol1(c1);
            perspMatrix.setCol2(c2);
            perspMatrix.setCol3(c3);

            return perspMatrix;
        }
        else
        {
            return math::Matrix4::perspective(fovyRadians, aspect, zNear, zFar);
        }
    }

    const Mat4 Orthographic(float left, float right, float bottom, float top, float zNear, float zFar, bool invertedDepth)
    {
        // orthographic method maps the depth between -1 and 1, so we have to change the near/far plane to map between 0 and 1
        if (invertedDepth)
            return math::Matrix4::orthographic(left, right, bottom, top, 2.0f * zFar - zNear, zNear);
        else
            return math::Matrix4::orthographic(left, right, bottom, top, 2.0f * zNear - zFar, zFar);
    }

} // namespace cauldron
