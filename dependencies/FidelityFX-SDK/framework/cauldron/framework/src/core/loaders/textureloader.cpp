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

#include "core/loaders/textureloader.h"
#include "core/contentmanager.h"
#include "core/taskmanager.h"
#include "core/framework.h"
#include "misc/assert.h"
#include "misc/fileio.h"
#include "render/device.h"
#include "render/gpuresource.h"

using namespace std::experimental;

namespace cauldron
{
    void TextureLoader::LoadAsync(void* pLoadParams)
    {
        // Validate there is at least one param instance
        TextureLoadParams* pParams = reinterpret_cast<TextureLoadParams*>(pLoadParams);
        if (pParams->LoadInfo.size() != 1)
        {
            CauldronError(L"Calling TextureLoader::LoadAsync with num LoadInfo != 1. Aborting read.");
            return;
        }

        // Copy the load parameters to use when loading textures
        TextureLoadParams* pTexLoadData = new TextureLoadParams();
        *pTexLoadData = *pParams;

        // Create a task completion callback to call in order to add textures to the content
        // manager once fully initialized and call the requester' callback
        TaskCompletionCallback* pLoadCompleteCallback = new TaskCompletionCallback(Task(&TextureLoader::AsyncLoadCompleteCallback, pTexLoadData), 1);

        // Enqueue the task to load content
        Task loadingTask(&TextureLoader::LoadTextureContent, &pTexLoadData->LoadInfo[0], pLoadCompleteCallback);
        GetTaskManager()->AddTask(loadingTask);
    }

    void TextureLoader::LoadMultipleAsync(void* pLoadParams)
    {
        TextureLoadParams* pParams = reinterpret_cast<TextureLoadParams*>(pLoadParams);

        // Copy the load parameters to use when loading textures
        TextureLoadParams* pTexLoadData = new TextureLoadParams();
        *pTexLoadData = *pParams;

        // Create a task completion callback to call in order to call the requester' callback
        TaskCompletionCallback* pLoadCompleteCallback = new TaskCompletionCallback(Task(&TextureLoader::AsyncLoadCompleteCallback, pTexLoadData), static_cast<uint32_t>(pTexLoadData->LoadInfo.size()));

        // Enqueue a task per texture to load the content
        std::queue<Task> taskList;
        for (size_t i = 0; i < pTexLoadData->LoadInfo.size(); ++i)
        {
            taskList.push(Task(&TextureLoader::LoadTextureContent, &pTexLoadData->LoadInfo[i], pLoadCompleteCallback));
        }
        GetTaskManager()->AddTaskList(taskList);
    }

    // Handler to load texture resources
    void TextureLoader::LoadTextureContent(void* pParam)
    {
        TextureLoadInfo& loadInfo = *reinterpret_cast<TextureLoadInfo*>(pParam);

        bool fileExists = filesystem::exists(loadInfo.TextureFile);
        CauldronAssert(ASSERT_ERROR, fileExists, L"Could not find texture file %ls. Please run ClearMediaCache.bat followed by UpdateMedia.bat to sync to latest media.", loadInfo.TextureFile.c_str());

        if (fileExists)
        {
            TextureDesc texDesc = {};

            // Figure out how to load this texture (whether it's a DDS or other)
            bool ddsFile = loadInfo.TextureFile.extension() == L".dds" || loadInfo.TextureFile.extension() == L".DDS";
            TextureDataBlock* pTextureData;

            if (ddsFile)
                pTextureData = new DDSTextureDataBlock();
            else
                pTextureData = new WICTextureDataBlock();

            bool loaded = pTextureData->LoadTextureData(loadInfo.TextureFile, loadInfo.AlphaThreshold, texDesc);

            CauldronAssert(ASSERT_ERROR, loaded, L"Could not load texture %ls (TextureDataBlock::LoadTextureData() failed)", loadInfo.TextureFile.c_str());
            if (loaded)
            {
                // We will use the relative path as the name of the asset since it's guaranteed to be unique
                texDesc.Name = loadInfo.TextureFile.c_str();

                // Pass along resource flags
                texDesc.Flags = static_cast<ResourceFlags>(loadInfo.Flags);

                // If SRGB was requested, apply format conversion
                if (loadInfo.SRGB)
                    texDesc.Format = ToGamma(texDesc.Format);

                Texture* pNewTexture = Texture::CreateContentTexture(&texDesc);
                CauldronAssert(ASSERT_ERROR, pNewTexture != nullptr, L"Could not create the texture %ls", texDesc.Name.c_str());
                if (pNewTexture != nullptr)
                {
                    pNewTexture->CopyData(pTextureData);

                    // Start managing the texture at this point
                    bool emplaced = GetContentManager()->StartManagingContent(texDesc.Name, pNewTexture);

                    // If it was emplaced, need to queue it up for a transition during the first graphics cmd list
                    if (emplaced)
                    {
                        // Now that the resource is ready, queue the resource change on the graphics queue for the next time it executes
                        Barrier textureTransition = Barrier::Transition(pNewTexture->GetResource(), ResourceState::CopyDest, ResourceState::PixelShaderResource | ResourceState::NonPixelShaderResource);
                        GetDevice()->ExecuteResourceTransitionImmediate(1, &textureTransition);
                    }

                    // if it wasn't emplaced, it's a duplicate and we can just delete it (callback will be called with previously loaded asset)
                    else
                    {
                        delete pNewTexture;
                    }
                }
            }

            delete pTextureData;
        }
    }

    // Completion handler to call when texture loads are all complete
    void TextureLoader::AsyncLoadCompleteCallback(void* pParam)
    {
        TextureLoadParams* pLoadParams = reinterpret_cast<TextureLoadParams*>(pParam);

        // If there was no callback, skip this work
        if (pLoadParams->LoadCompleteCallback)
        {
            // Build up a list of texture pointers to pass to the texture load callback function
            std::vector<const Texture*>   loadedTextures;
            std::vector<TextureLoadInfo>::iterator iter = pLoadParams->LoadInfo.begin();
            ContentManager* pContentManager = GetContentManager();
            while (iter != pLoadParams->LoadInfo.end())
            {
                loadedTextures.push_back(pContentManager->GetTexture(iter->TextureFile.c_str()));
                ++iter;
            }

            // Lastly, call the callback
            pLoadParams->LoadCompleteCallback(loadedTextures, pLoadParams->AdditionalParams);
        }

        // Delete the load parameters we allocated for the duration of the loading job
        delete pLoadParams;
    }


    // WICTextureDataBlock Implementation (Uses stb image loader)
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

    WICTextureDataBlock::~WICTextureDataBlock()
    {
        if (m_pData)
            free(m_pData);
    }

    float WICTextureDataBlock::GetAlphaCoverage(uint32_t width, uint32_t height, float scale, uint32_t alphaThreshold) const
    {
        double value = 0.0;

        uint32_t* pImgData = reinterpret_cast<uint32_t*>(m_pData);

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                uint8_t* pPixel = reinterpret_cast<uint8_t*>(pImgData++);
                uint32_t alpha = static_cast<uint32_t>(scale * (float)pPixel[3]);
                if (alpha > 255)
                    alpha = 255;
                if (alpha <= alphaThreshold)
                    continue;

                value += alpha;
            }
        }

        return static_cast<float>(value / (height * width * 255));
    }

    void WICTextureDataBlock::ScaleAlpha(uint32_t width, uint32_t height, float scale)
    {
        uint32_t* pImgData = reinterpret_cast<uint32_t*>(m_pData);

        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                uint8_t* pPixel = reinterpret_cast<uint8_t*>(pImgData++);

                int32_t alpha = (int)(scale * (float)pPixel[3]);
                if (alpha > 255)
                    alpha = 255;

                pPixel[3] = alpha;
            }
        }
    }

    void WICTextureDataBlock::MipImage(uint32_t width, uint32_t height)
    {
        //compute mip so next call gets the lower mip
        int32_t offsetsX[] = { 0,1,0,1 };
        int32_t offsetsY[] = { 0,0,1,1 };

        uint32_t* pImgData = reinterpret_cast<uint32_t*>(m_pData);

#define GetByte(color, component) (((color) >> (8 * (component))) & 0xff)
#define GetColor(ptr, x,y) (ptr[(x)+(y)*width])
#define SetColor(ptr, x,y, col) ptr[(x)+(y)*width/2]=col;

        for (uint32_t y = 0; y < height; y += 2)
        {
            for (uint32_t x = 0; x < width; x += 2)
            {
                uint32_t ccc = 0;
                for (uint32_t c = 0; c < 4; ++c)
                {
                    uint32_t cc = 0;
                    for (uint32_t i = 0; i < 4; ++i)
                        cc += GetByte(GetColor(pImgData, x + offsetsX[i], y + offsetsY[i]), 3 - c);

                    ccc = (ccc << 8) | (cc / 4);
                }
                SetColor(pImgData, x / 2, y / 2, ccc);
            }
        }


        // For cutouts we need to scale the alpha channel to match the coverage of the top MIP map
        // otherwise cutouts seem to get thinner when smaller mips are used
        // Credits: http://www.ludicon.com/castano/blog/articles/computing-alpha-mipmaps/
        if (m_AlphaTestCoverage < 1.0)
        {
            float ini = 0;
            float fin = 10;
            float mid;
            float alphaPercentage;
            int iter = 0;
            for (; iter < 50; iter++)
            {
                mid = (ini + fin) / 2;
                alphaPercentage = GetAlphaCoverage(width / 2, height / 2, mid, (int)(m_AlphaThreshold * 255));

                if (fabs(alphaPercentage - m_AlphaTestCoverage) < .001)
                    break;

                if (alphaPercentage > m_AlphaTestCoverage)
                    fin = mid;
                if (alphaPercentage < m_AlphaTestCoverage)
                    ini = mid;
            }
            ScaleAlpha(width / 2, height / 2, mid);
        }

    }

    bool WICTextureDataBlock::LoadTextureData(filesystem::path& textureFile, float alphaThreshold, TextureDesc& texDesc)
    {
        std::string fileName = textureFile.u8string();

        int32_t channels;
        m_pData = reinterpret_cast<char*>(stbi_load(fileName.c_str(), reinterpret_cast<int32_t*>(&texDesc.Width), reinterpret_cast<int32_t*>(&texDesc.Height), &channels, STBI_rgb_alpha));

        if (!m_pData)
            return false;

        // Compute number of mips
        uint32_t mipWidth = texDesc.Width;
        uint32_t mipHeight = texDesc.Height;
        texDesc.MipLevels = 0;
        for (;;)
        {
            ++texDesc.MipLevels;
            if (mipWidth > 1)
                mipWidth >>= 1;
            if (mipHeight > 1)
                mipHeight >>= 1;
            if (mipWidth == 1 && mipHeight == 1)
                break;
        }

        // Fill in remaining texture information
        texDesc.DepthOrArraySize = 1;
        texDesc.Format = ResourceFormat::RGBA8_UNORM;
        texDesc.Dimension = TextureDimension::Texture2D;

        // If there is an alpha threshold, compute the alpha test coverage of the top mip
        // Mip generation will try to match this value so objects don't get thinner as they use lower mips
        m_AlphaThreshold = alphaThreshold;
        if (m_AlphaThreshold < 1.0f)
            m_AlphaTestCoverage = GetAlphaCoverage(texDesc.Width, texDesc.Height, 1.0f, (uint32_t)(255 * m_AlphaThreshold));
        else
            m_AlphaTestCoverage = 1.0f;

        return true;
    }

    void WICTextureDataBlock::CopyTextureData(void* pDest, uint32_t stride, uint32_t bytesWidth, uint32_t height, uint32_t readOffset)
    {
        for (uint32_t y = 0; y < height; ++y)
            memcpy((char*)pDest + y * stride, m_pData + y * bytesWidth, bytesWidth);

        // Generate the next mip in the chain to read
        MipImage(bytesWidth / 4, height);
    }

    // Needed for DDS loading
#include <dxgiformat.h>

    struct DDS_PIXELFORMAT
    {
        UINT32 size;
        UINT32 flags;
        UINT32 fourCC;
        UINT32 bitCount;
        UINT32 bitMaskR;
        UINT32 bitMaskG;
        UINT32 bitMaskB;
        UINT32 bitMaskA;
    };

    struct DDS_HEADER
    {

        UINT32          dwSize;
        UINT32          dwHeaderFlags;
        UINT32          dwHeight;
        UINT32          dwWidth;
        UINT32          dwPitchOrLinearSize;
        UINT32          dwDepth;
        UINT32          dwMipMapCount;
        UINT32          dwReserved1[11];
        DDS_PIXELFORMAT ddspf;
        UINT32          dwSurfaceFlags;
        UINT32          dwCubemapFlags;
        UINT32          dwCaps3;
        UINT32          dwCaps4;
        UINT32          dwReserved2;
    };

    ResourceFormat DXGIToResourceFormat(DXGI_FORMAT format)
    {
        switch (format)
        {
        case DXGI_FORMAT_UNKNOWN:
            return ResourceFormat::Unknown;
        case DXGI_FORMAT_R16_FLOAT:
            return ResourceFormat::R16_FLOAT;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return ResourceFormat::RGBA8_UNORM;
        case DXGI_FORMAT_R8G8B8A8_SNORM:
            return ResourceFormat::RGBA8_SNORM;
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return ResourceFormat::RGBA8_SRGB;
        case DXGI_FORMAT_R10G10B10A2_UNORM:
            return ResourceFormat::RGB10A2_UNORM;
        case DXGI_FORMAT_R16G16_FLOAT:
            return ResourceFormat::RG16_FLOAT;
        case DXGI_FORMAT_R32_FLOAT:
            return ResourceFormat::R32_FLOAT;
        case DXGI_FORMAT_R16G16B16A16_UNORM:
            return ResourceFormat::RGBA16_UNORM;
        case DXGI_FORMAT_R16G16B16A16_SNORM:
            return ResourceFormat::RGBA16_SNORM;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
            return ResourceFormat::RGBA16_FLOAT;
        case DXGI_FORMAT_R32G32_FLOAT:
            return ResourceFormat::RG32_FLOAT;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return ResourceFormat::RGBA32_FLOAT;
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
            return ResourceFormat::RGBA32_TYPELESS;
        case DXGI_FORMAT_D16_UNORM:
            return ResourceFormat::D16_UNORM;
        case DXGI_FORMAT_D32_FLOAT:
            return ResourceFormat::D32_FLOAT;
        case DXGI_FORMAT_BC1_UNORM:
            return ResourceFormat::BC1_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return ResourceFormat::BC1_SRGB;
        case DXGI_FORMAT_BC2_UNORM:
            return ResourceFormat::BC2_UNORM;
        case DXGI_FORMAT_BC2_UNORM_SRGB:
            return ResourceFormat::BC2_SRGB;
        case DXGI_FORMAT_BC3_UNORM:
            return ResourceFormat::BC3_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            return ResourceFormat::BC3_SRGB;
        case DXGI_FORMAT_BC4_UNORM:
            return ResourceFormat::BC4_UNORM;
        case DXGI_FORMAT_BC4_SNORM:
            return ResourceFormat::BC4_SNORM;
        case DXGI_FORMAT_BC5_UNORM:
            return ResourceFormat::BC5_UNORM;
        case DXGI_FORMAT_BC5_SNORM:
            return ResourceFormat::BC5_SNORM;
        case DXGI_FORMAT_BC6H_UF16:
            return ResourceFormat::BC6_UNSIGNED;
        case DXGI_FORMAT_BC6H_SF16:
            return ResourceFormat::BC6_SIGNED;
        case DXGI_FORMAT_BC7_UNORM:
            return ResourceFormat::BC7_UNORM;
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return ResourceFormat::BC7_SRGB;
        default:
            CauldronCritical(L"Unsupported format detected in DXGIToResourceFormat(). Please file an issue for additional format support.");
            return ResourceFormat::Unknown;
        }
    }

    ResourceFormat GetResourceFormat(DDS_PIXELFORMAT pixelFmt)
    {
        if (pixelFmt.flags & 0x00000004)   //DDPF_FOURCC
        {
            // Check for D3DFORMAT enums being set here
            switch (pixelFmt.fourCC)
            {
            case '1TXD':         return ResourceFormat::BC1_UNORM;
            case '3TXD':         return ResourceFormat::BC2_UNORM;
            case '5TXD':         return ResourceFormat::BC3_UNORM;
            case 'U4CB':         return ResourceFormat::BC4_UNORM;
            case 'A4CB':         return ResourceFormat::BC4_SNORM;
            case '2ITA':         return ResourceFormat::BC5_UNORM;
            case 'S5CB':         return ResourceFormat::BC5_SNORM;
            case 36:             return ResourceFormat::RGBA16_UNORM;
            case 110:            return ResourceFormat::RGBA16_SNORM;
            case 111:            return ResourceFormat::R16_FLOAT;
            case 112:            return ResourceFormat::RG16_FLOAT;
            case 113:            return ResourceFormat::RGBA16_FLOAT;
            case 114:            return ResourceFormat::R32_FLOAT;
            case 115:            return ResourceFormat::RG32_FLOAT;
            case 116:            return ResourceFormat::RGBA32_FLOAT;
            default:
                CauldronError(L"Unsupported DDS_PIXELFORMAT requested. Please file an issue for additional format support.");
                return ResourceFormat::Unknown;
            }
        }
        else
        {
            switch (pixelFmt.bitMaskR)
            {
            case 0xff:        return ResourceFormat::RGBA8_UNORM;
            case 0x3ff:       return ResourceFormat::RGB10A2_UNORM;

            case 0:           //return DXGI_FORMAT_A8_UNORM;
            case 0x00ff0000:  return ResourceFormat::RGBA8_UNORM; // Temporarily modify to read atlas.dds
            case 0xffff:      //return DXGI_FORMAT_R16G16_UNORM;
            case 0x7c00:      //return DXGI_FORMAT_B5G5R5A1_UNORM;
            case 0xf800:      //return DXGI_FORMAT_B5G6R5_UNORM;
            default:          //return DXGI_FORMAT_UNKNOWN;
                CauldronError(L"Unsupported resource format requested. Please file an issue for additional format support.");
                return ResourceFormat::Unknown;
            };
        }
    }

    // DDSTextureDataBlock Implementation (Uses DDS loader)
    DDSTextureDataBlock::~DDSTextureDataBlock()
    {
        delete m_pData;
    }

    bool DDSTextureDataBlock::LoadTextureData(filesystem::path& textureFile, float alphaThreshold, TextureDesc& texDesc)
    {
        typedef enum RESOURCE_DIMENSION
        {
            RESOURCE_DIMENSION_UNKNOWN = 0,
            RESOURCE_DIMENSION_BUFFER = 1,
            RESOURCE_DIMENSION_TEXTURE1D = 2,
            RESOURCE_DIMENSION_TEXTURE2D = 3,
            RESOURCE_DIMENSION_TEXTURE3D = 4
        } RESOURCE_DIMENSION;

        typedef struct
        {
            DXGI_FORMAT         dxgiFormat;
            RESOURCE_DIMENSION  resourceDimension;
            UINT32              miscFlag;
            UINT32              arraySize;
            UINT32              reserved;
        } DDS_HEADER_DXT10;

        // Get the file size
        int64_t fileSize = GetFileSize(textureFile.c_str());
        int64_t rawTextureSize = fileSize;
        if (fileSize == -1)
        {
            CauldronError(L"Could not get file size of %ls", textureFile.c_str());
            return false;
        }

        // read the header
        constexpr int32_t c_HEADER_SIZE = 4 + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10);
        char headerData[c_HEADER_SIZE];
        uint32_t bytesRead = 0;

        int64_t sizeRead = ReadFilePartial(textureFile.c_str(), headerData, c_HEADER_SIZE);
        if (sizeRead != c_HEADER_SIZE)
        {
            CauldronError(L"Error reading texture header data for file %ls", textureFile.c_str());
            return false;
        }

        char* pByteData = headerData;
        uint32_t magicNumber = *reinterpret_cast<uint32_t*>(pByteData);
        CauldronAssert(ASSERT_ERROR, magicNumber == ' SDD', L"DDSLoader could not find DDS indicator in header info");
        if (magicNumber != ' SDD')   // "DDS "
            return false;

        pByteData += 4;
        rawTextureSize -= 4;

        DDS_HEADER* pHeader = reinterpret_cast<DDS_HEADER*>(pByteData);
        pByteData += sizeof(DDS_HEADER);
        rawTextureSize -= sizeof(DDS_HEADER);

        texDesc.Width = pHeader->dwWidth;
        texDesc.Height = pHeader->dwHeight;
        texDesc.DepthOrArraySize = pHeader->dwDepth ? pHeader->dwDepth : 1;
        texDesc.MipLevels = pHeader->dwMipMapCount ? pHeader->dwMipMapCount : 1;
        texDesc.Dimension = TextureDimension::Texture2D;

        if (pHeader->ddspf.fourCC == '01XD')
        {
            DDS_HEADER_DXT10* pHeader10 = reinterpret_cast<DDS_HEADER_DXT10*>((char*)pHeader + sizeof(DDS_HEADER));
            rawTextureSize -= sizeof(DDS_HEADER_DXT10);

            // Surface format
            texDesc.Format = DXGIToResourceFormat(pHeader10->dxgiFormat);

            switch (pHeader10->resourceDimension)
            {
            case 2:
                texDesc.Dimension = TextureDimension::Texture1D;
                texDesc.Height = 1;
                break;
            case 3:
                // Is this a cube map
                if (pHeader10->miscFlag == 4)
                {
                    texDesc.Dimension = TextureDimension::CubeMap;
                    texDesc.DepthOrArraySize = pHeader10->arraySize * 6;
                }
                else
                {
                    texDesc.Dimension = TextureDimension::Texture2D;
                }
                break;
            case 4:
                texDesc.Dimension = TextureDimension::Texture3D;
                break;
            default:
                CauldronCritical(L"Unexpected Resource Dimension Encountered!");
                break;
            }            
        }
        else
        {
            if (pHeader->dwCubemapFlags == 0xfe00)
            {
                texDesc.DepthOrArraySize = 6;
                texDesc.Dimension = TextureDimension::CubeMap;
            }
            else
                texDesc.DepthOrArraySize = 1;

            texDesc.Format = GetResourceFormat(pHeader->ddspf);
        }

        // Read in the data representing the texture (remainder of the file after the header)
        m_pData = new char[rawTextureSize];
        sizeRead = ReadFilePartial(textureFile.c_str(), m_pData, rawTextureSize, fileSize - rawTextureSize);
        if (sizeRead != rawTextureSize)
        {
            delete[](m_pData);
            CauldronError(L"Error reading texture data for file %ls", textureFile.c_str());
            return false;
        }

        return true;
    }

    void DDSTextureDataBlock::CopyTextureData(void* pDest, uint32_t stride, uint32_t bytesWidth, uint32_t height, uint32_t readOffset)
    {
        for (uint32_t y = 0; y < height; ++y)
            memcpy((char*)pDest + y * stride, m_pData + readOffset + y * bytesWidth, bytesWidth);
    }

    // MemTextureDataBlock Implementation (Loads data to texture from memory)
    MemTextureDataBlock::~MemTextureDataBlock()
    {
        m_pData = nullptr;  // We don't own this data
    }

    bool MemTextureDataBlock::LoadTextureData(filesystem::path& textureFile, float alphaThreshold, TextureDesc& texDesc)
    {
        CauldronError(L"MemTextureDataBlock does not support calls to LoadTextureData.");
        return false;
    }

    void MemTextureDataBlock::CopyTextureData(void* pDest, uint32_t stride, uint32_t bytesWidth, uint32_t height, uint32_t readOffset)
    {
        for (uint32_t y = 0; y < height; ++y)
            memcpy((char*)pDest + y * stride, m_pData + readOffset + y * bytesWidth, bytesWidth);
    }

} // namespace cauldron
