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

#include "core/contentloader.h"
#include "render/particle.h"

namespace cauldron
{
    /**
     * @struct ParticleLoadParams
     *
     * Parameter struct for both LoadAsync and LoadMultipleAsync versions of the <c><i>ParticleLoader</i></c>.
     *
     * @ingroup CauldronLoaders
     */
    struct ParticleLoadParams
    {
        std::vector<ParticleSpawnerDesc> LoadData = {}; ///< A list of <c><i>ParticleSpawnerDesc</i></c>s to load.
    };

    /**
     * @class ParticleLoader
     *
     * Particle loader class. Handles asynchronous particle system loading.
     *
     * @ingroup CauldronLoaders
     */
    class ParticleLoader : public ContentLoader
    {
    public:

        /**
         * @brief   Constructor with default behavior.
         */
        ParticleLoader() = default;

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~ParticleLoader() = default;

        /**
         * @brief   Loads a single instance of <c><i>ParticleSpawnerDesc</i></c> asynchronously.
         */
        virtual void LoadAsync(void* pLoadParams) override;

        /**
         * @brief   Loads multiple instances of <c><i>ParticleSpawnerDesc</i></c> asynchronously.
         */
        virtual void LoadMultipleAsync(void* pLoadParams) override;

    private:
        static void LoadParticleContent(void* pParam);
        static void TextureLoadCompleted(const std::vector<const Texture*>& textureList, void* pParam);
    };

}  // namespace cauldron
