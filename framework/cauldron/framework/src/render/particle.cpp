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

#include "render/particle.h"
#include "render/device.h"
#include "render/dynamicresourcepool.h"
#include "core/framework.h"
#include "core/loaders/textureloader.h"

#include "shaders/particlecommon.h"

namespace cauldron
{
    inline float RandomVariance(float median, float variance)
    {
        float fUnitRandomValue = (float)rand() / (float)RAND_MAX;
        float fRange           = variance * fUnitRandomValue;
        return median - variance + (2.0f * fRange);
    }

    ParticleSystem::ParticleSystem(const ParticleSpawnerDesc& particleSpawnerDesc)
    {
        m_Name     = particleSpawnerDesc.Name;
        m_Position = particleSpawnerDesc.Position;
        for (auto& emitterDesc : particleSpawnerDesc.Emitters)
        {
            m_Emitters.push_back(Emitter{emitterDesc.EmitterName,
                                         emitterDesc.SpawnOffset,
                                         emitterDesc.SpawnOffsetVariance,
                                         emitterDesc.SpawnVelocity,
                                         emitterDesc.SpawnVelocityVariance,
                                         emitterDesc.ParticlesPerSecond,
                                         emitterDesc.Lifespan,
                                         emitterDesc.SpawnSize,
                                         emitterDesc.KillSize,
                                         emitterDesc.Mass,
                                         emitterDesc.AtlasIndex,
                                         emitterDesc.Flags});
        }

        m_Sort = particleSpawnerDesc.Sort;

        m_StartColor[0] = Vec4(0.3f, 0.3f, 0.3f, 0.4f);
        m_EndColor[0]   = Vec4(0.4f, 0.4f, 0.4f, 0.1f);
        m_StartColor[1] = Vec4(10.0f, 10.0f, 10.0f, 0.9f);
        m_EndColor[1]   = Vec4(5.0f, 8.0f, 5.0f, 0.1f);

        m_ReadBufferStates    = ResourceState::CommonResource;
        m_WriteBufferStates   = ResourceState::CommonResource;
        m_StridedBufferStates = ResourceState::CommonResource;

        // Create the global particle pool. Each particle is split into two parts for better cache coherency. The first half contains the data more
        // relevant to rendering while the second half is more related to simulation
        BufferDesc bufferDescParticlesA =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_ParticleBufferA").c_str(), sizeof(GPUParticlePartA) * g_maxParticles, sizeof(GPUParticlePartA), 0, ResourceFlags::AllowUnorderedAccess);
        m_pParticleBufferA = GetDynamicResourcePool()->CreateBuffer(&bufferDescParticlesA, m_ReadBufferStates);

        BufferDesc bufferDescParticlesB =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_ParticleBufferB").c_str(), sizeof(GPUParticlePartB) * g_maxParticles, sizeof(GPUParticlePartB), 0, ResourceFlags::AllowUnorderedAccess);
        m_pParticleBufferB = GetDynamicResourcePool()->CreateBuffer(&bufferDescParticlesB, m_WriteBufferStates);

        // The packed view space positions of particles are cached during simulation so allocate a buffer for them
        BufferDesc bufferDescPackedViewSpaceParticlePositions =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_PackedViewSpaceParticlePositions").c_str(), 8 * g_maxParticles, 8, 0, ResourceFlags::AllowUnorderedAccess);
        m_pPackedViewSpaceParticlePositions = GetDynamicResourcePool()->CreateBuffer(&bufferDescPackedViewSpaceParticlePositions, m_ReadBufferStates);

        // The maximum radii of each particle is cached during simulation to avoid recomputing multiple times later. This is only required
        // for streaked particles as they are not round so we cache the max radius of X and Y
        BufferDesc bufferDescMaxRadiusBuffer =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_MaxRadiusBuffer").c_str(), sizeof(float) * g_maxParticles, sizeof(float), 0, ResourceFlags::AllowUnorderedAccess);
        m_pMaxRadiusBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescMaxRadiusBuffer, m_ReadBufferStates);

        // The dead particle index list. Created as an append buffer
        BufferDesc bufferDescDeadListBuffer =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_DeadListBuffer").c_str(), sizeof(INT) * (g_maxParticles + 1), sizeof(INT), 0, ResourceFlags::AllowUnorderedAccess);
        m_pDeadListBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescDeadListBuffer, m_WriteBufferStates);

        // Create the index buffer of alive particles that is to be sorted (at least in the rasterization path).
        // For the tiled rendering path this could be just a UINT index buffer as particles are not globally sorted
        BufferDesc bufferDescAliveIndexBuffer =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_AliveIndexBuffer").c_str(), sizeof(int) * g_maxParticles, sizeof(int), 0, ResourceFlags::AllowUnorderedAccess);
        m_pAliveIndexBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescAliveIndexBuffer, m_ReadBufferStates);

        BufferDesc bufferDescAliveDistanceBuffer =
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_AliveDistanceBuffer").c_str(), sizeof(float) * g_maxParticles, sizeof(float), 0, ResourceFlags::AllowUnorderedAccess);
        m_pAliveDistanceBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescAliveDistanceBuffer, m_WriteBufferStates);

        // Create the single element buffer which is used to store the count of alive particles
        BufferDesc bufferDescAliveCountBuffer = 
            BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_AliveCountBuffer").c_str(), sizeof(UINT), sizeof(UINT), 0, ResourceFlags::AllowUnorderedAccess);
        m_pAliveCountBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescAliveCountBuffer, m_ReadBufferStates);

        // Create the buffer to store the indirect args for the ExecuteIndirect call
        // Create the index buffer of alive particles that is to be sorted (at least in the rasterization path).
        BufferDesc bufferDescIndirectArgsBuffer = BufferDesc::Data(std::wstring(particleSpawnerDesc.Name + L"_IndirectArgsBuffer").c_str(),
                                                                   sizeof(IndirectCommand),
                                                                   sizeof(IndirectCommand),
                                                                   0,
                                                                   static_cast<ResourceFlags>(ResourceFlags::AllowUnorderedAccess | ResourceFlags::AllowIndirect));
        m_pIndirectArgsBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescIndirectArgsBuffer, m_WriteBufferStates);

        // Create the particle billboard index buffer required for the rasterization VS-only path
        ResourceFormat format;
        switch (sizeof(UINT))
        {
        case 2:
            format = ResourceFormat::R16_UINT;
            break;
        case 4:
            format = ResourceFormat::R32_UINT;
            break;
        default:
            CauldronWarning(L"Unsupported component type for index buffer.");
            return;
        }

        BufferDesc bufferDescIndexBuffer = BufferDesc::Index(std::wstring(m_Name + L"_IndexBuffer").c_str(), sizeof(UINT) * g_maxParticles * 6, format);
        m_pIndexBuffer                   = GetDynamicResourcePool()->CreateBuffer(&bufferDescIndexBuffer, ResourceState::CopyDest);

        UINT* indices = new UINT[g_maxParticles * 6];
        UINT* ptr     = indices;
        UINT  base    = 0;
        for (int i = 0; i < g_maxParticles; i++)
        {
            ptr[0] = base + 0;
            ptr[1] = base + 1;
            ptr[2] = base + 2;

            ptr[3] = base + 2;
            ptr[4] = base + 1;
            ptr[5] = base + 3;

            base += 4;
            ptr += 6;
        }
        const_cast<Buffer*>(m_pIndexBuffer)->CopyData(indices, sizeof(UINT) * g_maxParticles * 6);
        // Once done, auto-enqueue a barrier for start of next frame so it's usable
        Barrier bufferTransition = Barrier::Transition(m_pIndexBuffer->GetResource(), ResourceState::CopyDest, ResourceState::IndexBufferResource);
        GetDevice()->ExecuteResourceTransitionImmediate(1, &bufferTransition);
        delete[] indices;

        // Initialize the random numbers texture
        TextureDesc textureDesc = TextureDesc::Tex2D(std::wstring(m_Name + L"_RadomTexture").c_str(), ResourceFormat::RGBA32_FLOAT, 1024, 1024, 1, 1);
        m_pRandomTexture        = GetDynamicResourcePool()->CreateTexture(&textureDesc, ResourceState::CopyDest);

        if (m_pRandomTexture)
        {
            float* values = new float[1024 * 1024 * 4];
            float* ptr    = values;
            for (UINT i = 0; i < 1024 * 1024; i++)
            {
                ptr[0] = RandomVariance(0.0f, 1.0f);
                ptr[1] = RandomVariance(0.0f, 1.0f);
                ptr[2] = RandomVariance(0.0f, 1.0f);
                ptr[3] = RandomVariance(0.0f, 1.0f);
                ptr += 4;
            }
            MemTextureDataBlock* pDataBlock = new MemTextureDataBlock(reinterpret_cast<char*>(values));
            // Explicitly cast away const during data copy
            const_cast<Texture*>(m_pRandomTexture)->CopyData(pDataBlock);
            // Once done, auto-enqueue a barrier for start of next frame so it's usable
            Barrier bufferTransition = Barrier::Transition(
                m_pRandomTexture->GetResource(), ResourceState::CopyDest, ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
            GetDevice()->ExecuteResourceTransitionImmediate(1, &bufferTransition);
            delete[] values;
        }
    }

    void ParticleSystem::Update(double deltaTime)
    {
        m_FrameTime = (float)deltaTime;

        for (int i = 0; i < m_Emitters.size(); ++i)
        {
            Emitter& emitter = m_Emitters[i];

            m_EmitterLightingCenter[i] = Vec4(m_Position + emitter.SpawnOffset, 1.0f);
            if (emitter.ParticlesPerSecond > 0.0f)
            {
                emitter.NumToEmit = 0;
                emitter.Accumulation += emitter.ParticlesPerSecond * m_FrameTime;
                if (emitter.Accumulation > 1.0f)
                {
                    float integerPart = 0.0f;
                    float fraction    = modf(emitter.Accumulation, &integerPart);

                    emitter.NumToEmit    = (int)integerPart;
                    emitter.Accumulation = fraction;
                }
            }
        }
    }
}
