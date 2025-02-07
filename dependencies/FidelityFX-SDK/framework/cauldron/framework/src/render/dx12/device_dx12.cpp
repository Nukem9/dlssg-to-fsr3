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

#if defined(_DX12)

#include "core/framework.h"
#include "misc/assert.h"

#include "render/dx12/commandlist_dx12.h"
#include "render/dx12/device_dx12.h"
#include "render/dx12/swapchain_dx12.h"
#include "render/dx12/texture_dx12.h"
#include "render/dx12/uploadheap_dx12.h"

#include "dxheaders/include/directx/d3dx12.h"
#include "antilag2/ffx_antilag2_dx12.h"

#pragma comment(lib, "dxguid.lib")
#include <DXGIDebug.h>

// D3D12SDKVersion needs to line up with the version number on Microsoft's DirectX12 Agility SDK Download page
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

namespace cauldron
{
    // To track memory leaks in COM
    void ReportLiveObjects()
    {
        MSComPtr<IDXGIDebug1> pDxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDxgiDebug))))
            pDxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
    }

    ShaderModel DXToCauldronShaderModel(D3D_SHADER_MODEL dxSM)
    {
        switch (dxSM)
        {
        case D3D_SHADER_MODEL_5_1: return ShaderModel::SM5_1;
        case D3D_SHADER_MODEL_6_0: return ShaderModel::SM6_0;
        case D3D_SHADER_MODEL_6_1: return ShaderModel::SM6_1;
        case D3D_SHADER_MODEL_6_2: return ShaderModel::SM6_2;
        case D3D_SHADER_MODEL_6_3: return ShaderModel::SM6_3;
        case D3D_SHADER_MODEL_6_4: return ShaderModel::SM6_4;
        case D3D_SHADER_MODEL_6_5: return ShaderModel::SM6_5;
        case D3D_SHADER_MODEL_6_6: return ShaderModel::SM6_6;
        case D3D_SHADER_MODEL_6_7: return ShaderModel::SM6_7;
        case D3D_SHADER_MODEL_6_8: return ShaderModel::SM6_8;
        default:
            CauldronError(L"device_dx12::DxToCauldronShaderModel: Unsupported ShaderModel detected. Please add it.");
            return ShaderModel::SM5_1;
        }
    }

    D3D12_COMMAND_LIST_TYPE QueueTypeToCommandListType(CommandQueue queueType)
    {
        switch (queueType)
        {
        case CommandQueue::Graphics:
            return D3D12_COMMAND_LIST_TYPE_DIRECT;
        case CommandQueue::Compute:
            return D3D12_COMMAND_LIST_TYPE_COMPUTE;
        case CommandQueue::Copy:
        default:
            return D3D12_COMMAND_LIST_TYPE_COPY;
        }
    }

    Device* Device::CreateDevice()
    {
        return new DeviceInternal();
    }

    DeviceInternal::DeviceInternal() :
        Device()
    {
        // Will need config settings to initialize the device
        const CauldronConfig* pConfig = GetConfig();

        // Enable the D3D12 debug layer
        bool validationEnabled = pConfig->CPUValidationEnabled || pConfig->GPUValidationEnabled;

        // Note that it turns out the validation and debug layer are known to cause
        // deadlocks in certain circumstances, for example when the vsync interval
        // is 0 and full screen is used
        if (validationEnabled)
        {
            MSComPtr<ID3D12Debug1> pDebugController1;
            CauldronThrowOnFail(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController1)));

            pDebugController1->EnableDebugLayer();    // CPU Only (but also needed for GPU)
            pDebugController1->SetEnableGPUBasedValidation(pConfig->GPUValidationEnabled); // GPU validation
        }

        // Adapter initialization
        UINT factoryFlags = 0;
        if (validationEnabled)
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

        MSComPtr<IDXGIFactory> pFactory;
        MSComPtr<IDXGIFactory6> pFactory6;
        CauldronThrowOnFail(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&pFactory)));

        // Try to get Factory6 in order to use EnumAdapterByGpuPreference method. If it fails, fall back to regular EnumAdapters.
        if (S_OK == pFactory->QueryInterface(IID_PPV_ARGS(&pFactory6)))
            CauldronThrowOnFail(pFactory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_pAdapter)));

        else
            CauldronThrowOnFail(pFactory->EnumAdapters(0, &m_pAdapter));


        // Init the device
        InitDevice();

        // Set stable power state if requested (only works when Windows Developer Mode is enabled)
        if (pConfig->StablePowerState)
        {
            if (FAILED(m_pDevice->SetStablePowerState(true)))
            {
                // Handle failure / device removal
                HRESULT reason = m_pDevice->GetDeviceRemovedReason();
                CauldronError(L"Error: ID3D12Device::SetStablePowerState(true) failed:");
                CauldronError((const wchar_t*)_com_error(reason).ErrorMessage());

                // Override power state request to not crash again
                CauldronConfig* pModifiedConfig = const_cast<CauldronConfig*>(pConfig);
                pModifiedConfig->StablePowerState = false;

                // Release device and re-init
                if (m_pAGSContext)
                {
                    agsDriverExtensionsDX12_DestroyDevice(m_pAGSContext, m_pDevice, nullptr);
                    agsDeInitialize(m_pAGSContext);
                }
                else
                {
                    // Can't use Com for device due to AGS, so just release it manually
                    if (m_pDevice)
                        m_pDevice->Release();
                }

                // Need to re-init the device
                InitDevice();
            }
        }

        // Setup the D3D12 Memory Allocator (we will use this to back resources -- outside of the swapchain resources)
        D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
        allocatorDesc.pDevice = m_pDevice;
        allocatorDesc.pAdapter = m_pAdapter.Get();
        //AllocatorDesc.Flags = ??;
        //AllocatorDesc.PreferredBlockSize = ??;
        CauldronAssert(ASSERT_CRITICAL, !FAILED(D3D12MA::CreateAllocator(&allocatorDesc, &m_pD3D12Allocator)), L"Could not allocator D3D12MemoryAllocator. Terminating application");

        // Check for various support checks
        CD3DX12FeatureSupport features;
        HRESULT featureSupportInitResult = features.Init(m_pDevice);
        CauldronAssert(ASSERT_ERROR, !FAILED(featureSupportInitResult), L"Could not init feature support check.");

        // Committed is always present for D3D12
        m_SupportedFeatures |= DeviceFeature::DedicatedAllocs;
        // Shader storage buffer array non uniform indexing is always available for D3D12
        m_SupportedFeatures |= DeviceFeature::ShaderStorageBufferArrayNonUniformIndexing;
        // DeviceFeature_CoherentMemoryAMD and DeviceFeature_BufferMarkerAMD are only present in Vulkan
        // Same currently with DeviceFeature_ExtendedSynch but can be repurposed for later extensions

        if (!FAILED(featureSupportInitResult))
        {
            // FP16 support
            if (features.MinPrecisionSupport() & D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT)
                m_SupportedFeatures |= DeviceFeature::FP16;

            // Check if requested
            CauldronAssert(ASSERT_WARNING, !pConfig->FP16 || (static_cast<bool>(m_SupportedFeatures & DeviceFeature::FP16) && pConfig->FP16), L"FP16 support requested but unsupported on this device.");

            // VRS support
            if (features.VariableShadingRateTier() >= D3D12_VARIABLE_SHADING_RATE_TIER_1)
                m_SupportedFeatures |= DeviceFeature::VRSTier1;
            if (features.VariableShadingRateTier() >= D3D12_VARIABLE_SHADING_RATE_TIER_2)
                m_SupportedFeatures |= DeviceFeature::VRSTier2;

            // Check if requested
            CauldronAssert(ASSERT_WARNING, !pConfig->VRSTier1 || (static_cast<bool>(m_SupportedFeatures & DeviceFeature::VRSTier1) && pConfig->VRSTier1), L"VRS Tier1 support requested but unsupported on this device.");
            CauldronAssert(ASSERT_WARNING, !pConfig->VRSTier2 || (static_cast<bool>(m_SupportedFeatures & DeviceFeature::VRSTier2) && pConfig->VRSTier2), L"VRS Tier2 support requested but unsupported on this device.");

            // RT support
            if (features.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_0)
                m_SupportedFeatures |= DeviceFeature::RT_1_0;
            if (features.RaytracingTier() >= D3D12_RAYTRACING_TIER_1_1)
                m_SupportedFeatures |= DeviceFeature::RT_1_1;

            // Check if requested
            CauldronAssert(ASSERT_WARNING, !pConfig->RT_1_0 || (static_cast<bool>(m_SupportedFeatures & DeviceFeature::RT_1_0) && pConfig->RT_1_0), L"DXR 1.0 support requested but unsupported on this device.");
            CauldronAssert(ASSERT_WARNING, !pConfig->RT_1_1 || (static_cast<bool>(m_SupportedFeatures & DeviceFeature::RT_1_1) && pConfig->RT_1_1), L"DXR 1.1 support requested but unsupported on this device.");

            // Get max shader version support
            m_MaxSupportedShaderModel = DXToCauldronShaderModel(features.HighestShaderModel());
            CauldronAssert(ASSERT_WARNING, pConfig->MinShaderModel <= m_MaxSupportedShaderModel, L"This device does not support the minimum requested ShaderModel.");

            // Check if wave size control support
            if (features.HighestShaderModel() >= D3D_SHADER_MODEL::D3D_SHADER_MODEL_6_6)
                m_SupportedFeatures |= DeviceFeature::WaveSize;

            // Get min/max wave lane counts
            m_MaxWaveLaneCount = features.WaveLaneCountMax();
            m_MinWaveLaneCount = features.WaveLaneCountMin();
        }

        // Init the rest of the device class

        // Allocate the queue primitives
        auto initQueue = [this](CommandQueue queueType, const wchar_t* name) {
            InitQueueSyncPrim(queueType, m_QueueSyncPrims[static_cast<int32_t>(queueType)], name);
        };
        initQueue(CommandQueue::Graphics, L"CauldronGraphicsQueue");
        initQueue(CommandQueue::Compute,  L"CauldronComputeQueue");
        initQueue(CommandQueue::Copy,     L"CauldronCopyQueue");

        if (pConfig->AntiLag2)
        {
            if (AMD::AntiLag2DX12::Initialize(&m_AntiLag2Context, m_pDevice) == S_OK)
            {
                m_AntiLag2Supported = true;
                m_AntiLag2Enabled   = true;
            }
        }
    }

    DeviceInternal::~DeviceInternal()
    {
        // Make sure everything is clear
        FlushAllCommandQueues();

        AMD::AntiLag2DX12::DeInitialize(&m_AntiLag2Context);

        // Clear queue allocators
        for (uint32_t i = 0; i < static_cast<int32_t>(CommandQueue::Count); ++i)
        {
            MSComPtr<ID3D12CommandAllocator> pCmdAllocator;
            while (m_QueueSyncPrims[i].m_AvailableQueueAllocators.PopFront(pCmdAllocator)) {}
        }

        // Must be released right before releasing D3D12 device.
        m_pD3D12Allocator.Reset();

        // Release device
        if (m_pAGSContext)
        {
            agsDriverExtensionsDX12_DestroyDevice(m_pAGSContext, m_pDevice, nullptr);
            agsDeInitialize(m_pAGSContext);
        }
        else
        {
            // Can't use Com for device due to AGS, so just release it manually
            if (m_pDevice)
                m_pDevice->Release();
        }

        // Report live objects in debug to make sure we didn't forget to clean up anything
        if (GetConfig()->DeveloperMode == true) {
            atexit(&ReportLiveObjects);
        }
    }

    void DeviceInternal::InitQueueSyncPrim(CommandQueue queueType, QueueSyncPrimitive& queueSyncPrim, const wchar_t* queueName)
    {
        D3D12_COMMAND_LIST_TYPE  commandListType = QueueTypeToCommandListType(queueType);
        D3D12_COMMAND_QUEUE_DESC queueDesc       = {commandListType, D3D12_COMMAND_QUEUE_PRIORITY_HIGH, D3D12_COMMAND_QUEUE_FLAG_NONE, 0};

        // Create the queue
        int32_t queueIndex = static_cast<int32_t>(queueType);
        m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queueSyncPrim.m_pQueue));
        CauldronAssert(ASSERT_CRITICAL, queueSyncPrim.m_pQueue, L"Could not create required command queue!");

        // Set the queue name
        queueSyncPrim.m_pQueue->SetName(queueName);

        // Create a fence for the queue
        m_pDevice->CreateFence(1, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&queueSyncPrim.m_pQueueFence));
        CauldronAssert(ASSERT_CRITICAL, queueSyncPrim.m_pQueueFence, L"Could not create required command queue fence!");
    }

    void DeviceInternal::CreateSwapChain(SwapChain*& pSwapChain, const SwapChainCreationParams& params, CommandQueue queueType)
    {
        // Store for later usage if needed
        pSwapChain->GetImpl()->m_CreationQueue = queueType;

        // Create base swap chain
        MSComPtr<IDXGISwapChain1> pSwapChain1;
        CauldronThrowOnFail(params.pFactory->CreateSwapChainForHwnd(m_QueueSyncPrims[static_cast<uint32_t>(queueType)].m_pQueue.Get(),
                            params.WndHandle,
                            &params.DX12Desc, nullptr, nullptr, &pSwapChain1));

        // Request to ignore ALT-ENTER (we will control it ourselves). Error on fail, but can keep running.
        CauldronErrorOnFail(params.pFactory->MakeWindowAssociation(params.WndHandle, DXGI_MWA_NO_ALT_ENTER));

        // Get the SwapChain4 interface that we want
        CauldronThrowOnFail(pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain4), (void**)&pSwapChain->GetImpl()->m_pSwapChain));
    }

    uint64_t DeviceInternal::PresentSwapChain(SwapChain* pSwapChain)
    {
        if (m_AntiLag2Supported)
        {
            // {5083ae5b-8070-4fca-8ee5-3582dd367d13}
            static const GUID IID_IFfxAntiLag2Data = {0x5083ae5b, 0x8070, 0x4fca, {0x8e, 0xe5, 0x35, 0x82, 0xdd, 0x36, 0x7d, 0x13}};

            struct AntiLag2Data
            {
                AMD::AntiLag2DX12::Context* context;
                bool                        enabled;
            } data;

            data.context = &m_AntiLag2Context;
            data.enabled = m_AntiLag2Enabled;
            pSwapChain->GetImpl()->GetImpl()->DX12SwapChain()->SetPrivateData(IID_IFfxAntiLag2Data, sizeof(data), &data);

            if (m_AntiLag2Enabled)
            {
                AMD::AntiLag2DX12::MarkEndOfFrameRendering(&m_AntiLag2Context);
            }
        }

        HRESULT hrCode = S_OK;
        if (pSwapChain->GetImpl()->m_VSyncEnabled)
            hrCode = pSwapChain->GetImpl()->m_pSwapChain->Present(1, 0);
        else
            hrCode = pSwapChain->GetImpl()->m_pSwapChain->Present(0, pSwapChain->GetImpl()->m_TearingSupported ? DXGI_PRESENT_ALLOW_TEARING : 0);

        if ((hrCode == DXGI_ERROR_DEVICE_REMOVED || hrCode == DXGI_ERROR_DEVICE_RESET || hrCode == DXGI_ERROR_DEVICE_HUNG) && m_DeviceRemovedCallback)
            m_DeviceRemovedCallback(m_DeviceRemovedCustomData);
        CauldronThrowOnFail(hrCode);

        uint32_t queueID = static_cast<uint32_t>(pSwapChain->GetImpl()->m_CreationQueue);
        uint64_t signalValue;
        {
            std::unique_lock<std::mutex>    critSection(m_QueueSyncPrims[queueID].m_QueueAccessMutex);
            signalValue = ++m_QueueSyncPrims[queueID].m_QueueSignalValue;
            m_QueueSyncPrims[queueID].m_pQueue->Signal(m_QueueSyncPrims[queueID].m_pQueueFence.Get(), signalValue);
        }

        return signalValue;
    };

    uint64_t DeviceInternal::SignalQueue(CommandQueue queueType)
    {
        int32_t  queueID = static_cast<int32_t>(queueType);
        uint64_t signalValue;
        {
            std::unique_lock<std::mutex> critSection(m_QueueSyncPrims[queueID].m_QueueAccessMutex);
            signalValue = ++m_QueueSyncPrims[queueID].m_QueueSignalValue;
            m_QueueSyncPrims[queueID].m_pQueue->Signal(m_QueueSyncPrims[queueID].m_pQueueFence.Get(), signalValue);
        }
        return signalValue;
    }

    uint64_t DeviceInternal::QueryLastCompletedValue(CommandQueue queueType)
    {
        int32_t  queueID = static_cast<int32_t>(queueType);
        uint64_t lastCompletedValue = 0;
        {
            std::unique_lock<std::mutex> critSection(m_QueueSyncPrims[queueID].m_QueueAccessMutex);
            lastCompletedValue = m_QueueSyncPrims[queueID].m_pQueueFence->GetCompletedValue();
        }
        return lastCompletedValue;
    }

    void DeviceInternal::WaitOnQueue(uint64_t waitValue, CommandQueue queueType) const
    {
        m_QueueSyncPrims[static_cast<int32_t>(queueType)].Wait(waitValue);
    }

    void DeviceInternal::InitDevice()
    {
        // Will need config settings to initialize the device
        const CauldronConfig* pConfig = GetConfig();

        // Enable the D3D12 debug layer
        bool validationEnabled = pConfig->CPUValidationEnabled || pConfig->GPUValidationEnabled;

        // Query if we are on an AMD GPU
        DXGI_ADAPTER_DESC adapterDesc;
        m_pAdapter->GetDesc(&adapterDesc);
        const bool bAMDGPU = (adapterDesc.VendorId == 0x1002);

        // Store the device name
        m_DeviceName = adapterDesc.Description;

        // And graphics API
        m_GraphicsAPIShort   = L"DX12";
        m_GraphicsAPIPretty  = L"DirectX 12 AgilitySDK";
        m_GraphicsAPIVersion = L"v." + std::to_wstring(D3D12SDKVersion);
        m_GraphicsAPI        = m_GraphicsAPIPretty + L" " + m_GraphicsAPIVersion;

        // Create an AGS Device
        if (bAMDGPU && pConfig->AGSEnabled)
        {
            AGSReturnCode result = agsInitialize(AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH), nullptr, &m_pAGSContext, &m_AGSGPUInfo);
            if (result == AGS_SUCCESS)
            {
                AGSDX12DeviceCreationParams creationParams = {};
                creationParams.pAdapter = m_pAdapter.Get();
                creationParams.iid = __uuidof(m_pDevice);
                creationParams.FeatureLevel = D3D_FEATURE_LEVEL_12_0;

                AGSDX12ExtensionParams extensionParams = {};
                AGSDX12ReturnedParams returnedParams = {};

                // Create AGS Device
                AGSReturnCode rc = agsDriverExtensionsDX12_CreateDevice(m_pAGSContext, &creationParams, &extensionParams, &returnedParams);
                if (rc == AGS_SUCCESS)
                    m_pDevice = returnedParams.pDevice;

                // Check whether user markers are supported by the current driver
                if (returnedParams.extensionsSupported.userMarkers == 1)
                {
                    Log::Write(LOGLEVEL_INFO, L"AGS_DX12_EXTENSION_USER_MARKERS are supported.");
                }
                else
                {
                    Log::Write(LOGLEVEL_INFO, L"AGS_DX12_EXTENSION_USER_MARKERS are NOT supported.");
                }

                // Store the driver version
                m_DriverVersion = StringToWString(m_AGSGPUInfo.driverVersion);
            }
        }

        // If the AGS device wasn't created then try using a regular device
        if (!m_pDevice)
        {
            CauldronThrowOnFail(D3D12CreateDevice(m_pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_pDevice)));

            // If we are running with validation enabled, also enable break on validation errors
            if (validationEnabled)
            {
                MSComPtr<ID3D12InfoQueue> pInfoQueue;
                if (m_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)) == S_OK)
                {
                    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
                    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
                }
            }

            // No driver version without AGS
            m_DriverVersion = L"Enable AGS for Driver Version";
        }

        // Make sure we got a device
        CauldronAssert(ASSERT_CRITICAL, m_pDevice != nullptr, L"Could not create device.");
        m_pDevice->SetName(L"CauldronDevice");  // Give it a useful name
    }

    MSComPtr<ID3D12CommandAllocator> DeviceInternal::GetAllocator(CommandQueue queueType, QueueSyncPrimitive& queueSyncPrim, const wchar_t* allocatorName)
    {
        // Start by getting an allocator to create a temporary command list with (thread-safe)
        MSComPtr<ID3D12CommandAllocator> pCmdAllocator;
        if (queueSyncPrim.m_AvailableQueueAllocators.PopFront(pCmdAllocator))
        {
            pCmdAllocator->Reset();  // Reset allocator before re-using it
        }
        else
        {
            // Create a new one if we couldn't find an allocator
            D3D12_COMMAND_LIST_TYPE queueListType = QueueTypeToCommandListType(queueType);
            m_pDevice->CreateCommandAllocator(queueListType, IID_PPV_ARGS(&pCmdAllocator));
            CauldronAssert(ASSERT_CRITICAL, pCmdAllocator, L"Could not create a command allocator %ls.", allocatorName);
            pCmdAllocator->SetName(allocatorName);
        }

        return pCmdAllocator;
    }

    void DeviceInternal::GetFeatureInfo(DeviceFeature feature, void* pFeatureInfo)
    {
        switch (feature)
        {
        case cauldron::DeviceFeature::FP16:
            break;
        case cauldron::DeviceFeature::VRSTier1:
        case cauldron::DeviceFeature::VRSTier2:
        {
            D3D12_FEATURE_DATA_D3D12_OPTIONS6 vrsInfo = {};
            m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &vrsInfo, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS6));

            uint32_t* pNumShadingRates = &((FeatureInfo_VRS*)pFeatureInfo)->NumShadingRates;

            ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_1X1;
            ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_1X2;
            ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_2X1;
            ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_2X2;

            ((FeatureInfo_VRS*)pFeatureInfo)->Combiners = ShadingRateCombiner::ShadingRateCombiner_Passthrough |
                                                          ShadingRateCombiner::ShadingRateCombiner_Override | ShadingRateCombiner::ShadingRateCombiner_Min |
                                                          ShadingRateCombiner::ShadingRateCombiner_Max | ShadingRateCombiner::ShadingRateCombiner_Sum;

            ((FeatureInfo_VRS*)pFeatureInfo)->AdditionalShadingRatesSupported = vrsInfo.AdditionalShadingRatesSupported;

            if (vrsInfo.AdditionalShadingRatesSupported)
            {
                ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_2X4;
                ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_4X2;
                ((FeatureInfo_VRS*)pFeatureInfo)->ShadingRates[(*pNumShadingRates)++] = ShadingRate::ShadingRate_4X4;
            }

            if (static_cast<bool>(feature & cauldron::DeviceFeature::VRSTier2))
            {
                ((FeatureInfo_VRS*)pFeatureInfo)->MinTileSize[0] = vrsInfo.ShadingRateImageTileSize;
                ((FeatureInfo_VRS*)pFeatureInfo)->MinTileSize[1] = vrsInfo.ShadingRateImageTileSize;
                ((FeatureInfo_VRS*)pFeatureInfo)->MaxTileSize[0] = vrsInfo.ShadingRateImageTileSize;
                ((FeatureInfo_VRS*)pFeatureInfo)->MaxTileSize[1] = vrsInfo.ShadingRateImageTileSize;
            }
            break;
        }
        case cauldron::DeviceFeature::RT_1_0:
            break;
        case cauldron::DeviceFeature::RT_1_1:
            break;
        case cauldron::DeviceFeature::WaveSize:
            break;
        default:
            break;
        }
    }

    void DeviceInternal::FlushQueue(CommandQueue queueType)
    {
        uint64_t signalValue;
        {
            int32_t queueID = static_cast<int32_t>(queueType);
            std::unique_lock<std::mutex>    critSection(m_QueueSyncPrims[queueID].m_QueueAccessMutex);
            signalValue = ++m_QueueSyncPrims[queueID].m_QueueSignalValue;
            m_QueueSyncPrims[queueID].m_pQueue->Signal(m_QueueSyncPrims[queueID].m_pQueueFence.Get(), signalValue);
        }

        // Wait for the signal to be raised (queue to last command)
        WaitOnQueue(signalValue, queueType);
    }

    uint64_t DeviceInternal::QueryPerformanceFrequency(CommandQueue queueType)
    {
        uint64_t frequency = 0;

        CauldronAssert(ASSERT_ERROR, queueType == CommandQueue::Compute || queueType == CommandQueue::Graphics, L"Querying performance frequency on invalid device queue. Crash likely.");
        CauldronAssert(ASSERT_CRITICAL, m_QueueSyncPrims[static_cast<int32_t>(queueType)].m_pQueue, L"Trying to access a null queue. We be crashing now!");

        m_QueueSyncPrims[static_cast<int32_t>(queueType)].m_pQueue->GetTimestampFrequency(&frequency);
        return frequency;
    }

    CommandList* DeviceInternal::CreateCommandList(const wchar_t* name, CommandQueue queueType)
    {
        int32_t queueID = static_cast<int32_t>(queueType);
        switch (queueType)
        {
        case CommandQueue::Graphics:
            return CreateCommandList(name, queueType, m_QueueSyncPrims[queueID], L"CauldronGraphicsAllocator");
        case CommandQueue::Compute:
            return CreateCommandList(name, queueType, m_QueueSyncPrims[queueID], L"CauldronComputeAllocator");
        case CommandQueue::Copy:
            return CreateCommandList(name, queueType, m_QueueSyncPrims[queueID], L"CauldronCopyAllocator");
        default:
            CauldronCritical(L"Cannot call CreateCommandList for unknown queue type");
            return nullptr;
        }
    }

    CommandList* DeviceInternal::CreateCommandList(const wchar_t* name, CommandQueue queueType, QueueSyncPrimitive& queueSyncPrim, const wchar_t* allocatorName)
    {
        D3D12_COMMAND_LIST_TYPE queueListType = QueueTypeToCommandListType(queueType);

        // Create a new command list for this frame that will be backed by the appropriate allocator
        MSComPtr<ID3D12GraphicsCommandList2> pCmdList;
        MSComPtr<ID3D12CommandAllocator>     pCmdAllocator = GetAllocator(queueType, queueSyncPrim, allocatorName);

        m_pDevice->CreateCommandList(0, queueListType, pCmdAllocator.Get(), nullptr, IID_PPV_ARGS(&pCmdList));
        CauldronAssert(ASSERT_CRITICAL, pCmdList, L"Could not create D3D command list %ls.", name);

        CommandListInitParams initParams = { pCmdList, pCmdAllocator };

        CommandList* pCauldronCmdList = CommandList::CreateCommandList(name, queueType, &initParams);
        CauldronAssert(ASSERT_CRITICAL, pCauldronCmdList, L"Could not create Cauldron command list %ls.", name);

        return pCauldronCmdList;
    }

    void DeviceInternal::ReleaseCommandAllocator(CommandList* pCmdList)
    {
        int32_t queueID = static_cast<int32_t>(pCmdList->GetQueueType());
        m_QueueSyncPrims[queueID].m_AvailableQueueAllocators.PushBack(pCmdList->GetImpl()->DX12ComAllocator());
    }

    void DeviceInternal::ExecuteResourceTransitionImmediate(uint32_t barrierCount, const Barrier* pBarriers)
    {
        // Make sure any copying is being done on secondary threads
        CauldronAssert(ASSERT_ERROR, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Do not issue immediate resource transition commands on the main thread after initialization is complete as this will be a blocking operation.");

        CommandList* pImmediateCmdList = CreateCommandList(L"TransitionCmdList", CommandQueue::Graphics);

        // Enqueue the barriers and close the command list
        ResourceBarrier(pImmediateCmdList, barrierCount, pBarriers);
        CloseCmdList(pImmediateCmdList);

        // Execute and sync
        std::vector<CommandList*> cmdLists;
        cmdLists.push_back(pImmediateCmdList);
        ExecuteCommandListsImmediate(cmdLists, m_QueueSyncPrims[(uint32_t)CommandQueue::Graphics]);

        // No longer needed, will release allocator on destruction
        delete pImmediateCmdList;
    }

    void DeviceInternal::ExecuteTextureResourceCopyImmediate(uint32_t resourceCopyCount, const TextureCopyDesc* pCopyDescs)
    {
        static int32_t sCopyQueueID = static_cast<int32_t>(CommandQueue::Copy);

        // Make sure any copying is being done on secondary threads
        CauldronAssert(ASSERT_ERROR, std::this_thread::get_id() != GetFramework()->MainThreadID() || !GetFramework()->IsRunning(), L"Do not issue loaded resource copy commands on the main thread as this will be a blocking operation.");

        CommandList* pImmediateCmdList = CreateCommandList(L"TextureCopyCmdList", CommandQueue::Copy);

        // Enqueue the barriers and close the command list
        for (uint32_t i = 0; i < resourceCopyCount; ++i)
            CopyTextureRegion(pImmediateCmdList, &pCopyDescs[i]);
        CloseCmdList(pImmediateCmdList);

        // Execute and sync
        std::vector<CommandList*> cmdLists;
        cmdLists.push_back(pImmediateCmdList);
        ExecuteCommandListsImmediate(cmdLists, CommandQueue::Copy);

        // No longer needed, will release allocator on destruction
        delete pImmediateCmdList;
    }

    uint64_t DeviceInternal::ExecuteCommandLists(std::vector<CommandList*>& cmdLists, CommandQueue queueType, bool isFirstSubmissionOfFrame, bool isLastSubmissionOfFrame)
    {
        std::vector<ID3D12CommandList*> d3dCommandList;
        std::vector<CommandList*>::iterator it = cmdLists.begin();
        while (it != cmdLists.end())
        {
            d3dCommandList.push_back((*it)->GetImpl()->DX12CmdList());
            ++it;
        }

        // Get an ID to wait on so we know when work is processed, call execute command list within the critical section to enforce submission order
        int32_t queueID = static_cast<int32_t>(queueType);
        uint64_t signalValue;
        {
            std::unique_lock<std::mutex>    critSection(m_QueueSyncPrims[queueID].m_QueueAccessMutex);
            m_QueueSyncPrims[static_cast<int32_t>(queueType)].m_pQueue->ExecuteCommandLists(static_cast<UINT>(d3dCommandList.size()), d3dCommandList.data());
            signalValue = ++m_QueueSyncPrims[queueID].m_QueueSignalValue;
            m_QueueSyncPrims[queueID].m_pQueue->Signal(m_QueueSyncPrims[queueID].m_pQueueFence.Get(), signalValue);
        }

        return signalValue;
    }

    void DeviceInternal::ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, CommandQueue queueType)
    {
        ExecuteCommandListsImmediate(cmdLists, m_QueueSyncPrims[static_cast<int32_t>(queueType)]);
    }

    void DeviceInternal::ExecuteCommandListsImmediate(std::vector<CommandList*>& cmdLists, QueueSyncPrimitive& queueSyncPrim)
    {
        std::vector<ID3D12CommandList*>     d3dCommandList;
        std::vector<CommandList*>::iterator it = cmdLists.begin();
        while (it != cmdLists.end())
        {
            d3dCommandList.push_back((*it)->GetImpl()->DX12CmdList());
            ++it;
        }

        // Flush the queue and wait for it to be done
        uint64_t signalValue;
        {
            std::unique_lock<std::mutex> critSection(queueSyncPrim.m_QueueAccessMutex);
            queueSyncPrim.m_pQueue->ExecuteCommandLists(static_cast<UINT>(d3dCommandList.size()), d3dCommandList.data());
            signalValue = ++queueSyncPrim.m_QueueSignalValue;
            queueSyncPrim.m_pQueue->Signal(queueSyncPrim.m_pQueueFence.Get(), signalValue);
        }

        // Wait for the signal to be raised (queue to last command)
        queueSyncPrim.Wait(signalValue);
    }

    void DeviceInternal::QueueSyncPrimitive::Wait(uint64_t waitValue) const
    {
        HANDLE hHandleFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        m_pQueueFence->SetEventOnCompletion(waitValue, hHandleFenceEvent);
        WaitForSingleObject(hHandleFenceEvent, INFINITE);
        CloseHandle(hHandleFenceEvent);
    }

    void DeviceInternal::UpdateAntiLag2()
    {
        if (m_AntiLag2Supported)
        {
            AMD::AntiLag2DX12::Update(&m_AntiLag2Context, m_AntiLag2Enabled, m_AntiLag2FramerateLimiter);
        }
    }

} // namespace cauldron

#endif // #if defined(_DX12)
