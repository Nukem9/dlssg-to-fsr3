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
#include "render/gpuresource.h"

namespace cauldron
{
    /// Per platform/API implementation of <c><i>BufferAddressInfo</i></c>
    ///
    /// @ingroup CauldronRender
    struct BufferAddressInfoInternal;

    /// A structure representing buffer addressing information used to bind buffer resources
    /// to the GPU. Private implementations can be found under each API/Platform folder
    ///
    /// @ingroup CauldronRender
    struct BufferAddressInfo
    {
        uint64_t addressInfoSize[4];            // Memory placeholder
        const BufferAddressInfoInternal* GetImpl() const { return (const BufferAddressInfoInternal*)addressInfoSize; }
    };

    /// Per platform/API implementation of <c><i>BufferCopyDesc</i></c>
    ///
    /// @ingroup CauldronRender
    struct BufferCopyDescInternal;

    /// A structure representing buffer copy job description used to copy buffer resources
    /// on the GPU. Private implementations can be found under each API/Platform folder
    ///
    /// @ingroup CauldronRender
    struct BufferCopyDesc
    {
        BufferCopyDesc() = default;
        BufferCopyDesc(const GPUResource* pSrc, const GPUResource* pDst);
        
        uint64_t bufferCopyDescMem[6];            ///< Memory placeholder
        BufferCopyDescInternal* GetImpl() { return (BufferCopyDescInternal*)bufferCopyDescMem; }
        const BufferCopyDescInternal* GetImpl() const { return (const BufferCopyDescInternal*)bufferCopyDescMem; }
    };

    /// An enumeration for various types of buffers which can be created through <c><i>FidelityFX Cauldron Framework</i></c>
    ///
    /// @ingroup CauldronRender
    enum class BufferType
    {
        Vertex,                 ///< Resource represents a vertex buffer.
        Index,                  ///< Resource represents an index buffer.
        Constant,               ///< Resource represents a constant buffer.
        AccelerationStructure,  ///< Resource represents an acceleration structure.
        Data,                   ///< Resource represents a generic data buffer.
    };

    /// A buffer description structure used to create buffer resources. Provides convenience functions for creating
    /// buffer descriptions of all supported types of buffers.
    ///
    /// @ingroup CauldronRender
    struct BufferDesc
    {
        BufferType      Type = BufferType::Vertex;      ///< The <c><i>BufferType</i></c> this resource will be.
        ResourceFlags   Flags = ResourceFlags::None;    ///< Needed <c><i>ResourceFlags</i></c>.
        uint32_t        Size = 0;                       ///< The size of the buffer in bytes.
        uint32_t        Alignment = 0;                  ///< The required alignment of the buffer.
        union
        {
            uint32_t Stride = 0;                        ///< The stride of the buffer, or
            ResourceFormat Format;                      ///< The format of the buffer (when using as an index buffer).
        };
        std::wstring    Name = L"";                     ///< The name to assign to the created <c><i>Buffer</i></c> resource.

        /// Convenience creation function for vertex buffer descriptions.
        ///
        /// @ingroup CauldronRender
        static inline BufferDesc Vertex(const wchar_t* name, uint32_t size, uint32_t stride, uint32_t alignment = 0, ResourceFlags flags = ResourceFlags::None)
        {
            BufferDesc desc;
            desc.Type = BufferType::Vertex;
            desc.Flags = flags;
            desc.Size = size;
            desc.Alignment = alignment;
            desc.Stride = stride;
            desc.Name = name;

            return desc;
        }

        /// Convenience creation function for index buffer descriptions.
        ///
        /// @ingroup CauldronRender
        static inline BufferDesc Index(const wchar_t* name, uint32_t size, ResourceFormat format, uint32_t alignment = 0, ResourceFlags flags = ResourceFlags::None)
        {
            BufferDesc desc;
            desc.Type = BufferType::Index;
            desc.Flags = flags;
            desc.Size = size;
            desc.Alignment = alignment;
            desc.Format = format;
            desc.Name = name;

            return desc;
        }

        /// Convenience creation function for constant buffer descriptions.
        ///
        /// @ingroup CauldronRender
        static inline BufferDesc Constant(const wchar_t* name, uint32_t size, uint32_t stride, uint32_t alignment = 0, ResourceFlags flags = ResourceFlags::None)
        {
            BufferDesc desc;
            desc.Type = BufferType::Constant;
            desc.Flags = flags;
            desc.Size = size;
            desc.Alignment = alignment;
            desc.Stride = stride;
            desc.Name = name;

            return desc;
        }

        /// Convenience creation function for data buffer descriptions.
        ///
        /// @ingroup CauldronRender
        static inline BufferDesc Data(const wchar_t* name, uint32_t size, uint32_t stride, uint32_t alignment = 0, ResourceFlags flags = ResourceFlags::None)
        {
            BufferDesc desc;
            desc.Type = BufferType::Data;
            desc.Flags = flags;
            desc.Size = size;
            desc.Alignment = alignment;
            desc.Stride = stride;
            desc.Name = name;

            return desc;
        }

        /// Convenience creation function for acceleration structure descriptions.
        ///
        /// @ingroup CauldronRender
        static inline BufferDesc AccelerationStructure(const wchar_t* name, uint32_t size, uint32_t stride, uint32_t alignment = 0, ResourceFlags flags = ResourceFlags::None)
        {
            BufferDesc desc;
            desc.Type      = BufferType::AccelerationStructure;
            desc.Flags     = flags;
            desc.Size      = size;
            desc.Alignment = alignment;
            desc.Stride    = stride;
            desc.Name      = name;

            return desc;
        }

    };

    class UploadContext;

    /**
     * @class Buffer
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> api/platform-agnostic representation of a buffer gpu resource.
     *
     * @ingroup CauldronRender
     */
    class Buffer
    {
    public:
        /**
         * @brief   Function signature for buffer ResizeFunction (when needed).
         */
        typedef void (*ResizeFunction)(BufferDesc&, uint32_t, uint32_t, uint32_t, uint32_t);

        /**
         * @brief   Destruction.
         */
        virtual ~Buffer();

        /**
         * @brief   Buffer instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static Buffer* CreateBufferResource(const BufferDesc* pDesc, ResourceState initialState, ResizeFunction fn = nullptr, void* customOwner = nullptr);

        /**
         * @brief   Gets a constant view on the buffer's <c><i>BufferDesc</i></c> description.
         */
        const BufferDesc& GetDesc() const { return m_BufferDesc; }

        /**
         * @brief   Gets a constant pointer to the buffer's underlaying <c><i>GPUResource</i></c>.
         */
        const GPUResource* GetResource() const { return m_pResource; }

        /**
         * @brief   Gets a modifiable pointer to the buffer's underlaying <c><i>GPUResource</i></c>.
         */
        GPUResource* GetResource() { return m_pResource; }

        /**
        * @brief   Copy callback used when loading buffer data. Implemented internally per api/platform.
        */
        virtual void CopyData(const void* pData, size_t size) = 0;
        virtual void CopyData(const void* pData, size_t size, UploadContext* pUploadCtx, ResourceState postCopyState) = 0;

        /**
        * @brief   Gets the buffer's <c><i>BufferAddressInfo</i></c> for resource binding. Implemented internally per api/platform.
        */
        virtual BufferAddressInfo GetAddressInfo() const = 0;

        /**
        * @brief   Callback function executed on a buffer with resizing support to handle resourc recreation when size changes are needed.
        */
        void OnRenderingResolutionResize(uint32_t outputWidth, uint32_t outputHeight, uint32_t renderingWidth, uint32_t renderingHeight);

    private:
        // No copy, No move
        NO_COPY(Buffer)
        NO_MOVE(Buffer)

    protected:
        Buffer(const BufferDesc* pDesc, ResizeFunction fn);
        virtual void Recreate() = 0;

        Buffer() = delete;

        BufferDesc                   m_BufferDesc   = {};
        GPUResource*                 m_pResource    = nullptr;
        ResizeFunction               m_ResizeFn     = nullptr;
    };

} // namespace cauldron
