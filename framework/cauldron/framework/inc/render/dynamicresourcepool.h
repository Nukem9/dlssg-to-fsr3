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

#include "render/renderdefines.h"

#include <map>
#include <mutex>
#include <vector>

namespace cauldron
{
    class Buffer;
    struct BufferDesc;
    class GPUResource;
    class Texture;
    struct TextureDesc;

    /**
     * @class Buffer
     *
     * The dynamic resource pool is the allocation construct used to back all resource creation in <c><i>FidelityFX Cauldron Framework</i></c>.
     *
     * @ingroup CauldronRender
     */
    class DynamicResourcePool
    {
    public:

        /**
         * @brief   Construction.
         */
        DynamicResourcePool();

        /**
         * @brief   Destruction.
         */
        virtual ~DynamicResourcePool();

        /**
         * @brief   Callback when the rendering resolution changed. Will call all resource resize function callbacks
         *          and recreate/rebind all resources automatically in the background.
         */
        void OnResolutionChanged(const ResolutionInfo& resInfo);

        /**
         * @brief   Destroys a GPU resource.
         */
        void DestroyResource(const GPUResource* pResource);

        /**
         * @brief   Fetches a <c><i>Texture</i></c> resource by name. Will assert when called on the main thread while
         *          the Framework is running as it uses a map construct for lookup.
         */
        const Texture* GetTexture(const wchar_t* name);

        /**
         * @brief   Fetches a <c><i>Buffer</i></c> resource by name. Will assert when called on the main thread while
         *          the Framework is running as it uses a map construct for lookup.
         */
        const Buffer* GetBuffer(const wchar_t* name);

        typedef void (*TextureResizeFunction)(TextureDesc&, uint32_t, uint32_t, uint32_t, uint32_t);
        typedef void (*BufferResizeFunction)(BufferDesc&, uint32_t, uint32_t, uint32_t, uint32_t);

        /**
         * @brief   Creates a <c><i>Texture</i></c> resource. Will create the resource is the requested state.
         */
        const Texture* CreateTexture(const TextureDesc* pDesc, ResourceState initialState, TextureResizeFunction fn = nullptr);

        /**
         * @brief   Creates a <c><i>Texture</i></c> resource for rendering. Will automatically add AllowRenderTarget/AllowDepthTarget 
         *          resource flag based on the resource type.
         */
        const Texture* CreateRenderTexture(const TextureDesc* pDesc, TextureResizeFunction fn = nullptr);

        /**
         * @brief   Creates a <c><i>Buffer</i></c> resource. Will create the resource is the requested state.
         */
        const Buffer* CreateBuffer(const BufferDesc* pDesc, ResourceState initialState, BufferResizeFunction fn = nullptr);

    private:
        std::vector<std::pair<std::wstring, Texture*>>  m_Textures;
        std::vector<std::pair<std::wstring, Buffer*>>   m_Buffers;
        std::vector<Texture*>                           m_ResizableTextures; 
        std::vector<Buffer*>                            m_ResizableBuffers;
        std::mutex m_CriticalSection;
    };

} // namespace cauldron
