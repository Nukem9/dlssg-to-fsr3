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

#include "core/contentmanager.h"
#include "core/components/cameracomponent.h"
#include "core/components/lightcomponent.h"
#include "misc/helpers.h"
#include "render/animation.h"
#include "render/mesh.h"

#include "json/json.h"
using json = nlohmann::ordered_json;

namespace cauldron
{
    struct AnimationComponentData;

    /**
     * @struct GLTFDataRep
     *
     * GLTF data representation that persists throughout the loading process to facilitate data sharing across
     * multiple asynchronous loading jobs.
     *
     * @ingroup CauldronLoaders
     */
    struct GLTFDataRep
    {
        json*                                   pGLTFJsonData;                  ///< The json GLTF data instance.
        std::vector<std::vector<char>>          GLTFBufferData;                 ///< The GLTF buffer data entries.
        std::wstring                            GLTFFilePath;                   ///< The GLTF file path.
        std::wstring                            GLTFFileName;                   ///< The GLTF file name.

        std::vector<LightComponentData>         LightData;                      ///< Loaded <c><i>LightComponentData</i></c>.
        std::vector<CameraComponentData>        CameraData;                     ///< Loaded <c><i>CameraComponentData</i></c>.

        // To synchronize data loading and initialization
        bool                                    BuffersLoaded = false;          ///< Buffer load status. True if buffer loading is completed.
        bool                                    TexturesLoaded = false;         ///< Texture load status. True if buffer loading is completed.

        std::mutex                              CriticalSection;                ///< Mutex for syncing structure data changes.
        std::condition_variable                 BufferCV;                       ///< Condition variable for syncing structure buffer data changes.
        std::condition_variable                 TextureCV;                      ///< Condition variable for syncing structure texture data changes.

        // Content block being built up as we are loading various things
        ContentBlock*                           pLoadedContentRep = nullptr;    ///< The <c><i>ContentBlock</i></c> built by the loading processes.

        std::chrono::nanoseconds                loadStartTime;                  ///< The time content loading started (used to track loading times)

        ~GLTFDataRep()
        {
            delete pGLTFJsonData;
        }
    };

    class UploadContext;

    /**
     * @class GLTFLoader
     *
     * This is the GLTF loader class, which handles asynchronous GLTF scene loading.
     *
     * @ingroup CauldronLoaders
     */
    class GLTFLoader : public ContentLoader
    {
    public:

        /**
         * @brief   Constructor with default behavior.
         */
        GLTFLoader() = default;

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~GLTFLoader() = default;

        /**
         * @brief   Loads a single GLTF scene file asynchronously.
         */
        virtual void LoadAsync(void* loadParams) override;

        /**
         * @brief   Functionality not yet supported, and will assert.
         */
        virtual void LoadMultipleAsync(void* loadParams) override;

    private:
        NO_COPY(GLTFLoader)
        NO_MOVE(GLTFLoader)

        // Handler to load all glTF related assets and content
        void LoadGLTFContent(void* pParam);
        static void LoadGLTFTexturesCompleted(const std::vector<const Texture*>& textureList, void* pCallbackParams);

        static void InitSkinningData(const Mesh* pMesh, AnimationComponentData* pComponentData);

        static void LoadGLTFBuffer(void* pParam);
        static void LoadGLTFBuffersCompleted(void* pParam);
        static void LoadGLTFMesh(void* pParam);
        static void LoadGLTFAnimation(void* pParam);
        static void LoadGLTFSkin(void* pParam);
        static void GLTFAllBufferAssetLoadsCompleted(void* pParam);

        // Parameter struct for Buffer-related loads
        struct GLTFBufferLoadParams
        {
            GLTFDataRep* pGLTFData = nullptr;
            uint32_t     BufferIndex = 0;
            std::wstring BufferName = L"";
            UploadContext* pUploadCtx = nullptr;
        };

        static const json* LoadVertexBuffer(const json& attributes, const char* attributeName, const json& accessors, const json& bufferViews, const json& buffers, const GLTFBufferLoadParams& params, VertexBufferInformation& info, bool forceConversionToFloat);
        static void LoadIndexBuffer(const json& primitive, const json& accessors, const json& bufferViews, const json& buffers, const GLTFBufferLoadParams& params, IndexBufferInformation& info);
        static void LoadAnimInterpolant(AnimInterpolants& animInterpolant, const json& gltfData, int32_t interpAccessorID, const GLTFBufferLoadParams* pBufferLoadParams);
        static void LoadAnimInterpolants(AnimChannel* pAnimChannel, AnimChannel::ComponentSampler samplerType, int32_t samplerIndex, const GLTFBufferLoadParams* pBufferLoadParams);
        static void GetBufferDetails(int accessor, AnimInterpolants* pAccessor, const GLTFBufferLoadParams* pBufferLoadParams);
        static void BuildBLAS(std::vector<Mesh*> meshes);

        void PostGLTFContentLoadCompleted(void* pParam);
    };

} // namespace cauldron
