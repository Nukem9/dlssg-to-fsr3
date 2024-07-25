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
#include "vectormath/vectormath.hpp"

#define CAULDRON_PI     3.141592654f
#define CAULDRON_2PI    (2.f * CAULDRON_PI)

#define CAULDRON_PI2    (CAULDRON_PI / 2.f)
#define CAULDRON_PI4    (CAULDRON_PI / 4.f)

#define DEG_TO_RAD(degree)  ((degree) * CAULDRON_PI / 180.f)
#define RAD_TO_DEG(radian)  ((radian) * 180.f / CAULDRON_PI)

/// @defgroup CauldronMath Math
/// Sony VectorMath library wrapper.
///
/// @ingroup CauldronMisc

/// A type definition for a 3x3 matrix based on VectorMath::Matrix3.
///
/// @ingroup CauldronMath
using Mat3 = math::Matrix3;

/// A type definition for a 4x4 matrix based on VectorMath::Matrix4.
///
/// @ingroup CauldronMath
using Mat4 = math::Matrix4;

/// A type definition for a 2 component point based on VectorMath::Point2.
///
/// @ingroup CauldronMath
using Point2 = math::Point2;

/// A type definition for a 3 component point based on VectorMath::Point3.
///
/// @ingroup CauldronMath
using Point3 = math::Point3;

/// A type definition for a 2 component vector based on VectorMath::Vector2.
///
/// @ingroup CauldronMath
using Vec2 = math::Vector2;

/// A type definition for a 3 component vector based on VectorMath::Vector3.
///
/// @ingroup CauldronMath
using Vec3 = math::Vector3;

/// A type definition for a 4 component vector based on VectorMath::Vector4.
///
/// @ingroup CauldronMath
using Vec4 = math::Vector4;

namespace cauldron
{
    /// Converts polar rotation to a vector representation
    ///
    /// @param [in] yaw     Yaw angle (in radians).
    /// @param [in] pitch   Pitch angle (in radians).
    ///
    /// @returns            <c><i>Vec4</i></c> representation of the polar rotation.
    ///
    /// @ingroup CauldronMath
    Vec4 PolarToVector(float yaw, float pitch);

    /// Construct a perspective projection matrix with an inverted depth argument
    ///
    /// @param [in] fovyRadians     FOV (in radians).
    /// @param [in] aspect          Aspect ratio.
    /// @param [in] zNear           Near plane.
    /// @param [in] zFar            Far plane.
    /// @param [in] invertedDepth   True if using inverted infinite depth.
    ///
    /// @returns                    <c><i>Mat4</i></c> perspective matrix.
    ///
    /// @ingroup CauldronMath
    const Mat4 Perspective(float fovyRadians, float aspect, float zNear, float zFar, bool invertedDepth);
    
    /// Construct an orthographic projection matrix with z in range [0, 1]
    ///
    /// @param [in] left            Left plane.
    /// @param [in] right           Right plane.
    /// @param [in] bottom          Bottom plane.
    /// @param [in] top             Top plane.
    /// @param [in] zNear           Near plane.
    /// @param [in] zFar            Far plane.
    /// @param [in] invertedDepth   True if using inverted infinite depth.
    ///
    /// @returns                    <c><i>Mat4</i></c> orthographic matrix.
    ///
    /// @ingroup CauldronMath
    const Mat4 Orthographic(float left, float right, float bottom, float top, float zNear, float zFar, bool invertedDepth);

    /// Returns the inverse of a matrix
    ///
    /// @param [in] srcMatrix       The matrix to inverse.
    ///
    /// @returns            <c><i>Mat4</i></c> inverted matrix.
    ///
    /// @ingroup CauldronMath
    inline Mat4 InverseMatrix(const Mat4& srcMatrix)
    {
        return math::inverse(srcMatrix);
    }

    /// Returns the inverse of a matrix
    ///
    /// @param [in] srcMatrix       The matrix to inverse.
    ///
    /// @returns                    <c><i>Mat3</i></c> inverted matrix.
    ///
    /// @ingroup CauldronMath
    inline Mat3 InverseMatrix(const Mat3& srcMatrix)
    {
        return math::inverse(srcMatrix);
    }

    /// Construct a matrix positioned at 'eye' and rotated to face 'lookAt'
    ///
    /// @param [in] eye             Source (translation).
    /// @param [in] lookAt          Look at (direction).
    /// @param [in] up              Up vector to orient the matrix properly.
    ///
    /// @returns                    <c><i>Mat4</i></c> matrix.
    ///
    /// @ingroup CauldronMath
    inline Mat4 LookAtMatrix(Vec4 eye, Vec4 lookAt, Vec4 up)
    {
        return math::Matrix4::lookAt(math::toPoint3(eye), math::toPoint3(lookAt), up.getXYZ());
    }

    /// Returns the transpose of a matrix
    ///
    /// @param [in] srcMatrix       The matrix to transpose.
    ///
    /// @returns            <c><i>Mat4</i></c> transposed matrix.
    ///
    /// @ingroup CauldronMath
    inline Mat4 TransposeMatrix(const Mat4& srcMatrix)
    {
        return math::transpose(srcMatrix);
    }

    /// Returns a vector composed of per-element minimums
    ///
    /// @param [in] vec1            Vector1.
    /// @param [in] vec2            Vector2.
    ///
    /// @returns                    <c><i>Vec4</i></c> the 4-component vector of minimums per element.
    ///
    /// @ingroup CauldronMath
    inline Vec4 MinPerElement(const Vec4& vec1, const Vec4& vec2)
    {
#if VECTORMATH_MODE_SSE
        return math::SSE::minPerElem(vec1, vec2);
#else
        return math::Scalar::minPerElem(vec1, vec2);
#endif

    }

    /// Returns a vector composed of per-element maximums
    ///
    /// @param [in] vec1            Vector1.
    /// @param [in] vec2            Vector2.
    ///
    /// @returns                    <c><i>Vec4</i></c> the 4-component vector of maximums per element.
    ///
    /// @ingroup CauldronMath
    inline Vec4 MaxPerElement(const Vec4& vec1, const Vec4& vec2)
    {
#if VECTORMATH_MODE_SSE
        return math::SSE::maxPerElem(vec1, vec2);
#else
        return math::Scalar::maxPerElem(vec1, vec2);
#endif

    }

} // namespace cauldron
