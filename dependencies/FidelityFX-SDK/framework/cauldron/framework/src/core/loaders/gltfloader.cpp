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

#include "core/loaders/gltfloader.h"
#include "core/entity.h"
#include "core/framework.h"
#include "core/taskmanager.h"
#include "core/components/cameracomponent.h"
#include "core/components/lightcomponent.h"
#include "core/components/meshcomponent.h"
#include "core/components/animationcomponent.h"
#include "misc/assert.h"
#include "misc/fileio.h"
#include "misc/helpers.h"
#include "misc/math.h"
#include "render/animation.h"
#include "render/device.h"
#include "render/material.h"
#include "render/mesh.h"
#include "render/rtresources.h"
#include "render/sampler.h"

#include "render/commandlist.h"

#include <string>

using namespace std::experimental;
using namespace math;

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Helpers

    struct BufferViewInfo
    {
        int    BufferID = -1;
        size_t Length = 0;
        size_t Offset = 0;
        size_t Stride = 0;
    };

    BufferViewInfo GetBufferInfo(const json& accessor, const json& bufferViews)
    {
        int viewID = accessor["bufferView"];
        auto& view = bufferViews[viewID];

        BufferViewInfo info;
        info.BufferID = view["buffer"];
        info.Length = view["byteLength"].get<size_t>();

        info.Offset   = 0;
        auto offsetIt = view.find("byteOffset");
        if (offsetIt != view.end())
            info.Offset = offsetIt->get<size_t>();

        info.Stride = 0;
        auto strideIt = view.find("byteStride");
        if (strideIt != view.end())
            info.Stride = strideIt->get<size_t>();
        return info;
    }

    constexpr int g_GLTFComponentType_Byte          = 5120;
    constexpr int g_GLTFComponentType_UnsignedByte  = 5121;
    constexpr int g_GLTFComponentType_Short         = 5122;
    constexpr int g_GLTFComponentType_UnsignedShort = 5123;
    constexpr int g_GLTFComponentType_Int           = 5124;
    constexpr int g_GLTFComponentType_UnsignedInt   = 5125;
    constexpr int g_GLTFComponentType_Float         = 5126;

    constexpr char* g_LightExtensionName = "KHR_lights_punctual";

    float ReadFloat(const json& object, const char* name, float defaultValue)
    {
        auto it = object.find(name);
        if (it == object.end())
            return defaultValue;

        return it->get<float>();
    }

    Vector3 ReadVec3(const json& object, const char* name, Vector3 defaultValue)
    {
        auto it = object.find(name);
        if (it == object.end())
            return defaultValue;

        return Vector3(
            it->at(0).get<float>(),
            it->at(1).get<float>(),
            it->at(2).get<float>());
    }

    Quat ReadQuat(const json& object, const char* name, Quat defaultValue)
    {
        auto it = object.find(name);
        if (it == object.end())
            return defaultValue;

        return Quat(
            it->at(0).get<float>(),
            it->at(1).get<float>(),
            it->at(2).get<float>(),
            it->at(3).get<float>());
    }

    Matrix4 ReadMatrix(const json& object, const char* name, Matrix4 defaultValue)
    {
        auto it = object.find(name);
        if (it == object.end())
            return defaultValue;

        Vector4 col0(
            it->at(0).get<float>(),
            it->at(1).get<float>(),
            it->at(2).get<float>(),
            it->at(3).get<float>());

        Vector4 col1(
            it->at(4).get<float>(),
            it->at(5).get<float>(),
            it->at(6).get<float>(),
            it->at(7).get<float>());

        Vector4 col2(
            it->at(8).get<float>(),
            it->at(9).get<float>(),
            it->at(10).get<float>(),
            it->at(11).get<float>());

        Vector4 col3(
            it->at(12).get<float>(),
            it->at(13).get<float>(),
            it->at(14).get<float>(),
            it->at(15).get<float>());

        return Matrix4(
            col0,
            col1,
            col2,
            col3);
    }

    void VisitNodeRecursive(int index, const json& nodes, const Matrix4 parentTransform, std::vector<cauldron::EntityDataBlock*>& entityDataBlocks, std::vector<bool>& visited)
    {
        // update transform
        Matrix4 transform = entityDataBlocks[index]->pEntity->GetTransform();
        transform = parentTransform * transform;
        entityDataBlocks[index]->pEntity->SetTransform(transform);

        // notify visited
        cauldron::CauldronAssert(cauldron::ASSERT_CRITICAL, !visited[index], L"Node %d has already been visited.", index);
        visited[index] = true;

        // recursive call
        const json& node = nodes[index];
        if (node.find("children") != node.end())
        {
            const json& children = node["children"];
            for (size_t i = 0; i < children.size(); ++i)
                VisitNodeRecursive(children[i].get<int>(), nodes, transform, entityDataBlocks, visited);
        }
    }

    ResourceFormat ResourceDataFormat(AttributeFormat attributeFormat, int32_t resourceFormatID)
    {
        if (attributeFormat == AttributeFormat::Scalar)
        {
            switch (resourceFormatID)
            {
            case g_GLTFComponentType_Byte:          return ResourceFormat::R8_SINT;     // Byte
            case g_GLTFComponentType_UnsignedByte:  return ResourceFormat::R8_UINT;     // Unsigned Byte
            case g_GLTFComponentType_Short:         return ResourceFormat::R16_SINT;    // Short
            case g_GLTFComponentType_UnsignedShort: return ResourceFormat::R16_UINT;    // Unsigned Short
            case g_GLTFComponentType_Int:           return ResourceFormat::R32_SINT;    // Signed int
            case g_GLTFComponentType_UnsignedInt:   return ResourceFormat::R32_UINT;    // Unsigned int
            case g_GLTFComponentType_Float:         return ResourceFormat::R32_FLOAT;   // Float
            }
        }
        else if (attributeFormat == AttributeFormat::Vec2)
        {
            switch (resourceFormatID)
            {
            case g_GLTFComponentType_Byte:          return ResourceFormat::RG8_SINT;     // Byte
            case g_GLTFComponentType_UnsignedByte:  return ResourceFormat::RG8_UINT;     // Unsigned Byte
            case g_GLTFComponentType_Short:         return ResourceFormat::RG16_SINT;    // Short
            case g_GLTFComponentType_UnsignedShort: return ResourceFormat::RG16_UINT;    // Unsigned Short
            case g_GLTFComponentType_Int:           return ResourceFormat::RG32_SINT;    // Signed int
            case g_GLTFComponentType_UnsignedInt:   return ResourceFormat::RG32_UINT;    // Unsigned int
            case g_GLTFComponentType_Float:         return ResourceFormat::RG32_FLOAT;   // Float
            }
        }
        else if (attributeFormat == AttributeFormat::Vec3)
        {
            switch (resourceFormatID)
            {
            case g_GLTFComponentType_Byte:                                               // Byte
            case g_GLTFComponentType_UnsignedByte:                                       // Unsigned Byte
            case g_GLTFComponentType_Short:                                              // Short
            case g_GLTFComponentType_UnsignedShort: return ResourceFormat::Unknown;      // Unsigned Short
            case g_GLTFComponentType_Int:           return ResourceFormat::RGB32_SINT;   // Signed int
            case g_GLTFComponentType_UnsignedInt:   return ResourceFormat::RGB32_UINT;   // Unsigned int
            case g_GLTFComponentType_Float:         return ResourceFormat::RGB32_FLOAT;  // Float
            }
        }
        else if (attributeFormat == AttributeFormat::Vec4)
        {
            switch (resourceFormatID)
            {
            case g_GLTFComponentType_Byte:          return ResourceFormat::RGBA8_SINT;   // Byte
            case g_GLTFComponentType_UnsignedByte:  return ResourceFormat::RGBA8_UINT;   // Unsigned Byte
            case g_GLTFComponentType_Short:         return ResourceFormat::RGBA16_SINT;  // Short
            case g_GLTFComponentType_UnsignedShort: return ResourceFormat::RGBA16_UINT;  // Unsigned Short
            case g_GLTFComponentType_Int:           return ResourceFormat::RGBA32_SINT;  // Signed int
            case g_GLTFComponentType_UnsignedInt:   return ResourceFormat::RGBA32_UINT;  // Unsigned int
            case g_GLTFComponentType_Float:         return ResourceFormat::RGBA32_FLOAT; // Float
            }
        }

        return ResourceFormat::Unknown;
    }

    uint32_t ResourceDataStride(int32_t resourceFormatID)
    {
        switch (resourceFormatID)
        {
        case g_GLTFComponentType_Byte:          return 1;   // Byte
        case g_GLTFComponentType_UnsignedByte:  return 1;   // Unsigned Byte
        case g_GLTFComponentType_Short:         return 2;  // Short
        case g_GLTFComponentType_UnsignedShort: return 2;  // Unsigned Short
        case g_GLTFComponentType_Int:           return 4;  // Signed int
        case g_GLTFComponentType_UnsignedInt:   return 4;  // Unsigned int
        case g_GLTFComponentType_Float:         return 4; // Float
        }

        CauldronCritical(L"Invalid GLtf componentType for accessor");
        return 0;
    }

    uint32_t ResourceFormatDimension(const std::string& str)
    {
        if (str == "SCALAR")
            return 1;
        else if (str == "VEC2")
            return 2;
        else if (str == "VEC3")
            return 3;
        else if (str == "VEC4")
            return 4;
        else if (str == "MAT4")
            return 4 * 4;
        else
            return -1;
    }

    AttributeFormat ResourceFormatType(const std::string& str)
    {
        if (str == "SCALAR")
            return AttributeFormat::Scalar;
        else if (str == "VEC2")
            return AttributeFormat::Vec2;
        else if (str == "VEC3")
            return AttributeFormat::Vec3;
        else if (str == "VEC4")
            return AttributeFormat::Vec4;
        else
            return AttributeFormat::Unknown;
    }

    //////////////////////////////////////////////////////////////////////////
    // GLTFLoader

    void GLTFLoader::LoadAsync(void* pLoadParams)
    {
        // Allocate path information for duration we need it
        filesystem::path* pPathInfo = new filesystem::path(*reinterpret_cast<filesystem::path*>(pLoadParams));

        // Enqueue the task to load content
        Task loadingTask(std::bind(&GLTFLoader::LoadGLTFContent, this, std::placeholders::_1), pPathInfo);
        GetTaskManager()->AddTask(loadingTask);
    }

    void GLTFLoader::LoadMultipleAsync(void* pLoadParams)
    {
        CauldronError(L"Multiple Async load of GLTF content not yet supported. Please file an issue to have it implemented.");
    }

    // Handler to load all glTF related assets and content
    void GLTFLoader::LoadGLTFContent(void* pParam)
    {
        filesystem::path* pFileToLoad = reinterpret_cast<filesystem::path*>(pParam);

        bool fileExists = filesystem::exists(*pFileToLoad);
        CauldronAssert(ASSERT_ERROR, fileExists, L"Could not load GLTF file %ls", pFileToLoad->c_str());

        if (fileExists)
        {
            // Grab the path without the filename for resource loading
            filesystem::path filePath = pFileToLoad->parent_path();
            std::wstring filePathString = filePath.c_str();
            filePathString.append(L"\\");

            // Create glTF data representation that will be passed around for loading
            GLTFDataRep* glTFDataRep = new GLTFDataRep();

            glTFDataRep->loadStartTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());

            // And the content block that will hold references to all the managed content in this block (textures, buffers, entities)
            glTFDataRep->pLoadedContentRep = new ContentBlock();

            // Add the name of the file all content was loaded from (will be used to uniquely identify internal assets like meshes and animations)
            glTFDataRep->GLTFFilePath = filePathString;
            glTFDataRep->GLTFFileName = pFileToLoad->c_str();

            // Start by loading the glTF file and reading in all the json data
            glTFDataRep->pGLTFJsonData = new json();
            CauldronAssert(ASSERT_CRITICAL, ParseJsonFile(pFileToLoad->c_str(), *glTFDataRep->pGLTFJsonData), L"Could not parse JSON file %ls", pFileToLoad->c_str());

            // Grab the handle to the GLTF data
            const json& glTFData = *glTFDataRep->pGLTFJsonData;
            bool hasImages = glTFData.find("images") != glTFData.end();
            bool hasBuffers = glTFData.find("buffers") != glTFData.end();
            bool hasMaterials = glTFData.find("materials") != glTFData.end();
            bool hasSamplers = glTFData.find("samplers") != glTFData.end();
            bool hasTextureRedirects = glTFData.find("textures") != glTFData.end();

            std::vector<bool>   textureSRGBMap;
            if (hasImages)
                textureSRGBMap.resize(glTFData["images"].size(), false);

            // Load available sampler descriptors so they can be added to material information when needed
            std::vector<SamplerDesc>    textureSamplers;
            if (hasSamplers)
            {
                // Read samplers that are present into the file (sampler desc defaults to clamped linear
                SamplerDesc samplerDesc = {};

                // Will need config settings to initialize the device
                const CauldronConfig* pConfig = GetConfig();

                const json& samplers = glTFData["samplers"];
                for (size_t i = 0; i < samplers.size(); ++i)
                {
                    // ALl values come from glTF 2.0 spec here: https://www.khronos.org/registry/glTF/specs/2.0/glTF-2.0.html#reference-sampler
                    const json& sampler = samplers[i];

                    // Set min/mag/mip filter accordingly
                    int32_t magFilter(9729), minFilter(9729);
                    if (sampler.find("magFilter") != sampler.end())
                        magFilter = sampler["magFilter"];
                    if (sampler.find("minFilter") != sampler.end())
                        minFilter = sampler["minFilter"];

                    // Mag filter can be NEAREST (9728) or LINEAR (9729)

                    // Min filter represents:
                    //  NEAREST                 (9728)
                    //  LINEAR                  (9729)
                    //  NEAREST_MIPMAP_NEAREST  (9984)
                    //  LINEAR_MIPMAP_NEAREST   (9985)
                    //  NEAREST_MIPMAP_LINEAR   (9986)
                    //  LINEAR_MIPMAP_LINEAR    (9987)

                    // If this isn't explicitly nearest, use linear filtering
                    // unless we are overriding samplers, in which case we'll use anisotropic
                    if (magFilter != 9728 && minFilter != 9728)
                    {
                        if (pConfig->OverrideSceneSamplers)
                        {
                            samplerDesc.Filter = FilterFunc::Anisotropic;
                        }
                        else
                        {
                            samplerDesc.Filter = FilterFunc::MinMagMipLinear;
                        }
                    }
                    else
                    {
                        if (magFilter == 9728)
                        {
                            switch (minFilter)
                            {
                            case 9728:                                                                      // NEAREST
                            case 9984: samplerDesc.Filter = FilterFunc::MinMagMipPoint; break;              // NEAREST_MIPMAP_NEAREST
                            case 9729:                                                                      // LINEAR
                            case 9987: samplerDesc.Filter = FilterFunc::MinLinearMagPointMipLinear; break;  // LINEAR_MIPMAP_LINEAR
                            case 9985: samplerDesc.Filter = FilterFunc::MinLinearMagMipPoint; break;        // LINEAR_MIPMAP_NEAREST
                            case 9986: samplerDesc.Filter = FilterFunc::MinMagPointMipLinear; break;        // NEAREST_MIPMAP_LINEAR
                            default: CauldronError(L"Unsupported sampler filter combination detected"); break;
                            }
                        }
                        else
                        {
                            switch (minFilter)
                            {
                            case 9728:                                                                      // NEAREST
                            case 9984: samplerDesc.Filter = FilterFunc::MinPointMagLinearMipPoint; break;   // NEAREST_MIPMAP_NEAREST
                            case 9729:                                                                      // LINEAR
                            case 9987: samplerDesc.Filter = FilterFunc::MinMagMipLinear; break;             // LINEAR_MIPMAP_LINEAR
                            case 9985: samplerDesc.Filter = FilterFunc::MinMagLinearMipPoint; break;        // LINEAR_MIPMAP_NEAREST
                            case 9986: samplerDesc.Filter = FilterFunc::MinPointMagMipLinear; break;        // NEAREST_MIPMAP_LINEAR
                            default: CauldronError(L"Unsupported sampler filter combination detected"); break;
                            }
                        }
                    }

                    // Set proper wrap mode
                    int32_t wrapU(10497), wrapV(10497);
                    if (sampler.find("wrapS") != sampler.end())
                        wrapU = sampler["wrapS"];
                    if (sampler.find("wrapT") != sampler.end())
                        wrapV = sampler["wrapT"];
                    switch (wrapU)
                    {
                    case 33071: samplerDesc.AddressU = AddressMode::Clamp; break;   // CLAMP_TO_EDGE
                    case 33648: samplerDesc.AddressU = AddressMode::Mirror; break;  // MIRRORED_REPEAT
                    case 10497: samplerDesc.AddressU = AddressMode::Wrap; break;    // REPEAT
                    default: CauldronError(L"Unsupported glTF sampler wrap mode detected"); break;
                    }

                    switch (wrapV)
                    {
                    case 33071: samplerDesc.AddressV = AddressMode::Clamp; break;   // CLAMP_TO_EDGE
                    case 33648: samplerDesc.AddressV = AddressMode::Mirror; break;  // MIRRORED_REPEAT
                    case 10497: samplerDesc.AddressV = AddressMode::Wrap; break;    // REPEAT
                    default: CauldronError(L"Unsupported glTF sampler wrap mode detected"); break;
                    }

                    textureSamplers.push_back(samplerDesc);
                }
            }

            // Read in material information and build up partially completed material info (textures will be fixed up later)
            // material information required ahead of texture and buffer being done loading to properly setup textures and meshes
            if (hasMaterials)
            {
                // Init the material data in the content block
                const json& materials = glTFData["materials"];
                glTFDataRep->pLoadedContentRep->Materials.resize(materials.size());

                // Load texture redirects
                json empty;
                const json& textures = hasTextureRedirects ? glTFData["textures"] : empty;

                // Go through all the materials and pre-init them with everything except the valid texture pointers (which we will fixup after
                // textures are loaded). This will also create an SRGB format map for texture loading so we don't have to re-scan the materials.
                for (size_t i = 0; i < materials.size(); ++i)
                {
                    const json& materialEntry = materials[i];
                    glTFDataRep->pLoadedContentRep->Materials[i] = new Material();
                    glTFDataRep->pLoadedContentRep->Materials[i]->InitFromGLTFData(materialEntry, textures, textureSRGBMap, textureSamplers);
                }
            }
            else
            {
                // Default material
                glTFDataRep->pLoadedContentRep->Materials.resize(1);
                glTFDataRep->pLoadedContentRep->Materials[0] = new Material();
                glTFDataRep->pLoadedContentRep->Materials[0]->SetDoubleSided(true);
            }

            // Load all textures, and when done, continue initialization of resources
            // Note that we are loading "images" and not "textures" in case multiple textures point to the same image source (which is against spec, but I don't trust it)
            if (hasImages)
            {
                const json& images = glTFData["images"];
                std::vector<TextureLoadInfo> texLoadInfo;
                for (size_t i = 0; i < images.size(); ++i)
                {
                    const std::string& uriName = images[i]["uri"];
                    filesystem::path filePath = filePathString + StringToWString(uriName);

                    // Push the load info
                    texLoadInfo.push_back(TextureLoadInfo(filePath, textureSRGBMap[i]));
                }

                // Load all the textures in the background
                GetContentManager()->LoadTextures(texLoadInfo, &GLTFLoader::LoadGLTFTexturesCompleted, glTFDataRep);
            }

            // Now, schedule asynchronous loads of all the buffer data while we setup
            // other structures (if we have any)
            if (hasBuffers)
            {
                // Reserve the right number of entries
                const json& buffers = glTFData["buffers"];
                glTFDataRep->GLTFBufferData.resize(buffers.size());

                // Load them asynchronously
                TaskCompletionCallback* pCompletionCallback = new TaskCompletionCallback(Task(&GLTFLoader::LoadGLTFBuffersCompleted, glTFDataRep), static_cast<uint32_t>(buffers.size()));

                std::queue<Task>   taskList;
                for (size_t i = 0; i < buffers.size(); ++i)
                {
                    GLTFBufferLoadParams* pBufferLoadParams = new GLTFBufferLoadParams();
                    pBufferLoadParams->pGLTFData = glTFDataRep;
                    pBufferLoadParams->BufferIndex = (uint32_t)i;
                    const std::string& uriName = buffers[i]["uri"];
                    pBufferLoadParams->BufferName = filePathString + StringToWString(uriName);

                    // Verify the file exists, otherwise we don't want to load
                    // We can get around textures not being there, but not whole buffer info
                    filesystem::path uriFile(pBufferLoadParams->BufferName);
                    CauldronAssert(ASSERT_ERROR, filesystem::exists(uriFile), L"Buffer file %ls does not exist", pBufferLoadParams->BufferName.c_str());

                    // Push the task
                    taskList.push(Task(&GLTFLoader::LoadGLTFBuffer, pBufferLoadParams, pCompletionCallback));
                }

                // If all buffers were found, trigger the loading
                GetTaskManager()->AddTaskList(taskList);
            }

            // Load lights
            auto extensionsUsedIt = glTFData.find("extensionsUsed");
            if (extensionsUsedIt != glTFData.end())
            {
                bool hasLights = false;
                for (size_t i = 0; i < extensionsUsedIt->size(); ++i)
                {
                    if ((*extensionsUsedIt)[i].get<std::string>() == g_LightExtensionName)
                    {
                        hasLights = true;
                        break;
                    }
                }

                if (hasLights)
                {
                    const json& lights = glTFData["extensions"][g_LightExtensionName]["lights"];
                    glTFDataRep->LightData.reserve(lights.size());
                    for (const json& light : lights)
                    {
                        LightComponentData data;
                        data.Color = ReadVec3(light, "color", data.Color);
                        data.Intensity = ReadFloat(light, "intensity", data.Intensity);
                        data.Range = ReadFloat(light, "range", data.Range);

                        std::string lightTypeName = light["type"].get<std::string>();
                        if (lightTypeName == "directional")
                        {
                            data.Type = LightType::Directional;
                        }
                        else if (lightTypeName == "spot")
                        {
                            data.Type = LightType::Spot;

                            const json& spotInfo = light["spot"];
                            data.SpotInnerConeAngle = ReadFloat(spotInfo, "innerConeAngle", data.SpotInnerConeAngle);
                            data.SpotOuterConeAngle = ReadFloat(spotInfo, "outerConeAngle", data.SpotOuterConeAngle);
                        }
                        else if (lightTypeName == "point")
                        {
                            data.Type = LightType::Point;
                        }
                        else
                        {
                            CauldronWarning(L"Unknown light type. Using default type");
                        }

                        // Auto-set depth bias for now. This is not an "officical" value from gltf, but we can add it if we
                        // want to support different settings (which we had done for Hanger Demo in Cauldron v1.4)
                        data.DepthBias = (data.Type == LightType::Spot) ? (70.0f / 100000.0f) : (1000.0f / 100000.0f);
                        data.Name = StringToWString(light["name"].get<std::string>());

                        glTFDataRep->LightData.push_back(std::move(data));
                    }
                }
            }

            // Load cameras
            auto cameraIt = glTFData.find("cameras");
            if (cameraIt != glTFData.end())
            {
                glTFDataRep->CameraData.reserve(cameraIt->size());
                for (const json& camera : *cameraIt)
                {
                    CameraComponentData data;

                    if (camera.find("name") != camera.end())
                        data.Name = StringToWString(camera["name"].get<std::string>());

                    std::string cameraTypeName = camera["type"].get<std::string>();
                    if (cameraTypeName == "perspective")
                    {
                        data.Type = CameraType::Perspective;

                        const json& perspective = camera["perspective"];
                        data.Znear = perspective["znear"].get<float>();
                        data.Zfar = perspective["zfar"].get<float>();
                        data.Perspective.AspectRatio = GetFramework()->GetAspectRatio();
                        data.Perspective.Yfov = std::min<float>(perspective["yfov"].get<float>(), CAULDRON_PI2 / data.Perspective.AspectRatio);
                    }
                    else if (cameraTypeName == "orthographic")
                    {
                        data.Type = CameraType::Orthographic;

                        const json& orthographic = camera["orthographic"];
                        data.Znear = orthographic["znear"].get<float>();
                        data.Zfar = orthographic["zfar"].get<float>();
                        data.Orthographic.Xmag = orthographic["xmag"].get<float>();
                        data.Orthographic.Ymag = orthographic["ymag"].get<float>();
                    }
                    else
                    {
                        CauldronWarning(L"Unknown camera type");
                    }

                    glTFDataRep->CameraData.push_back(std::move(data));
                }
            }

            // For now, just create a task to call when all other loads are done (this task may wait for the loads to complete)
            GetTaskManager()->AddTask(Task(std::bind(&GLTFLoader::PostGLTFContentLoadCompleted, this, std::placeholders::_1), glTFDataRep));
        }

        // Done with file path data
        delete pFileToLoad;
    }

    void GLTFLoader::LoadGLTFTexturesCompleted(const std::vector<const Texture*>& textureList, void* pCallbackParams)
    {
        GLTFDataRep* pGLTFData = reinterpret_cast<GLTFDataRep*>(pCallbackParams);

        // Copy the textures that were loaded to our own reference list
        pGLTFData->pLoadedContentRep->TextureAssets = textureList;

        // Fix up the material texture pointers properly
        auto& materials = pGLTFData->pLoadedContentRep->Materials;
        auto iter = materials.begin();
        while (iter != materials.end())
        {
            for (int i = 0; i < static_cast<int>(TextureClass::Count); ++i)
            {
                const TextureInfo* pTextureInfo = (*iter)->GetTextureInfo(static_cast<TextureClass>(i));
                if (pTextureInfo)
                    const_cast<TextureInfo*>(pTextureInfo)->pTexture = textureList[(uint64_t)pTextureInfo->pTexture];
            }

            ++iter;
        }

        // Mark load of buffer data complete and notify in case someone was waiting
        {
            std::unique_lock<std::mutex>    lock(pGLTFData->CriticalSection);
            pGLTFData->TexturesLoaded = true;
            pGLTFData->TextureCV.notify_one();
        }
    }

    void GLTFLoader::LoadGLTFBuffer(void* pParam)
    {
        GLTFBufferLoadParams* pLoadData = reinterpret_cast<GLTFBufferLoadParams*>(pParam);

        int64_t dataSize = GetFileSize(pLoadData->BufferName.c_str());

        // Allocate the data and read it in
        pLoadData->pGLTFData->GLTFBufferData[pLoadData->BufferIndex].resize(dataSize+1);
        CauldronAssert(ASSERT_ERROR,
                       dataSize == ReadFileAll(pLoadData->BufferName.c_str(), pLoadData->pGLTFData->GLTFBufferData[pLoadData->BufferIndex].data(), dataSize),
                       L"Error reading buffer file %ls",
                       pLoadData->BufferName.c_str());

        // Done with this memory
        delete pLoadData;
    }

    void GLTFLoader::LoadGLTFBuffersCompleted(void* pParam)
    {
        GLTFDataRep* pGLTFData = reinterpret_cast<GLTFDataRep*>(pParam);
        const json& glTFData = *pGLTFData->pGLTFJsonData;

        bool hasMeshData = glTFData.find("meshes") != glTFData.end();
        bool hasAnimationData = glTFData.find("animations") != glTFData.end();
        bool hasAnimationSkins = glTFData.find("skins") != glTFData.end();

        uint32_t numLoads = 0;

        if (hasMeshData)
        {
            numLoads += (uint32_t)glTFData["meshes"].size();
        }
        if (hasAnimationData)
        {
            numLoads += (uint32_t)glTFData["animations"].size();
        }
        if (hasAnimationSkins)
        {
            numLoads += (uint32_t)glTFData["skins"].size();
        }

        // Create a completion callback to be called after all buffer loads have completed
        TaskCompletionCallback* pLoadCompleteCallback = new TaskCompletionCallback(Task(&GLTFLoader::GLTFAllBufferAssetLoadsCompleted, pParam), numLoads);

        // Dispatch a task for every mesh we need to load
        if (hasMeshData)
        {
            const json& meshes = glTFData["meshes"];
            std::queue<Task> meshTaskList;

            pGLTFData->pLoadedContentRep->Meshes.resize(meshes.size());

            for (int i = 0; i < meshes.size(); ++i)
            {
                // Build load info for the loading job
                GLTFBufferLoadParams* pBufferLoadParams = new GLTFBufferLoadParams();
                pBufferLoadParams->pGLTFData = pGLTFData;
                pBufferLoadParams->BufferIndex = i;
                pBufferLoadParams->BufferName = pGLTFData->GLTFFilePath;

                if (meshes[i].find("name") != meshes[i].end())
                {
                    std::string meshName = meshes[i]["name"];
                    pBufferLoadParams->BufferName += StringToWString(meshName);
                }
                else
                {
                    std::wstring meshName = L"Mesh_";
                    meshName += std::to_wstring(i);
                    pBufferLoadParams->BufferName += meshName;
                }

                // Push the task
                meshTaskList.push(Task(&GLTFLoader::LoadGLTFMesh, pBufferLoadParams, pLoadCompleteCallback));
            }

            // Dispatch the loading tasks
            GetTaskManager()->AddTaskList(meshTaskList);
        }

        if (hasAnimationData)
        {
            const json& animationsJson = glTFData["animations"];
            std::queue<Task> animationTaskList;

            pGLTFData->pLoadedContentRep->Animations.resize(animationsJson.size());

            for (int i = 0; i < animationsJson.size(); ++i)
            {
                // Build load info for the loading job
                GLTFBufferLoadParams* pBufferLoadParams = new GLTFBufferLoadParams();
                pBufferLoadParams->pGLTFData            = pGLTFData;
                pBufferLoadParams->BufferIndex          = i;
                pBufferLoadParams->BufferName           = pGLTFData->GLTFFilePath;

                if (animationsJson[i].find("name") != animationsJson[i].end())
                {
                    std::string animationName = animationsJson[i]["name"];
                    pBufferLoadParams->BufferName += StringToWString(animationName);
                }
                else
                {
                    std::wstring animationName = L"Animation_";
                    animationName += std::to_wstring(i);
                    pBufferLoadParams->BufferName += animationName;
                }

                // Push the task
                animationTaskList.push(Task(&GLTFLoader::LoadGLTFAnimation, pBufferLoadParams, pLoadCompleteCallback));
            }

            // Dispatch the loading tasks
            GetTaskManager()->AddTaskList(animationTaskList);
        }

        if (hasAnimationSkins)
        {
            const json&      skinsJson = glTFData["skins"];
            std::queue<Task> skinTaskList;

            pGLTFData->pLoadedContentRep->Skins.resize(skinsJson.size());

            for (size_t i = 0; i < skinsJson.size(); ++i)
            {
                // Build load info for the loading job
                GLTFBufferLoadParams* pBufferLoadParams = new GLTFBufferLoadParams();
                pBufferLoadParams->pGLTFData            = pGLTFData;
                pBufferLoadParams->BufferIndex          = (uint32_t)i;
                pBufferLoadParams->BufferName           = pGLTFData->GLTFFilePath;

                if (skinsJson[i].find("name") != skinsJson[i].end())
                {
                    std::string skinName = skinsJson[i]["name"];
                    pBufferLoadParams->BufferName += StringToWString(skinName);
                }
                else
                {
                    std::wstring skinName = L"Skin_";
                    skinName += std::to_wstring(i);
                    pBufferLoadParams->BufferName += skinName;
                }

                // Push the task
                skinTaskList.push(Task(&GLTFLoader::LoadGLTFSkin, pBufferLoadParams, pLoadCompleteCallback));
            }

            // Dispatch the loading tasks
            GetTaskManager()->AddTaskList(skinTaskList);
        }

    }

    const json* GLTFLoader::LoadVertexBuffer(const json& attributes, const char* attributeName, const json& accessors, const json& bufferViews, const json& buffers, const GLTFBufferLoadParams& params, VertexBufferInformation& info, bool forceConversionToFloat)
    {
        auto attributeIter = attributes.find(attributeName);
        if (attributeIter != attributes.end())
        {
            int attributeID = attributes[attributeName];
            auto& accessor = accessors[attributeID];

            std::string& type = accessor["type"].get<std::string>();
            uint32_t resourceFormatDimension = ResourceFormatDimension(type);

            int32_t resourceFormatType = accessor["componentType"];
            uint32_t resourceDataStride = ResourceDataStride(resourceFormatType);
            uint32_t stride = resourceFormatDimension * resourceDataStride;

            uint32_t byteOffset = 0;
            auto byteOffsetIt = accessor.find("byteOffset");
            if (byteOffsetIt != accessor.end())
                byteOffset = byteOffsetIt->get<uint32_t>();

            // Update vertex buffer information
            info.Count = accessor["count"].get<uint32_t>();
            info.AttributeDataFormat = ResourceFormatType(type);
            info.ResourceDataFormat = ResourceDataFormat(info.AttributeDataFormat, resourceFormatType);

            // Original buffer validation
                BufferViewInfo bufferViewInfo = GetBufferInfo(accessor, bufferViews);

            if (strcmp(attributeName, "JOINTS_0") == 0)
                stride = static_cast<uint32_t>(bufferViewInfo.Stride);

            // Only support tightly packed data
            CauldronAssert(ASSERT_WARNING, bufferViewInfo.Stride == 0 || bufferViewInfo.Stride == stride, L"Stride doesn't match between the type of the accessor and the type of the vertex attribute.");

            // Verify that the buffer is big enough
            uint32_t totalLength = info.Count * stride;
            CauldronAssert(ASSERT_CRITICAL, byteOffset + totalLength <= bufferViewInfo.Length, L"Vertex buffer out of buffer view bounds.");

            uint32_t bufferLength = buffers[bufferViewInfo.BufferID]["byteLength"].get<uint32_t>();
            CauldronAssert(ASSERT_CRITICAL, bufferViewInfo.Offset + byteOffset + totalLength <= bufferLength, L"Vertex buffer out of buffer bounds.");

            // Get a pointer to the data at the correct offset into the buffer
            char* data = params.pGLTFData->GLTFBufferData[bufferViewInfo.BufferID].data();
            data += bufferViewInfo.Offset + byteOffset;

            // Verify that the component is already using floats or allowed to be converted to floats
            if (!(strcmp(attributeName, "JOINTS_0") == 0 || strcmp(attributeName, "JOINTS_1")))
            {
                CauldronAssert(ASSERT_ERROR, (resourceFormatType == g_GLTFComponentType_Float) || forceConversionToFloat, L"Unsupported component type for vertex attribute.");
            }

            // Convert to float if necessary
            std::vector<float> convertedData;
            if (resourceFormatType != g_GLTFComponentType_Float && forceConversionToFloat)
            {
                // Update resource format, stride and length
                info.ResourceDataFormat = ResourceDataFormat(info.AttributeDataFormat, g_GLTFComponentType_Float);
                resourceDataStride = ResourceDataStride(g_GLTFComponentType_Float);
                stride = resourceFormatDimension * resourceDataStride;
                totalLength = info.Count * stride;

                // Allocate a new buffer of floats for the converted component
                const int elementCount = info.Count * resourceDataStride;
                convertedData.resize(elementCount);

                // Do conversion. Data that requires conversion from byte/short to floats is normalized.
                if (resourceFormatType == g_GLTFComponentType_UnsignedByte)
                {
                    const uint8_t* dataPtr = (uint8_t*)data;
                    for (int i = 0; i < elementCount; i++)
                    {
                        convertedData[i] = float(dataPtr[i]) / 256.0f;
                    }
                }
                else if (resourceFormatType == g_GLTFComponentType_UnsignedShort)
                {
                    const uint16_t* dataPtr = (uint16_t*)data;
                    for (int i = 0; i < elementCount; i++)
                    {
                        convertedData[i] = float(dataPtr[i]) / 65536.0f;
                    }
                }
                else
                {
                    CauldronAssert(ASSERT_ERROR, false, L"Unsupported component type conversion for vertex attribute.");
                }

                // Make the data pointer point towards our converted data
                data = (char*)convertedData.data();
            }

            // align buffer size up to 4-bytes for compatibility with StructuredBuffers with uints.
            uint32_t totalAlignedLength = AlignUp(totalLength, 4u);

            BufferDesc desc = BufferDesc::Vertex(StringToWString(std::string("VertexBuffer_") + std::string(attributeName)).c_str(), totalAlignedLength, stride);
            info.pBuffer = Buffer::CreateBufferResource(&desc, ResourceState::CopyDest);
            info.pBuffer->CopyData(data, totalLength, params.pUploadCtx, ResourceState::VertexBufferResource);
            return &accessor;
        }
        return nullptr;
    }

    void GLTFLoader::LoadIndexBuffer(const json& primitive, const json& accessors, const json& bufferViews, const json& buffers, const GLTFBufferLoadParams& params, IndexBufferInformation& info)
    {
        auto indicesIt = primitive.find("indices");
        if (indicesIt != primitive.end())
        {
            int indicesID = primitive["indices"];
            auto& accessor = accessors[indicesID];

            uint32_t byteOffset = 0;
            auto byteOffsetIt = accessor.find("byteOffset");
            if (byteOffsetIt != accessor.end())
                byteOffset = byteOffsetIt->get<uint32_t>();

            info.Count = accessor["count"].get<uint32_t>();

            std::string& type = accessor["type"].get<std::string>();
            CauldronAssert(ASSERT_ERROR, type == "SCALAR", L"Indices types are only scalar");

            // create buffer
            BufferViewInfo bufferViewInfo = GetBufferInfo(accessor, bufferViews);
            const char* data = params.pGLTFData->GLTFBufferData[bufferViewInfo.BufferID].data();
            data += bufferViewInfo.Offset + byteOffset;

            int componentType = accessor["componentType"];
            uint32_t stride = ResourceDataStride(componentType);
            // only support tightly packed data
            CauldronAssert(ASSERT_WARNING, bufferViewInfo.Stride == 0 || bufferViewInfo.Stride == stride, L"Stride doesn't match between the type of the accessor and the type of the index buffer.");

            // verify that the buffer is big enough
            uint32_t totalLength = info.Count * stride;
            CauldronAssert(ASSERT_CRITICAL, byteOffset + totalLength <= bufferViewInfo.Length, L"Index buffer out of buffer view bounds.");

            size_t bufferLength = buffers[bufferViewInfo.BufferID]["byteLength"].get<size_t>();
            CauldronAssert(ASSERT_CRITICAL, bufferViewInfo.Offset + byteOffset + totalLength <= bufferLength, L"Index buffer out of buffer bounds.");

            std::vector<uint16_t> convertedData;
            const uint8_t* dataPtr = (uint8_t*)data;
            switch (componentType)
            {
            case g_GLTFComponentType_UnsignedByte:
                convertedData.resize(info.Count);
                for (uint32_t i = 0; i < info.Count; i++)
                {
                    convertedData[i] = uint16_t(dataPtr[i]);
                }
                data = (char*)convertedData.data();
                totalLength = info.Count * 2;
                info.IndexFormat = ResourceFormat::R16_UINT;
                break;
            case g_GLTFComponentType_UnsignedShort:
                info.IndexFormat = ResourceFormat::R16_UINT;
                break;
            case g_GLTFComponentType_UnsignedInt:
                info.IndexFormat = ResourceFormat::R32_UINT;
                break;
            default:
                CauldronWarning(L"Unsupported component type for index buffer.");
                return;
            }

            // align buffer size up to 4-bytes for compatibility with StructuredBuffers with uints.
            uint32_t totalAlignedLength = AlignUp(totalLength, 4u);

            BufferDesc desc = BufferDesc::Index(L"IndexBuffer", totalAlignedLength, info.IndexFormat);
            info.pBuffer = Buffer::CreateBufferResource(&desc, ResourceState::CopyDest);
            info.pBuffer->CopyData(data, totalLength, params.pUploadCtx, ResourceState::IndexBufferResource);
        }
    }

    void GLTFLoader::LoadAnimInterpolant(AnimInterpolants& animInterpolant, const json& gltfData, int32_t interpAccessorID, const GLTFBufferLoadParams* pBufferLoadParams)
    {
        auto&       accessors    = gltfData["accessors"];
        auto&       bufferViews  = gltfData["bufferViews"];

        const json& inAccessor = accessors.at(interpAccessorID);

        int32_t bufferViewIdx = inAccessor.value("bufferView", -1);
        CauldronAssert(ASSERT_CRITICAL, bufferViewIdx >= 0, L"Animation buffer view ID invalid");
        const json& bufferView = bufferViews.at(bufferViewIdx);

        int32_t bufferIdx = bufferView.value("buffer", -1);
        CauldronAssert(ASSERT_CRITICAL, bufferIdx >= 0, L"Animation buffer ID invalid");

        std::vector<char>& animData = pBufferLoadParams->pGLTFData->GLTFBufferData[bufferIdx];

        int32_t offset     = bufferView.value("byteOffset", 0);
        int32_t byteLength = bufferView["byteLength"];
        int32_t byteOffset = inAccessor.value("byteOffset", 0);

        offset += byteOffset;
        byteLength -= byteOffset;

        animInterpolant.Data      = std::vector<char>(animData.begin() + offset, animData.end());
        animInterpolant.Dimension = ResourceFormatDimension(inAccessor["type"]);
        animInterpolant.Stride    = animInterpolant.Dimension * ResourceDataStride(inAccessor["componentType"]);
        animInterpolant.Count     = inAccessor["count"];

        // Read in min/max according to how big they are (rest is zero initialized)
        if (inAccessor.find("min") != inAccessor.end())
        {
            animInterpolant.Min[0] = inAccessor["min"][0];
            if (inAccessor["min"].size() > 1)
            {
                animInterpolant.Min[1] = inAccessor["min"][1];

                if (inAccessor["min"].size() > 2)
                {
                    animInterpolant.Min[2] = inAccessor["min"][2];

                    if (inAccessor["min"].size() > 3)
                    {
                        animInterpolant.Min[3] = inAccessor["min"][3];
                    }
                }
            }
        }

        if (inAccessor.find("max") != inAccessor.end())
        {
            animInterpolant.Max[0] = inAccessor["max"][0];
            if (inAccessor["max"].size() > 1)
            {
                animInterpolant.Max[1] = inAccessor["max"][1];

                if (inAccessor["max"].size() > 2)
                {
                    animInterpolant.Max[2] = inAccessor["max"][2];

                    if (inAccessor["max"].size() > 3)
                    {
                        animInterpolant.Max[3] = inAccessor["max"][3];
                    }
                }
            }
        }
    }

    void GLTFLoader::LoadAnimInterpolants(AnimChannel* pAnimChannel, AnimChannel::ComponentSampler samplerType, int32_t samplerIndex, const GLTFBufferLoadParams* pBufferLoadParams)
    {
        const json& gltfData = *pBufferLoadParams->pGLTFData->pGLTFJsonData;
        auto& animations = gltfData["animations"];
        auto& samplersJson = animations[pBufferLoadParams->BufferIndex]["samplers"];

        
        // Create the component sampler and retrieve interpolant addresses to be populated
        AnimInterpolants* pTimeInterpolant = nullptr;
        AnimInterpolants* pValueInterpolant = nullptr;
        pAnimChannel->CreateComponentSampler(samplerType, &pTimeInterpolant, &pValueInterpolant);

        //Populate interpolant data
        LoadAnimInterpolant(*pTimeInterpolant, gltfData, samplersJson[samplerIndex]["input"], pBufferLoadParams);
        LoadAnimInterpolant(*pValueInterpolant, gltfData, samplersJson[samplerIndex]["output"], pBufferLoadParams);

        // Validate the data
        switch (samplerType)
        {
            case AnimChannel::ComponentSampler::Translation:
            {
                CauldronAssert(ASSERT_CRITICAL, pValueInterpolant->Stride == 3 * 4, L"Animation translation stride differs from expected value");
                CauldronAssert(ASSERT_CRITICAL, pValueInterpolant->Dimension == 3, L"Animation translation dimension differs from expected value");
            }
            break;
            case AnimChannel::ComponentSampler::Rotation:
            {
                CauldronAssert(ASSERT_CRITICAL, pValueInterpolant->Stride == 4 * 4, L"Animation rotation stride differs from expected value");
                CauldronAssert(ASSERT_CRITICAL, pValueInterpolant->Dimension == 4, L"Animation rotation dimension differs from expected value");
            }
            break;
            case AnimChannel::ComponentSampler::Scale:
            {
                CauldronAssert(ASSERT_CRITICAL, pValueInterpolant->Stride == 3 * 4, L"Animation scale stride differs from expected value");
                CauldronAssert(ASSERT_CRITICAL, pValueInterpolant->Dimension == 3, L"Animation scale dimension differs from expected value");
            }
            break;
            default:
                CauldronCritical(L"Unsupported Animation component sampler type");
            break;
        }
    }

    // Called for each mesh in order to create and load mesh information
    void GLTFLoader::LoadGLTFMesh(void* pParam)
    {
        GLTFBufferLoadParams* pBufferLoadParams = reinterpret_cast<GLTFBufferLoadParams*>(pParam);

        const json& glTFData = *pBufferLoadParams->pGLTFData->pGLTFJsonData;

        // Get the mesh to load
        const json& meshes = glTFData["meshes"];
        auto& accessors = glTFData["accessors"];
        auto& bufferViews = glTFData["bufferViews"];
        auto& buffers = glTFData["buffers"];
        auto& primitives = meshes[pBufferLoadParams->BufferIndex]["primitives"];

        // Create the mesh
        Mesh* pMeshResource = new Mesh(pBufferLoadParams->BufferName, primitives.size());

        UploadContext* pUploadContext = UploadContext::CreateUploadContext();
        pBufferLoadParams->pUploadCtx = pUploadContext;

        // Start loading all of the surfaces for it (a surface in our context is mesh geometry that is associated with a particular material)
        std::vector<VertexBufferInformation> vertexBufferPositions;
        for (uint32_t i = 0; i < (uint32_t)primitives.size(); ++i)
        {
            auto& primitive = primitives[i];
            auto& attributes = primitive["attributes"];
            Surface* pSurface = pMeshResource->GetSurface(i);

            // Start by setting up the center and radius (if we got them)
            const json* pPosAccessor = LoadVertexBuffer(attributes, "POSITION", accessors, bufferViews, buffers , *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Position), false);
            if (pPosAccessor != nullptr && pPosAccessor->contains("max") && pPosAccessor->contains("min"))
            {
                auto& maxAccessor = (*pPosAccessor)["max"];
                auto& minAccessor = (*pPosAccessor)["min"];
                Vec4 max = Vec4(maxAccessor[0], maxAccessor[1], maxAccessor[2], maxAccessor.size() == 4 ? maxAccessor[3] : 0);
                Vec4 min = Vec4(minAccessor[0], minAccessor[1], minAccessor[2], minAccessor.size() == 4 ? minAccessor[3] : 0);

                pSurface->Center() = (min + max) * 0.5f;
                pSurface->Radius() = max - pMeshResource->GetSurface(i)->Center();
                pSurface->Center().setW(1.f);
            }
            vertexBufferPositions.push_back(pSurface->GetVertexBuffer(VertexAttributeType::Position));

            LoadVertexBuffer(attributes, "NORMAL",      accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Normal),    false);
            LoadVertexBuffer(attributes, "TANGENT",     accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Tangent),   false);
            LoadVertexBuffer(attributes, "TEXCOORD_0",  accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Texcoord0), true);
            LoadVertexBuffer(attributes, "TEXCOORD_1",  accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Texcoord1), true);
            LoadVertexBuffer(attributes, "COLOR_0",     accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Color0),    true);
            LoadVertexBuffer(attributes, "COLOR_1",     accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Color1),    true);
            LoadVertexBuffer(attributes, "WEIGHTS_0",   accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Weights0),  true);
            LoadVertexBuffer(attributes, "WEIGHTS_1",   accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Weights1),  true);
            LoadVertexBuffer(attributes, "JOINTS_0",    accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Joints0),   false);
            LoadVertexBuffer(attributes, "JOINTS_1",    accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetVertexBuffer(VertexAttributeType::Joints1),   false);
            LoadIndexBuffer(primitive, accessors, bufferViews, buffers, *pBufferLoadParams, pSurface->GetIndexBuffer());

            bool hasAnimationSkins = glTFData.find("skins") != glTFData.end();
            if (hasAnimationSkins)
            {
                // Previous Positions
                {
                    VertexBufferInformation previousPosInfo = pSurface->GetVertexBuffer(VertexAttributeType::Position);
                    BufferDesc              desc            = previousPosInfo.pBuffer->GetDesc();
                    desc.Name                               = L"PreviousPositions";
                    desc.Flags                              = ResourceFlags::AllowUnorderedAccess;
                    previousPosInfo.pBuffer                 = Buffer::CreateBufferResource(&desc, ResourceState::VertexBufferResource);

                    pSurface->GetVertexBuffer(VertexAttributeType::PreviousPosition) = previousPosInfo;
                }

                // Set Mesh to have animated BLAS for raytracing
                pMeshResource->setAnimatedBlas(true);
            }

            int  materialIndex = 0;
            auto materialIndexIt = primitive.find("material");
            if (materialIndexIt != primitive.end())
                materialIndex = primitive["material"].get<int>();
            CauldronAssert(ASSERT_ERROR, materialIndex >= 0 && materialIndex < pBufferLoadParams->pGLTFData->pLoadedContentRep->Materials.size(), L"Referenced material out of bounds");
            pSurface->SetMaterial(pBufferLoadParams->pGLTFData->pLoadedContentRep->Materials[materialIndex]);
        }

        pUploadContext->Execute();
        delete pUploadContext;

        // Add BLAS info
        if (GetConfig()->BuildRayTracingAccelerationStructure)
        {
            pMeshResource->GetStaticBlas()->AddGeometry(pMeshResource, vertexBufferPositions);
            pMeshResource->GetStaticBlas()->InitBufferResources();
        }

        pBufferLoadParams->pGLTFData->pLoadedContentRep->Meshes[pBufferLoadParams->BufferIndex] = pMeshResource;

        // Done with this memory
        delete pBufferLoadParams;
    }

    void GLTFLoader::LoadGLTFAnimation(void* pParam)
    {
        GLTFBufferLoadParams* pBufferLoadParams = reinterpret_cast<GLTFBufferLoadParams*>(pParam);

        const json& glTFData = *pBufferLoadParams->pGLTFData->pGLTFJsonData;

        const json& animations  = glTFData["animations"];
        auto&       accessors   = glTFData["accessors"];
        auto&       bufferViews = glTFData["bufferViews"];
        auto&       buffers     = glTFData["buffers"];

        auto& channelsJson = animations[pBufferLoadParams->BufferIndex]["channels"];
        auto& samplersJson = animations[pBufferLoadParams->BufferIndex]["samplers"];

        pBufferLoadParams->pGLTFData->pLoadedContentRep->Animations[pBufferLoadParams->BufferIndex] = new Animation();

        Animation* animation = pBufferLoadParams->pGLTFData->pLoadedContentRep->Animations[pBufferLoadParams->BufferIndex];

        for (int c = 0; c < channelsJson.size(); c++)
        {
            auto& channel = channelsJson[c];
            int   sampler = channel["sampler"];
            int     node  = channel["target"]["node"];
            std::string path = channel["target"]["path"];

            const uint32_t numNodes = (uint32_t)glTFData["nodes"].size();

            // This is inefficient on gltfScenes as a whole (that mix geometry and animations),
            // but effective on anim heavy gltf
            animation->SetNumAnimationChannels(numNodes);

            AnimChannel* animChannelNew = animation->GetAnimationChannel(node);

            AnimChannel::ComponentSampler samplerType = (path == "translation") ? AnimChannel::ComponentSampler::Translation :
                ((path == "rotation") ? AnimChannel::ComponentSampler::Rotation : AnimChannel::ComponentSampler::Scale);
            LoadAnimInterpolants(animChannelNew, samplerType, sampler, pBufferLoadParams);

            // Get the duration of this channel component
            animation->SetDuration(std::max<float>(animChannelNew->GetComponentSamplerDuration(samplerType), animation->GetDuration()));
        }
    }

    void GLTFLoader::LoadGLTFSkin(void* pParam)
    {
        GLTFBufferLoadParams* pBufferLoadParams = reinterpret_cast<GLTFBufferLoadParams*>(pParam);

        const json& glTFData = *pBufferLoadParams->pGLTFData->pGLTFJsonData;

        const json& skinEntry = glTFData["skins"][pBufferLoadParams->BufferIndex];

        pBufferLoadParams->pGLTFData->pLoadedContentRep->Skins[pBufferLoadParams->BufferIndex] = new AnimationSkin();
        AnimationSkin* skin = pBufferLoadParams->pGLTFData->pLoadedContentRep->Skins[pBufferLoadParams->BufferIndex];

        GetBufferDetails(skinEntry["inverseBindMatrices"].get<int>(), &skin->m_InverseBindMatrices, pBufferLoadParams);

        skin->m_skeletonId = -1;
        auto skeletonIt    = skinEntry.find("skeleton");
        if (skeletonIt != skinEntry.end())
            skin->m_skeletonId = skinEntry["skeleton"].get<int>();

        const json& jointsJson = skinEntry["joints"];
        for (uint32_t n = 0; n < jointsJson.size(); n++)
        {
            skin->m_jointsNodeIdx.push_back(jointsJson[n]);
        }
    }

    void GLTFLoader::GetBufferDetails(int accessor, AnimInterpolants* pAccessor, const GLTFBufferLoadParams* pBufferLoadParams)
    {
        const json& gltfData    = *pBufferLoadParams->pGLTFData->pGLTFJsonData;
        auto&       accessors   = gltfData["accessors"];
        auto&       bufferViews = gltfData["bufferViews"];

        const json& inAccessor = accessors.at(accessor);

        int32_t bufferViewIdx = inAccessor.value("bufferView", -1);
        assert(bufferViewIdx >= 0);
        const json& bufferView = bufferViews.at(bufferViewIdx);

        int32_t bufferIdx = bufferView.value("buffer", -1);
        assert(bufferIdx >= 0);

        std::vector<char> &animData = pBufferLoadParams->pGLTFData->GLTFBufferData[bufferIdx];

        int32_t offset     = bufferView.value("byteOffset", 0);
        int32_t byteLength = bufferView["byteLength"];
        int32_t byteOffset = inAccessor.value("byteOffset", 0);

        offset += byteOffset;
        byteLength -= byteOffset;

        pAccessor->Data      = std::vector<char>(animData.begin() + offset, animData.end());
        pAccessor->Dimension = ResourceFormatDimension(inAccessor["type"]);
        pAccessor->Stride    = pAccessor->Dimension * ResourceDataStride(inAccessor["componentType"]);
        pAccessor->Count     = inAccessor["count"];
    }

    // Called once all buffer-related assets have been created and uploaded (i.e. Mesh, Animations, etc.)
    void GLTFLoader::GLTFAllBufferAssetLoadsCompleted(void* pParam)
    {
        GLTFDataRep* pGLTFData = reinterpret_cast<GLTFDataRep*>(pParam);

        // Mark load of buffer data complete and notify in case someone was waiting
        std::unique_lock<std::mutex>    lock(pGLTFData->CriticalSection);
        pGLTFData->BuffersLoaded = true;
        pGLTFData->BufferCV.notify_one();
    }

    void GLTFLoader::BuildBLAS(std::vector<Mesh*> meshes)
    {
        std::vector<CommandList*> cmdLists(1);
        cmdLists[0] = GetDevice()->CreateCommandList(L"Build BLAS cmdList", CommandQueue::Graphics);

        std::vector<Barrier> blasBarriers;
        for (Mesh* pMesh : meshes)
        {
            pMesh->GetStaticBlas()->Build(cmdLists[0]);
            blasBarriers.push_back(Barrier::UAV(pMesh->GetStaticBlas()->GetBuffer()->GetResource()));
        }
        ResourceBarrier(cmdLists[0], (uint32_t)blasBarriers.size(), blasBarriers.data());

        CloseCmdList(cmdLists[0]);
        GetDevice()->ExecuteCommandListsImmediate(cmdLists, CommandQueue::Graphics);

        delete cmdLists[0];
    }

    void GLTFLoader::InitSkinningData(const Mesh* pMesh, AnimationComponentData* pComponentData)
    {
        const size_t numSurfaces = pMesh->GetNumSurfaces();
        pComponentData->m_skinnedPositions.resize(numSurfaces);
        pComponentData->m_skinnedNormals.resize(numSurfaces);
        pComponentData->m_skinnedPreviousPosition.resize(numSurfaces);

        for (uint32_t i = 0; i < numSurfaces; ++i)
        {
            const Surface* pSurface = pMesh->GetSurface(i);

            // Skinned Positions
            {
                VertexBufferInformation skinnedPosInfo = pSurface->GetVertexBuffer(VertexAttributeType::Position);
                BufferDesc              desc           = skinnedPosInfo.pBuffer->GetDesc();
                desc.Name                              = L"SkinnedPositions";
                desc.Flags                             = ResourceFlags::AllowUnorderedAccess;
                skinnedPosInfo.pBuffer                 = Buffer::CreateBufferResource(&desc, ResourceState::VertexBufferResource);

                pComponentData->m_skinnedPositions[i] = skinnedPosInfo;
            }

            // Previous Positions
            {
                VertexBufferInformation previousPosInfo = pSurface->GetVertexBuffer(VertexAttributeType::Position);
                BufferDesc              desc            = previousPosInfo.pBuffer->GetDesc();
                desc.Name                               = L"PreviousPositions";
                desc.Flags                              = ResourceFlags::AllowUnorderedAccess;
                previousPosInfo.pBuffer                 = Buffer::CreateBufferResource(&desc, ResourceState::VertexBufferResource);

                pComponentData->m_skinnedPreviousPosition[i] = previousPosInfo;
            }

            // Skinned Normals
            {
                if (!pSurface->GetVertexBuffer(VertexAttributeType::Normal).pBuffer)
                {
                    CauldronError(L"Skinned Meshes must have normal attribute");
                }

                VertexBufferInformation skinnedNormalsInfo = pSurface->GetVertexBuffer(VertexAttributeType::Normal);
                BufferDesc              desc               = skinnedNormalsInfo.pBuffer->GetDesc();
                desc.Name                                  = L"SkinnedNormals";
                desc.Flags                                 = ResourceFlags::AllowUnorderedAccess;
                skinnedNormalsInfo.pBuffer                 = Buffer::CreateBufferResource(&desc, ResourceState::VertexBufferResource);

                pComponentData->m_skinnedNormals[i] = skinnedNormalsInfo;
            }
        }

        // Add BLAS info
        if (GetConfig()->BuildRayTracingAccelerationStructure)
        {
            pComponentData->m_animatedBlas->AddGeometry(pMesh, pComponentData->m_skinnedPositions);
            pComponentData->m_animatedBlas->InitBufferResources();
        }
    }

    bool isAnimationTarget(const json& pGLTFData, const uint32_t nodeIndex)
    {
        for (const auto& animationEntry : pGLTFData["animations"])
        {
            for (const auto& channels : animationEntry["channels"])
            {
                if (nodeIndex == channels["target"]["node"])
                    return true;
            }
        }

        return false;
    }

    bool entityInAnimatedSubTree(Entity* pParentEntity)
    {
        if (pParentEntity != nullptr)
        {
            if (pParentEntity->HasComponent(AnimationComponentMgr::Get()))
                return true;
            entityInAnimatedSubTree(pParentEntity->GetParent());
        }

        return false;
    };

    void GLTFLoader::PostGLTFContentLoadCompleted(void* pParam)
    {
        GLTFDataRep* pGLTFData = reinterpret_cast<GLTFDataRep*>(pParam);
        const json& glTFData = *pGLTFData->pGLTFJsonData;

        // ID of the current model being loaded
        static uint32_t modelIndex = 0;

        // create entities and component data
        // If we had buffers, make sure all content was created/loaded
        if (glTFData.find("buffers") != glTFData.end())
        {
            std::unique_lock<std::mutex> lock(pGLTFData->CriticalSection);
            pGLTFData->BufferCV.wait(lock, [pGLTFData] { return pGLTFData->BuffersLoaded; });
        }

        // If we had textures, make sure all content was created/loaded
        if (glTFData.find("images") != glTFData.end())
        {
            std::unique_lock<std::mutex> lock(pGLTFData->CriticalSection);
            pGLTFData->TextureCV.wait(lock, [pGLTFData] { return pGLTFData->TexturesLoaded; });
        }

        bool hasNodes = glTFData.find("nodes") != glTFData.end();
        bool hasScene = glTFData.find("scenes") != glTFData.end();
        CauldronAssert(ASSERT_ERROR, hasScene && hasNodes, L"Could not find nodes and / or scene. No scene entities will be created!");

        // If scene is valid, traverse it and build up our scene entities as we go
        if (hasScene && hasNodes)
        {
            // We are going to traverse each scene through it's nodes, creating entities as we go, each root node/child is an entity (can be nested)
            const json& nodes = glTFData["nodes"];
            const json& scenes = glTFData["scenes"];

            // We want to keep track if which nodes we've visited to make sure we don't miss any or don't visit any twice
            std::vector<bool> visitedNodes(nodes.size(), false);

            // Define a function that will process our nodes recursively and setup as needed
            std::function<void(uint32_t nodeIndex, Entity* pParentEntity, EntityDataBlock* pBackingMem)> processNodeRecursive = [&](uint32_t nodeIndex, Entity* pParentEntity, EntityDataBlock* pParentEntityBlock)
            {
                auto& node = nodes[nodeIndex];

                // Get it's name if it has one
                std::wstring nodeName = (node.find("name") != node.end()) ? StringToWString(node["name"].get<std::string>()) : L"un-named";

                // Mark it as visited as well
                CauldronAssert(ASSERT_CRITICAL, !visitedNodes[nodeIndex], L"Visiting hierarchy nodes more than once. Something has gone horribly wrong. Abort!");
                visitedNodes[nodeIndex] = true;

                // Start by creating an entity. If there is no parent entity, create the entity in the entity block that will back all of it's hierarchy of entities and components
                Entity*             pNewEntity(nullptr);
                EntityDataBlock*    pBackingMem(nullptr);
                if (!pParentEntity)
                {
                    // Memory backing all entity creation
                    EntityDataBlock* pEntityDataBlock = new EntityDataBlock();
                    CauldronAssert(ASSERT_CRITICAL, pEntityDataBlock, L"Could not allocate new entity data block for parent entity %ls", nodeName.c_str());
                    pGLTFData->pLoadedContentRep->EntityDataBlocks.push_back(pEntityDataBlock);
                    pBackingMem = pEntityDataBlock;

                    pNewEntity = new Entity(nodeName.c_str());
                    CauldronAssert(ASSERT_CRITICAL, pNewEntity, L"Could not allocate new entity %ls", nodeName.c_str());
                    pEntityDataBlock->pEntity = pNewEntity;
                }

                // Otherwise, add this entity to the hierarchy
                else
                {
                    pBackingMem = pParentEntityBlock;
                    pNewEntity = new Entity(nodeName.c_str());
                    CauldronAssert(ASSERT_CRITICAL, pNewEntity, L"Could not allocate new entity %ls", nodeName.c_str());
                    pParentEntity->AddChildEntity(pNewEntity);
                    pNewEntity->SetParent(pParentEntity);
                }

                // Process transforms for this entity
                Mat4 transform;
                if (node.find("matrix") != node.end())
                    transform = ReadMatrix(node, "matrix", Mat4::identity());

                else
                {
                    const Vec3 scale = ReadVec3(node, "scale", Vec3(1.0f, 1.0f, 1.0f));
                    const Vec3 translation = ReadVec3(node, "translation", Vec3(0.0f, 0.0f, 0.0f));
                    const Quat rotation = ReadQuat(node, "rotation", Quat::identity());

                    // build the transformation matrix
                    const Mat4 scaleMatrix = Mat4::scale(scale);
                    const Mat4 translationMatrix = Mat4::translation(translation);
                    const Mat4 rotationMatrix = Mat4::rotation(rotation);

                    transform = translationMatrix * rotationMatrix * scaleMatrix;
                }

                // Process hierarchy (used for non animated models)
                if (pParentEntity)
                    pNewEntity->SetTransform(pParentEntity->GetTransform() * transform);
                else
                    pNewEntity->SetTransform(transform);
                pNewEntity->SetPrevTransform(pNewEntity->GetTransform());


                // Next, do any component creation needed by this entity

                // Add light component (if present)
                if (LightComponentMgr::Get() != nullptr && pGLTFData->LightData.size() > 0)
                {
                    auto nodeExtIt = node.find("extensions");
                    if (nodeExtIt != node.end())
                    {
                        auto lightExtIt = nodeExtIt->find(g_LightExtensionName);
                        if (lightExtIt != nodeExtIt->end())
                        {
                            const int lightIndex = (*lightExtIt)["light"].get<int>();
                            CauldronAssert(ASSERT_ERROR, lightIndex >= 0 && lightIndex < pGLTFData->LightData.size(), L"Referenced light out of bounds");

                            LightComponentData* pComponentData = new LightComponentData(pGLTFData->LightData[lightIndex]);
                            pBackingMem->ComponentsData.push_back(pComponentData);
                            LightComponent* pComponent = LightComponentMgr::Get()->SpawnLightComponent(pNewEntity, pComponentData);
                            pBackingMem->Components.push_back(pComponent);
                        }
                    }
                }

                // Add mesh component
                if (MeshComponentMgr::Get() != nullptr && pGLTFData->pLoadedContentRep->Meshes.size() > 0)
                {
                    auto meshIt = node.find("mesh");
                    if (meshIt != node.end())
                    {
                        const int meshIndex = meshIt->get<int>();
                        CauldronAssert(ASSERT_ERROR, meshIndex >= 0 && meshIndex < pGLTFData->pLoadedContentRep->Meshes.size(), L"Referenced mesh out of bounds");

                        MeshComponentData* pComponentData = new MeshComponentData();
                        pComponentData->pMesh = pGLTFData->pLoadedContentRep->Meshes[meshIndex];
                        const_cast<cauldron::Mesh*>(pComponentData->pMesh)->setMeshIndex(meshIndex);
                        pBackingMem->ComponentsData.push_back(pComponentData);
                        MeshComponent* pComponent = MeshComponentMgr::Get()->SpawnMeshComponent(pNewEntity, pComponentData);
                        pBackingMem->Components.push_back(pComponent);
                    }
                }



                // Add animation component (if present)
                if (AnimationComponentMgr::Get != nullptr && pGLTFData->pLoadedContentRep->Animations.size() > 0)
                {
                    bool isSkiningTarget = node.find("skin") != node.end();

                    // if node is the target of an animation, or is in a subtree of an animated node: attach the animation component
                    if (isAnimationTarget(glTFData, nodeIndex) || entityInAnimatedSubTree(pParentEntity) || isSkiningTarget)
                    {
                        AnimationComponentData* pComponentData = new AnimationComponentData();
                        pComponentData->m_pAnimRef             = &(pGLTFData->pLoadedContentRep->Animations);
                        pComponentData->m_nodeId               = nodeIndex;
                        pComponentData->m_modelId              = modelIndex;
                        pComponentData->m_skinId               = isSkiningTarget ? node["skin"].get<int>() : -1;

                        pBackingMem->ComponentsData.push_back(pComponentData);

                        AnimationComponent* pComponent = AnimationComponentMgr::Get()->SpawnAnimationComponent(pNewEntity, pComponentData);

                        // Skinning and the Mesh component are available
                        if (pComponentData->m_skinId != -1 && MeshComponentMgr::Get() != nullptr && pGLTFData->pLoadedContentRep->Meshes.size() > 0)
                        {
                            Component*  pMeshComponent = pBackingMem->Components.back();
                            const Mesh* pMesh          = reinterpret_cast<MeshComponent*>(pMeshComponent)->GetData().pMesh;

                            InitSkinningData(pMesh, pComponentData);
                        }

                        pComponent->SetLocalTransform(transform);
                        pBackingMem->Components.push_back(pComponent);
                    }
                }

                // Add camera component
                if (CameraComponentMgr::Get() != nullptr && pGLTFData->CameraData.size() > 0)
                {
                    auto cameraIt = node.find("camera");
                    if (cameraIt != node.end())
                    {
                        const int cameraIndex = cameraIt->get<int>();
                        CauldronAssert(ASSERT_ERROR, cameraIndex >= 0 && cameraIndex < pGLTFData->CameraData.size(), L"Referenced camera out of bounds");

                        CameraComponentData* pComponentData = new CameraComponentData(pGLTFData->CameraData[cameraIndex]);
                        pBackingMem->ComponentsData.push_back(pComponentData);
                        CameraComponent* pComponent = CameraComponentMgr::Get()->SpawnCameraComponent(pNewEntity, pComponentData);
                        pBackingMem->Components.push_back(pComponent);

                        static const CauldronConfig* pConfig = GetConfig();
                        if (!pGLTFData->pLoadedContentRep->ActiveCamera)
                        {
                            // If we've requested a specific camera be set to default, see if we have a match
                            if (!pConfig->StartupContent.Camera.empty())
                            {
                                if (pConfig->StartupContent.Camera == pNewEntity->GetName())
                                {
                                    pGLTFData->pLoadedContentRep->ActiveCamera = pNewEntity;
                                }
                            }
                            else
                            {
                                // Set the first camera we encounter as the "Active" one
                                pGLTFData->pLoadedContentRep->ActiveCamera = pNewEntity;
                            }
                        }

                    }
                }

                // Process any children it has
                if (node.find("children") != node.end())
                {
                    const json& childNodes = node["children"];
                    for (size_t child = 0; child < childNodes.size(); ++child)
                        processNodeRecursive(childNodes[child], pNewEntity, pBackingMem);
                }
            };

            // Iterate scenes to load
            for (size_t sceneID = 0; sceneID < scenes.size(); ++sceneID)
            {
                auto scene = scenes[sceneID];
                if (scene.find("nodes") != scene.end())
                {
                    const json& sceneNodes = scene["nodes"];
                    for (size_t node = 0; node < sceneNodes.size(); ++node)
                    {
                        uint32_t nodeIndex = sceneNodes[node];
                        processNodeRecursive(nodeIndex, nullptr, nullptr);
                    }
                }
            }

            // Update the component manager with skinning information
            if (AnimationComponentMgr::Get != nullptr && pGLTFData->pLoadedContentRep->Animations.size() > 0)
            {
                const auto& skins = pGLTFData->pLoadedContentRep->Skins;

                AnimationComponentMgr::Get()->m_skinningData.insert({modelIndex, {}});
                AnimationComponentMgr::Get()->m_skinningData[modelIndex].m_SkinningMatrices.resize(skins.size());
                AnimationComponentMgr::Get()->m_skinningData[modelIndex].m_pSkins = &skins;

                for (size_t i = 0; i < skins.size(); ++i)
                {
                    AnimationComponentMgr::Get()->m_skinningData[modelIndex].m_SkinningMatrices[i].resize(skins[i]->m_jointsNodeIdx.size());
                }
            }

            ++modelIndex;
        }

        // Process Bottom Level Acceleration Structures
        if (GetFramework()->GetConfig()->BuildRayTracingAccelerationStructure)
            BuildBLAS(pGLTFData->pLoadedContentRep->Meshes);

        GetContentManager()->StartManagingContent(pGLTFData->GLTFFileName, pGLTFData->pLoadedContentRep);

        std::chrono::nanoseconds endLoad = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch());
        std::chrono::nanoseconds loadDuration = endLoad - pGLTFData->loadStartTime;
        float loadTime = loadDuration.count() * 0.000000001f;
        Log::Write(LOGLEVEL_TRACE, L"GLTF file %ls took %f seconds to load.", pGLTFData->GLTFFileName.c_str(), loadTime);

        // Clear out the gltfContent rep memory
        delete pGLTFData;
    }

} // namespace cauldron
