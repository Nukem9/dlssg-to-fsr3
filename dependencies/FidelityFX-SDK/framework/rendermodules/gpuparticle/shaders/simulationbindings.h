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

#include "particlecommon.h"
#include "particlesimulationcommon.h"


// The particle buffers to fill with new particles
RWStructuredBuffer<GPUParticlePartA>        g_ParticleBufferA           : register( u0 );
RWStructuredBuffer<GPUParticlePartB>        g_ParticleBufferB           : register( u1 );

// The dead list, so any particles that are retired this frame can be added to this list. The first element is the number of dead particles
RWStructuredBuffer<int>                     g_DeadList                  : register( u2 );

// The alive list which gets built in the similution.  The distances also get written out
RWStructuredBuffer<int>                     g_IndexBuffer               : register( u3 );
RWStructuredBuffer<float>                   g_DistanceBuffer            : register( u4 );

// The maximum radius in XY is calculated here and stored
RWStructuredBuffer<float>                   g_MaxRadiusBuffer           : register( u5 );

// Viewspace particle positions are calculated here and stored
RWStructuredBuffer<uint2>                   g_PackedViewSpacePositions  : register( u6 );

// The draw args for the ExecuteIndirect call needs to be filled in before the rasterization path is called, so do it here
RWStructuredBuffer<IndirectCommand>         g_DrawArgs                  : register( u7 );

RWStructuredBuffer<uint>                    g_AliveParticleCount        : register( u8 );

// The opaque scene's depth buffer read as a texture
Texture2D                                  g_DepthBuffer               : register( t0 );

// A texture filled with random values for generating some variance in our particles when we spawn them
Texture2D                                  g_RandomBuffer              : register( t1 );


SamplerState g_samWrapPoint : register( s0 );
