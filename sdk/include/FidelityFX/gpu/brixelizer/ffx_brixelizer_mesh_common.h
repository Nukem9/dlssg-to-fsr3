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

#ifndef FFX_BRIXELIZER_MESH_COMMON_H
#define FFX_BRIXELIZER_MESH_COMMON_H

#include "ffx_brixelizer_common_private.h"

struct FfxBrixelizerTrianglePos {
    FfxUInt32x3  face3;

    FfxFloat32x3 wp0;
    FfxFloat32x3 wp1;
    FfxFloat32x3 wp2;
};

FfxFloat32x3 FFX_Fetch_PositionRGBA16(FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    FfxUInt32x2 pack = LoadVertexBufferUInt2(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
    
    FfxFloat32x2 xy = ffxUnpackF32(pack.x);
    FfxFloat32x2 zw = ffxUnpackF32(pack.y);

#if 0
    FfxFloat32  x    = f16tof32((pack.x >> FfxUInt32(0)) & FfxUInt32(0xFFFF));
    FfxFloat32  y    = f16tof32((pack.x >> FfxUInt32(16)) & FfxUInt32(0xFFFF));
    FfxFloat32  z    = f16tof32((pack.y >> FfxUInt32(0)) & FfxUInt32(0xFFFF));
    FfxFloat32  w    = f16tof32((pack.y >> FfxUInt32(16)) & FfxUInt32(0xFFFF));
#endif

    return FfxFloat32x4(xy, zw).xyz;
}

FfxFloat32x4 FFX_Fetch_Unorm4(FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    FfxUInt32  pack = LoadVertexBufferUInt(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
    FfxFloat32 x    = FfxFloat32((pack >> FfxUInt32(0)) & FfxUInt32(0xFF)) / FfxFloat32(255.0);
    FfxFloat32 y    = FfxFloat32((pack >> FfxUInt32(8)) & FfxUInt32(0xFF)) / FfxFloat32(255.0);
    FfxFloat32 z    = FfxFloat32((pack >> FfxUInt32(16)) & FfxUInt32(0xFF)) / FfxFloat32(255.0);
    FfxFloat32 w    = FfxFloat32((pack >> FfxUInt32(24)) & FfxUInt32(0xFF)) / FfxFloat32(255.0);
    return FfxFloat32x4(x, y, z, w);
}

FfxFloat32x2 FFX_Fetch_RG16_UNORM(FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    FfxUInt32  pack = LoadVertexBufferUInt(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
    FfxFloat32 x    = FfxFloat32((pack >> FfxUInt32(0)) & 0xFFFFu) / FfxFloat32(65535.0);
    FfxFloat32 y    = FfxFloat32((pack >> FfxUInt32(16)) & 0xFFFFu) / FfxFloat32(65535.0);
    return FfxFloat32x2(x, y);
}

void FFX_Fetch_Face_Indices_U32(FfxUInt32 buffer_id, FFX_PARAMETER_OUT FfxUInt32x3 face3, FfxUInt32 offset, FfxUInt32 triangle_id)
{
    face3 = LoadVertexBufferUInt3(buffer_id, (offset + FfxUInt32(12) * triangle_id) / FFX_BRIXELIZER_SIZEOF_UINT);
}

void FFX_Fetch_Face_Indices_U16(FfxUInt32 buffer_id, FFX_PARAMETER_OUT FfxUInt32x3 face3, FfxUInt32 offset, FfxUInt32 triangle_id)
{
    FfxUInt32 word_offset = offset / 2;

    FfxUInt32 word_id_0  = word_offset + triangle_id * FfxUInt32(3) + FfxUInt32(0);
    FfxUInt32 dword_id_0 = word_id_0 / FfxUInt32(2);
    FfxUInt32 shift_0    = FfxUInt32(16) * (word_id_0 & FfxUInt32(1));

    FfxUInt32 word_id_1  = word_offset + triangle_id * FfxUInt32(3) + FfxUInt32(1);
    FfxUInt32 dword_id_1 = word_id_1 / FfxUInt32(2);
    FfxUInt32 shift_1    = FfxUInt32(16) * (word_id_1 & FfxUInt32(1));

    FfxUInt32 word_id_2  = word_offset + triangle_id * FfxUInt32(3) + FfxUInt32(2);
    FfxUInt32 dword_id_2 = word_id_2 / FfxUInt32(2);
    FfxUInt32 shift_2    = FfxUInt32(16) * (word_id_2 & FfxUInt32(1));

    FfxUInt32 u0 = LoadVertexBufferUInt(buffer_id, dword_id_0);
    u0      = (u0 >> shift_0) & FfxUInt32(0xFFFF);
    FfxUInt32 u1 = LoadVertexBufferUInt(buffer_id, dword_id_1);
    u1      = (u1 >> shift_1) & FfxUInt32(0xFFFF);
    FfxUInt32 u2 = LoadVertexBufferUInt(buffer_id, dword_id_2);
    u2      = (u2 >> shift_2) & FfxUInt32(0xFFFF);
    face3   = FfxUInt32x3(u0, u1, u2);
}

FfxFloat32x2 FFX_Fetch_float2(FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    return LoadVertexBufferFloat2(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
}

FfxFloat32x3 FFX_Fetch_float3(FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    return LoadVertexBufferFloat3(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
}

FfxFloat32x4 FFX_Fetch_float4(FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    return LoadVertexBufferFloat4(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
}

FfxFloat32x3 FFX_Fetch_Position(FfxUInt32 flags, FfxUInt32 buffer_id, FfxUInt32 offset, FfxUInt32 vertex_id, FfxUInt32 stride)
{
    if (FFX_HAS_FLAG(flags, FFX_BRIXELIZER_INSTANCE_FLAG_USE_RGBA16_VERTEX)) {
        FfxUInt32x2 pack = LoadVertexBufferUInt2(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
        
        FfxFloat32x2 xy = ffxUnpackF32(pack.x);
        FfxFloat32x2 zw = ffxUnpackF32(pack.y);

        return FfxFloat32x4(xy, zw).xyz;
    } else
        return LoadVertexBufferFloat3(buffer_id, (offset + stride * vertex_id) / FFX_BRIXELIZER_SIZEOF_UINT);
}

FfxUInt32x3 FfxBrixelizerFetchFace(FfxUInt32 flags, FfxUInt32 indexBufferID, FfxUInt32 indexBufferOffset, FfxUInt32 triangle_idx)
{
    if (FFX_HAS_FLAG(flags, FFX_BRIXELIZER_INSTANCE_FLAG_USE_INDEXLESS_QUAD_LIST)) { // Procedural quad index buffer
        FfxUInt32 quad_id     = triangle_idx / FfxUInt32(2);               // 2 triangles per quad
        FfxUInt32 base_vertex = quad_id * FfxUInt32(4);                    // 4 vertices per quad
        if (FFX_HAS_FLAG(triangle_idx, FfxUInt32(1))) {
            return FFX_BROADCAST_UINT32X3(base_vertex) + FfxUInt32x3(FfxUInt32(2), FfxUInt32(3), FfxUInt32(0));
        } else {
            return FFX_BROADCAST_UINT32X3(base_vertex) + FfxUInt32x3(FfxUInt32(0), FfxUInt32(1), FfxUInt32(2));
        }
    } else {
        FfxUInt32x3 face3;
        if (FFX_HAS_FLAG(flags, FFX_BRIXELIZER_INSTANCE_FLAG_USE_U16_INDEX)) {
            FFX_Fetch_Face_Indices_U16(indexBufferID, /* out */ face3, /* in */ indexBufferOffset, /* in */ triangle_idx);
        } else { // FFX_BRIXELIZER_INDEX_TYPE_U32
            FFX_Fetch_Face_Indices_U32(indexBufferID, /* out */ face3, /* in */ indexBufferOffset, /* in */ triangle_idx);
        }
        return face3;
    }
}

FfxBrixelizerTrianglePos FfxBrixelizerFetchTriangle(FfxUInt32 flags, FfxUInt32 indexBufferID, FfxUInt32 indexBufferOffset, FfxUInt32 vertexBufferID, FfxUInt32 vertexBufferOffset, FfxUInt32 vertexBufferStride,
                                        FfxUInt32 instance_idx, FfxUInt32 triangle_idx)
{
    FfxUInt32x3              face3 = FfxBrixelizerFetchFace(flags, indexBufferID, indexBufferOffset, triangle_idx);
    FfxFloat32x3             p0    = FFX_Fetch_Position(flags, vertexBufferID, vertexBufferOffset, face3.x, vertexBufferStride);
    FfxFloat32x3             p1    = FFX_Fetch_Position(flags, vertexBufferID, vertexBufferOffset, face3.y, vertexBufferStride);
    FfxFloat32x3             p2    = FFX_Fetch_Position(flags, vertexBufferID, vertexBufferOffset, face3.z, vertexBufferStride);
    FfxBrixelizerTrianglePos tri;

    FfxFloat32x3x4 obj_to_anchor = LoadInstanceTransform(instance_idx);
    tri.face3                    = face3;
    tri.wp0                      = FFX_TRANSFORM_VECTOR(obj_to_anchor, FfxFloat32x4(p0, FfxFloat32(1.0))).xyz;
    tri.wp1                      = FFX_TRANSFORM_VECTOR(obj_to_anchor, FfxFloat32x4(p1, FfxFloat32(1.0))).xyz;
    tri.wp2                      = FFX_TRANSFORM_VECTOR(obj_to_anchor, FfxFloat32x4(p2, FfxFloat32(1.0))).xyz;
    return tri;
}

FfxBrixelizerTrianglePos FfxBrixelizerFetchTriangle(FfxBrixelizerBasicMeshInfo mesh_info, FfxUInt32 instance_id, FfxUInt32 triangle_id)
{
    return FfxBrixelizerFetchTriangle(mesh_info.flags, mesh_info.indexBufferID, mesh_info.indexBufferOffset, mesh_info.vertexBufferID, mesh_info.vertexBufferOffset,
                                mesh_info.vertexStride, instance_id, triangle_id);
}

#endif // FFX_BRIXELIZER_MESH_COMMON_H