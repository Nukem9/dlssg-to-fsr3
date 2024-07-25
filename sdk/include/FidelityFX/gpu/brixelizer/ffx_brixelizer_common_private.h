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

#ifndef FFX_BRIXELIZER_COMMON_PRIVATE_H
#define FFX_BRIXELIZER_COMMON_PRIVATE_H

#include "ffx_brixelizer_common.h"
#include "ffx_brixelizer_host_gpu_shared_private.h"

FfxInt32x3        to_int3(FfxUInt32x3 a) { return FfxInt32x3(FfxInt32(a.x), FfxInt32(a.y), FfxInt32(a.z)); }
FfxFloat32x3      to_float3(FfxUInt32x3 a) { return FfxFloat32x3(FfxFloat32(a.x), FfxFloat32(a.y), FfxFloat32(a.z)); }
FfxFloat32x3      to_float3(FfxInt32x3 a) { return FfxFloat32x3(FfxFloat32(a.x), FfxFloat32(a.y), FfxFloat32(a.z)); }

FfxUInt32 FfxBrixelizerPackDistance(FfxFloat32 distance)
{
    FfxUInt32 uval     = ffxAsUInt32(distance);
    FfxUInt32 sign_bit = uval >> FfxUInt32(31);
    return (uval << 1) | sign_bit;
}

FfxFloat32 FfxBrixelizerUnpackDistance(FfxUInt32 uval)
{
    FfxUInt32 sign_bit = (uval & 1);
    return ffxAsFloat((uval >> FfxUInt32(1)) | (sign_bit << FfxUInt32(31)));
}

// Returns the minimal absolute value with its sign unchanged
FfxFloat32 FfxBrixelizerUnsignedMin(FfxFloat32 a, FfxFloat32 b) { return abs(a) < abs(b) ? a : b; }
// sign without zero
FfxFloat32 FfxBrixelizerGetSign(FfxFloat32 v) { return v < FfxFloat32(0.0) ? -FfxFloat32(1.0) : FfxFloat32(1.0); }

FfxFloat32 dot2(FfxFloat32x3 v) { return dot(v, v); }
FfxFloat32 dot2(FfxFloat32x2 v) { return dot(v, v); }

// https://www.shadertoy.com/view/4sXXRN
// The MIT License
// Copyright © 2014 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// It computes the distance to a triangle.
//
// See here: http://iquilezles.org/www/articles/triangledistance/triangledistance.htm
//
// In case a mesh was rendered, only one square root would be needed for the
// whole mesh. In this example the triangle is given a thckness of 0.01 units
// for display purposes. Like the square root, this thickness should be added
// only once for the whole mesh too.
FfxFloat32 CalculateDistanceToTriangle(FfxFloat32x3 p, FfxFloat32x3 a, FfxFloat32x3 b, FfxFloat32x3 c) {
    FfxFloat32x3 ba  = b - a;
    FfxFloat32x3 pa  = p - a;
    FfxFloat32x3 cb  = c - b;
    FfxFloat32x3 pb  = p - b;
    FfxFloat32x3 ac  = a - c;
    FfxFloat32x3 pc  = p - c;
    FfxFloat32x3 nor = cross(ba, ac);

    return sqrt(                                                       //
        (sign(dot(cross(ba, nor), pa)) +                               //
             sign(dot(cross(cb, nor), pb)) +                           //
             sign(dot(cross(ac, nor), pc)) <                           //
         FfxFloat32(2.0))                                              //
            ?                                                          //
            min(min(                                                   //
                    dot2(ba * ffxSaturate(dot(ba, pa) / dot2(ba)) - pa),  //
                    dot2(cb * ffxSaturate(dot(cb, pb) / dot2(cb)) - pb)), //
                dot2(ac * ffxSaturate(dot(ac, pc) / dot2(ac)) - pc))      //
            :                                                          //
            dot(nor, pa) * dot(nor, pa) / dot2(nor)                    //
    );                                                                 //
}

FfxFloat32 CalculateDistanceToTriangleSquared(FfxFloat32x3 ba,           //
                                              FfxFloat32x3 pa,           //
                                              FfxFloat32x3 cb,           //
                                              FfxFloat32x3 pb,           //
                                              FfxFloat32x3 ac,           //
                                              FfxFloat32x3 pc,           //
                                              FfxFloat32x3 nor,          //
                                              FfxFloat32x3 cross_ba_nor, //
                                              FfxFloat32x3 cross_cb_nor, //
                                              FfxFloat32x3 cross_ac_nor, //
                                              FfxFloat32  dot2_ba,      //
                                              FfxFloat32  dot2_cb,      //
                                              FfxFloat32  dot2_ac,      //
                                              FfxFloat32  dot2_nor      //
) {
    return                                                                //
        (                                                                 //
            (sign(dot(cross_ba_nor, pa)) +                                //
                 sign(dot(cross_cb_nor, pb)) +                            //
                 sign(dot(cross_ac_nor, pc)) <                            //
             FfxFloat32(2.0))                                             //
                ?                                                         //
                min(min(                                                  //
                        dot2(ba * ffxSaturate(dot(ba, pa) / dot2_ba) - pa),  //
                        dot2(cb * ffxSaturate(dot(cb, pb) / dot2_cb) - pb)), //
                    dot2(ac * ffxSaturate(dot(ac, pc) / dot2_ac) - pc))      //
                :                                                         //
                dot(nor, pa) * dot(nor, pa) / dot2_nor                    //
        );                                                                //
}

#define brixelizerreal    FfxFloat32
#define brixelizerreal2   FfxFloat32x2
#define brixelizerreal3   FfxFloat32x3
#define brixelizerreal4   FfxFloat32x4

#ifdef FFX_HLSL
#    define brixelizerreal3x2 float3x2
#else
#    define brixelizerreal3x2 mat3x2
#endif

brixelizerreal3 to_brixelizerreal3(FfxUInt32x3 a) { return brixelizerreal3(brixelizerreal(a.x), brixelizerreal(a.y), brixelizerreal(a.z)); }

// Offsets the plane equation for the nearest grid point
brixelizerreal FfxBrixelizerOffsetByMax(brixelizerreal de, brixelizerreal2 ne, brixelizerreal offset)
{
    de += max(FfxFloat32(0.0), ne.x) + abs(ne.x * offset);
    de += max(FfxFloat32(0.0), ne.y) + abs(ne.y * offset);
    return de;
}

// Offsets the plane equation for the next grid point
brixelizerreal FfxBrixelizerOffsetByMin(brixelizerreal de, brixelizerreal2 ne, brixelizerreal offset)
{
    de += min(FfxFloat32(0.0), ne.x) - abs(ne.x * offset);
    de += min(FfxFloat32(0.0), ne.y) - abs(ne.x * offset);
    return de;
}

void FfxBrixelizerGet2DEdge(FFX_PARAMETER_OUT brixelizerreal2 ne,
                      FFX_PARAMETER_OUT brixelizerreal  de,
                      brixelizerreal orientation,
                      brixelizerreal edge_x,
                      brixelizerreal edge_y,
                      brixelizerreal vertex_x,
                      brixelizerreal vertex_y,
                      brixelizerreal offset)
{
    ne = brixelizerreal2(-orientation * edge_y, orientation * edge_x);
    de = -(ne.x * vertex_x + ne.y * vertex_y);
    de = FfxBrixelizerOffsetByMax(de, ne, offset);
}

void FfxBrixelizerGet2DEdges(
    FFX_PARAMETER_OUT brixelizerreal3   de_xy,
    FFX_PARAMETER_OUT brixelizerreal3x2 ne_xy,
    FFX_PARAMETER_OUT brixelizerreal3   de_xz,
    FFX_PARAMETER_OUT brixelizerreal3x2 ne_xz,
    FFX_PARAMETER_OUT brixelizerreal3   de_yz,
    FFX_PARAMETER_OUT brixelizerreal3x2 ne_yz,
    FFX_PARAMETER_OUT brixelizerreal3   gn,
    brixelizerreal3 TRIANGLE_VERTEX_0,
    brixelizerreal3 TRIANGLE_VERTEX_1,
    brixelizerreal3 TRIANGLE_VERTEX_2,
    brixelizerreal  offset,
    bool    invert)
{
    brixelizerreal3 e0 = TRIANGLE_VERTEX_1.xyz - TRIANGLE_VERTEX_0.xyz;
    brixelizerreal3 e1 = TRIANGLE_VERTEX_2.xyz - TRIANGLE_VERTEX_1.xyz;
    brixelizerreal3 e2 = TRIANGLE_VERTEX_0.xyz - TRIANGLE_VERTEX_2.xyz;
    if (invert) {
        e0 = -e0;
        e1 = -e1;
        e2 = -e2;
    }

    gn = normalize(cross(e2, e0));

    brixelizerreal orientation_xy = brixelizerreal(gn.z < FfxFloat32(0.0) ? -FfxFloat32(1.0) : FfxFloat32(1.0));
    FfxBrixelizerGet2DEdge(/* out */ ne_xy[0], /* out */ de_xy[0], orientation_xy, e0.x, e0.y, TRIANGLE_VERTEX_0.x, TRIANGLE_VERTEX_0.y, offset);
    FfxBrixelizerGet2DEdge(/* out */ ne_xy[1], /* out */ de_xy[1], orientation_xy, e1.x, e1.y, TRIANGLE_VERTEX_1.x, TRIANGLE_VERTEX_1.y, offset);
    FfxBrixelizerGet2DEdge(/* out */ ne_xy[2], /* out */ de_xy[2], orientation_xy, e2.x, e2.y, TRIANGLE_VERTEX_2.x, TRIANGLE_VERTEX_2.y, offset);
    brixelizerreal orientation_xz = brixelizerreal(gn.y > FfxFloat32(0.0) ? -FfxFloat32(1.0) : FfxFloat32(1.0));
    FfxBrixelizerGet2DEdge(/* out */ ne_xz[0], /* out */ de_xz[0], orientation_xz, e0.x, e0.z, TRIANGLE_VERTEX_0.x, TRIANGLE_VERTEX_0.z, offset);
    FfxBrixelizerGet2DEdge(/* out */ ne_xz[1], /* out */ de_xz[1], orientation_xz, e1.x, e1.z, TRIANGLE_VERTEX_1.x, TRIANGLE_VERTEX_1.z, offset);
    FfxBrixelizerGet2DEdge(/* out */ ne_xz[2], /* out */ de_xz[2], orientation_xz, e2.x, e2.z, TRIANGLE_VERTEX_2.x, TRIANGLE_VERTEX_2.z, offset);
    brixelizerreal orientation_yz = brixelizerreal(gn.x < FfxFloat32(0.0) ? -FfxFloat32(1.0) : FfxFloat32(1.0));
    FfxBrixelizerGet2DEdge(/* out */ ne_yz[0], /* out */ de_yz[0], orientation_yz, e0.y, e0.z, TRIANGLE_VERTEX_0.y, TRIANGLE_VERTEX_0.z, offset);
    FfxBrixelizerGet2DEdge(/* out */ ne_yz[1], /* out */ de_yz[1], orientation_yz, e1.y, e1.z, TRIANGLE_VERTEX_1.y, TRIANGLE_VERTEX_1.z, offset);
    FfxBrixelizerGet2DEdge(/* out */ ne_yz[2], /* out */ de_yz[2], orientation_yz, e2.y, e2.z, TRIANGLE_VERTEX_2.y, TRIANGLE_VERTEX_2.z, offset);
}

bool FfxBrixelizerEvalEdge(brixelizerreal2 vertex, brixelizerreal3 de, brixelizerreal3x2 ne)
{
    return (ne[0].x * vertex.x + ne[0].y * vertex.y + de[0] >= brixelizerreal(FfxFloat32(0.0))) &&
           (ne[1].x * vertex.x + ne[1].y * vertex.y + de[1] >= brixelizerreal(FfxFloat32(0.0))) &&
           (ne[2].x * vertex.x + ne[2].y * vertex.y + de[2] >= brixelizerreal(FfxFloat32(0.0)));
}

// We need those non-POT versions around for culling
FfxUInt32x3 FfxBrixelizerUnflatten(FfxUInt32 flat_bx_coord, FfxUInt32x3 dim)
{
    return FfxUInt32x3(flat_bx_coord % dim.x, (flat_bx_coord / dim.x) % dim.y, flat_bx_coord / (dim.x * dim.y));
}

FfxUInt32x2 FfxBrixelizerUnflatten(FfxUInt32 flat_bx_coord, FfxUInt32x2 dim)
{
    return FfxUInt32x2(flat_bx_coord % dim.x, (flat_bx_coord / dim.x) % dim.y);
}

FfxUInt32  FfxBrixelizerFlatten(FfxUInt32x3 voxel_coord, FfxUInt32x3 dim)
{
    return voxel_coord.x + voxel_coord.y * dim.x + voxel_coord.z * dim.x * dim.y;
}

#endif // FFX_BRIXELIZER_COMMON_PRIVATE_H