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

#include "misc/helpers.h"
#include "misc/math.h"
#include "render/buffer.h"
#include "render/dynamicbufferpool.h"
#include "render/material.h"
#include "render/shaderbuilder.h"

#include <array>

#include <cstring>
#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING    // To avoid receiving deprecation error since we are using C++11 only
#include <experimental/filesystem>
#include <vector>

class GPUParticleRenderModule;
class TranslucencyRenderModule;
namespace cauldron
{
    /// Structure representing the needed information for the creation of particle emitters.
    ///
    /// @ingroup CauldronRender
    struct EmitterDesc
    {
        enum EmitterFlags
        {
            EF_Streaks   = 1 << 0,  ///< Streak the particles based on velocity
            EF_Reactive  = 1 << 1   ///< Particles also write to the reactive mask
        };

        std::wstring    EmitterName = L"";                      ///< The emitter's name.
        Vec3            SpawnOffset = Vec3(0, 0, 0);            ///< The emitter's spawning offset (optional).
        Vec3            SpawnOffsetVariance = Vec3(0, 0, 0);    ///< The emitter's spawning offset variance (vector) (optional).
        Vec3            SpawnVelocity = Vec3(0, 0, 0);          ///< The emitter's spawning velocity (optional).
        float           SpawnVelocityVariance = 0.f;            ///< The emitter's spawning velocity variance (magnitude) (optional)
        uint32_t        ParticlesPerSecond = 0;                 ///< The emitter's spawning rate in particles spawned per second.
        float           Lifespan = 0.f;                         ///< The emitter's particle's lifetime (how long it stays active).
        float           SpawnSize = 0.f;                        ///< The emitter's particle's size at creation.
        float           KillSize = 0.f;                         ///< The emitter's particle's size at death.
        float           Mass = 0.f;                             ///< The emitter's particle's mass.
        int32_t         AtlasIndex = -1;                        ///< The emitter's particle's backing sub-resource in the particle spawner texture atlas.
        int             Flags = 0;                              ///< The emitter's <c><i>EmitterFlags</i></c>.
    };

    /// Structure representing the needed information for the creation of particle spawners.
    ///
    /// @ingroup CauldronRender
    struct ParticleSpawnerDesc
    {
        std::wstring                        Name = L"";                 ///< The particle spawner's name.
        std::experimental::filesystem::path AtlasPath = {};             ///< The particle spawner's texture atlas (textures backing all emitters in spawner).
        Vec3                                Position = Vec3(0, 0, 0);   ///< The particle spawner's position.
        std::vector<EmitterDesc>            Emitters = {};              ///< The list of <c><i>EmitterDesc</i></c> for all emitters in this spawner.
        bool                                Sort = true;                ///< Whether or not to sort the particles spawned by this particle system.
    };

    /// The maximum number of supported GPU particles
    ///
    /// @ingroup CauldronRender
    static const int g_maxParticles = 400 * 1024;

    /**
     * @class ParticleSystem
     *
     * Represents a run-time particle spawning system as defined by a <c><i>ParticleSpawnerDesc</i></c>. 
     *
     * @ingroup CauldronRender
     */
    class ParticleSystem
    {
    public:

        /**
         * @brief   Construction from passed in <c><i>ParticleSpawnerDesc</i></c>.
         */
        ParticleSystem(const ParticleSpawnerDesc& particleSpawnerDesc);

        /**
         * @brief   Destruction with default behavior.
         */
        ~ParticleSystem() = default;

        /**
         * @brief   Particle system update. Called once per frame.
         */
        void Update(double deltaTime);

        /**
         * @brief   Returns the particle system's name.
         */
        std::wstring& GetName() { return m_Name; }
        const std::wstring& GetName() const { return m_Name; }

        /**
         * @brief   Returns the particle system's position.
         */
        Vec3& GetPosition() { return m_Position; }
        const Vec3& GetPosition() const { return m_Position; }

    private:
        NO_COPY(ParticleSystem);
        NO_MOVE(ParticleSystem);

        ParticleSystem() = delete;

        struct Emitter
        {
            std::wstring EmitterName           = L"";
            Vec3         SpawnOffset           = Vec3(0, 0, 0);
            Vec3         SpawnOffsetVariance   = Vec3(0, 0, 0);
            Vec3         SpawnVelocity         = Vec3(0, 0, 0);
            float        SpawnVelocityVariance = 0.f;
            uint32_t     ParticlesPerSecond    = 0;
            float        Lifespan              = 0.f;
            float        SpawnSize             = 0.f;
            float        KillSize              = 0.f;
            float        Mass                  = 0.f;
            int32_t      AtlasIndex            = -1;
            int          Flags                 = 0;
            uint32_t     NumToEmit             = 0;
            float        Accumulation          = 0.f;
        };

        std::wstring         m_Name           = L"";
        Vec3                 m_Position       = Vec3(0, 0, 0);
        std::vector<Emitter> m_Emitters       = {};
        std::atomic_bool     m_RenderReady    = false;

        const Buffer*  m_pParticleBufferA                  = nullptr;
        const Buffer*  m_pParticleBufferB                  = nullptr;
        const Buffer*  m_pPackedViewSpaceParticlePositions = nullptr;
        const Buffer*  m_pMaxRadiusBuffer                  = nullptr;
        const Buffer*  m_pDeadListBuffer                   = nullptr;
        const Buffer*  m_pAliveIndexBuffer                 = nullptr;
        const Buffer*  m_pAliveDistanceBuffer              = nullptr;
        const Buffer*  m_pAliveCountBuffer                 = nullptr;
        const Buffer*  m_pRenderingBuffer                  = nullptr;
        const Buffer*  m_pIndirectArgsBuffer               = nullptr;
        const Buffer*  m_pIndexBuffer                      = nullptr;
        const Texture* m_pRandomTexture                    = nullptr;
        const Texture* m_pAtlas                            = nullptr;

        ResourceState m_ReadBufferStates;
        ResourceState m_WriteBufferStates;
        ResourceState m_StridedBufferStates;

        bool  m_Sort                    = true;
        float m_AlphaThreshold          = 0.97f;

        Vec4  m_StartColor[10]            = {};
        Vec4  m_EndColor[10]              = {};
        Vec4  m_EmitterLightingCenter[10] = {};
        float m_FrameTime                 = 0.0f;

        friend class ParticleLoader;
        friend class GPUParticleRenderModule;
        friend class TranslucencyRenderModule;
    };

}  // namespace cauldron
