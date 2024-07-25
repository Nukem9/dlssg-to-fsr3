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
#include "render/renderdefines.h"

#include <vector>

namespace cauldron
{
    /// A structure representing a texture description
    ///
    /// @ingroup CauldronRender
    struct TextureDesc
    {
        ResourceFormat      Format           = ResourceFormat::Unknown;         ///< The <c><i>ResourceFormat</i></c> for the texture.
        ResourceFlags       Flags            = ResourceFlags::None;             ///< The <c><i>ResourceFlags</i></c> for the texture.
        uint32_t            Width            = 0;                               ///< The texture's width.
        uint32_t            Height           = 0;                               ///< The texture's height.
        TextureDimension    Dimension        = TextureDimension::Unknown;       ///< The texture's dimension (1D/2D/3D/etc.).
        uint32_t            DepthOrArraySize = 0;                               ///< The texture's depth or number of arrays
        uint32_t            MipLevels        = 0;                               ///< The texture's mip map count.
        std::wstring        Name             = L"";                             ///< The tesxture's name.

        /// A convenience function to create a texture description for 1D textures.
        ///
        static inline TextureDesc Tex1D(const wchar_t* name, ResourceFormat format, uint32_t width, uint32_t arraySize = 1, uint32_t mipLevels = 0, ResourceFlags flags = ResourceFlags::None)
        {
            TextureDesc desc;
            desc.Format = format;
            desc.Flags = flags;
            desc.Width = width;
            desc.Height = 1;
            desc.Dimension = TextureDimension::Texture1D;
            desc.DepthOrArraySize = arraySize;
            desc.MipLevels = mipLevels;
            desc.Name = name;

            return desc;
        }
        
        /// A convenience function to create a texture description for 2D textures.
        ///
        static inline TextureDesc Tex2D(const wchar_t* name, ResourceFormat format, uint32_t width, uint32_t height, uint32_t arraySize = 1, uint32_t mipLevels = 0, ResourceFlags flags = ResourceFlags::None) 
        {
            TextureDesc desc;
            desc.Format = format;
            desc.Flags = flags;
            desc.Width = width;
            desc.Height = height;
            desc.Dimension = TextureDimension::Texture2D;
            desc.DepthOrArraySize = arraySize;
            desc.MipLevels = mipLevels;
            desc.Name = name;

            return desc;
        }

        /// A convenience function to create a texture description for 3D textures.
        ///
        static inline TextureDesc Tex3D(const wchar_t* name, ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t mipLevels = 0, ResourceFlags flags = ResourceFlags::None)
        {
            TextureDesc desc;
            desc.Format = format;
            desc.Flags = flags;
            desc.Width = width;
            desc.Height = height;
            desc.Dimension = TextureDimension::Texture3D;
            desc.DepthOrArraySize = depth;
            desc.MipLevels = mipLevels;
            desc.Name = name;

            return desc;
        }

        /// A convenience function to create a texture description for cube textures.
        ///
        static inline TextureDesc TexCube(const wchar_t* name, ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth = 1, uint32_t mipLevels = 0, ResourceFlags flags = ResourceFlags::None)
        {
            TextureDesc desc;
            desc.Format = format;
            desc.Flags = flags;
            desc.Width = width;
            desc.Height = height;
            desc.Dimension = TextureDimension::CubeMap;
            desc.DepthOrArraySize = 6 * depth;
            desc.MipLevels = mipLevels;
            desc.Name = name;

            return desc;
        }
    };

    class GPUResource;
    class TextureDataBlock;
   
    /// Per platform/API implementation of <c><i>Texture</i></c>
    ///
    /// @ingroup CauldronRender
    class TextureInternal;

    /**
     * @class Texture
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of a texture resource.
     *
     * @ingroup CauldronRender
     */
    class Texture
    {
    public:
        typedef void (*ResizeFunction)(TextureDesc&, uint32_t, uint32_t, uint32_t, uint32_t);
        
        /**
         * @brief   Texture instance creation function (generic). Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Texture* CreateTexture(const TextureDesc* pDesc, ResourceState initialState, ResizeFunction fn = nullptr);
        
        /**
         * @brief   Texture instance creation function for swap chains. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Texture* CreateSwapchainTexture(const TextureDesc* pDesc, GPUResource* pResource);

        /**
         * @brief   Texture instance creation function for loaded content. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Texture* CreateContentTexture(const TextureDesc* pDesc);

        /**
         * @brief   Destruction.
         */
        virtual ~Texture();

        /**
         * @brief   Returns the texture's format.
         */
        ResourceFormat     GetFormat() const { return m_TextureDesc.Format; }

        /**
         * @brief   Returns the texture's description.
         */
        const TextureDesc& GetDesc() const { return m_TextureDesc; }

        /**
         * @brief   Returns the texture's backing <c><i>GPUResource</i></c>.
         */
        GPUResource*       GetResource() { return m_pResource; }
        const GPUResource* GetResource() const { return m_pResource; }

        /**
         * @brief   Copies data from a texture data block into the texture resource. Used when loading content from file/memory.
         */
        void CopyData(TextureDataBlock* pTextureDataBlock);

        /**
         * @brief   Returns true if this resource is a swap chain. Used to isolate swapchain surfaces from 
         *          non-swap chain (specialization class exists per platform to overload this).
         */
        virtual bool IsSwapChain() const { return false; }

        /**
         * @brief   Callback invoked by OnResize event.
         */
        void OnRenderingResolutionResize(uint32_t outputWidth, uint32_t outputHeight, uint32_t renderingWidth, uint32_t renderingHeight);

        /**
         * @brief   Gets the internal implementation for api/platform parameter accessors.
         */
        TextureInternal* GetImpl() { return m_pImpl; }
        const TextureInternal* GetImpl() const { return m_pImpl; }

    private:
        // No copy, No move
        NO_COPY(Texture)
        NO_MOVE(Texture)

    protected:
        Texture(const TextureDesc* pDesc, ResourceState initialState, ResizeFunction fn);
        Texture(const TextureDesc* pDesc, GPUResource* pResource);
        Texture() = delete;

        void Recreate();

        TextureDesc         m_TextureDesc = {};
        GPUResource*        m_pResource   = nullptr;
        TextureInternal*    m_pImpl       = nullptr;
        ResizeFunction      m_ResizeFn    = nullptr;
    };

    /**
     * @class SwapChainRenderTarget
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of a swap chain render target resource.
     *
     * @ingroup CauldronRender
     */
    class SwapChainRenderTarget : public Texture
    {
    public:

        /**
         * @brief   Destruction.
         */
        virtual ~SwapChainRenderTarget();

        /**
         * @brief   Returns true to indicate the resource is a swapchain.
         */
        virtual bool IsSwapChain() const override { return true; }

        /**
         * @brief   Returns the number of back buffers held by the resource.
         */
        uint32_t GetBackBufferCount() const { return static_cast<uint32_t>(m_TextureResources.size()); }

        /**
        * @brief    Returns the specified back buffer backing GPUResource.
        */
        GPUResource* GetResource(uint32_t index) const { return m_TextureResources[index]->GetResource(); }

        /**
         * @brief   Returns the current back buffer.
         */
        GPUResource* GetCurrentResource() const { return m_TextureResources[m_CurrentBackBuffer]->GetResource(); }

    private:
        friend class SwapChain;
        friend class SwapChainInternal;
        SwapChainRenderTarget(const TextureDesc* pDesc, std::vector<GPUResource*>& resources);

        void SetCurrentBackBufferIndex(uint32_t index);
        void ClearResources();
        void Update(const TextureDesc* pDesc, std::vector<GPUResource*>& resources);

    private:
        uint32_t                m_CurrentBackBuffer = 0;
        Texture*                m_pCurrentTexture = nullptr;
        std::vector<Texture*>   m_AdditionalTextures;
        std::vector<Texture*>   m_TextureResources;        
    };

    /// Per platform/API implementation of <c><i>TextureCopyDesc</i></c>
    ///
    /// @ingroup CauldronRender
    struct TextureCopyDescInternal;

    /// A structure representing texture copy job description used to copy texture resources
    /// on the GPU. Private implementations can be found under each API/Platform folder
    ///
    /// @ingroup CauldronRender
    struct TextureCopyDesc
    {
        TextureCopyDesc() = default;
        TextureCopyDesc(const GPUResource* pSrc, const GPUResource* pDst, unsigned int arrayIndex = 0, unsigned int mipLevel = 0);

        uint64_t textureCopyDescMem[20];            ///< Memory placeholder
        TextureCopyDescInternal* GetImpl() { return (TextureCopyDescInternal*)textureCopyDescMem; }
        const TextureCopyDescInternal* GetImpl() const { return (const TextureCopyDescInternal*)textureCopyDescMem; }
    };

} // namespace cauldron
