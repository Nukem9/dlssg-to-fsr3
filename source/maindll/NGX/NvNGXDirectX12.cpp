#include <d3d12.h>
#include <dxgi.h>
#include "FFFrameInterpolatorDX.h"
#include "NvNGX.h"

typedef LONG NTSTATUS;
#include <d3dkmthk.h>

static std::shared_mutex FeatureInstanceHandleLock;
static std::unordered_map<uint32_t, std::shared_ptr<FFFrameInterpolatorDX>> FeatureInstanceHandles;

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_CreateFeature(
	ID3D12CommandList *CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!CommandList || !Parameters || !OutInstanceHandle)
		return NGX_INVALID_PARAMETER;

	// Grab the device from the command list
	ID3D12Device *device = nullptr;

	if (FAILED(CommandList->GetDevice(IID_PPV_ARGS(&device))))
		return 0xBAD00002;

	// Grab NGX parameters from sl.dlss_g.dll
	// https://forums.developer.nvidia.com/t/using-dlssg-without-idxgiswapchain-present/247260/8
	Parameters->Set4("DLSSG.MustCallEval", 1);

	uint32_t swapchainWidth = 0;
	Parameters->Get5("Width", &swapchainWidth);

	uint32_t swapchainHeight = 0;
	Parameters->Get5("Height", &swapchainHeight);

	// Then initialize FSR
	try
	{
		auto instance = std::make_shared<FFFrameInterpolatorDX>(device, swapchainWidth, swapchainHeight, Parameters);

		std::scoped_lock lock(FeatureInstanceHandleLock);
		{
			const auto handle = NGXHandle::Allocate(11);
			*OutInstanceHandle = handle;

			FeatureInstanceHandles.emplace(handle->InternalId, std::move(instance));
		}
	}
	catch (const std::exception& e)
	{
		spdlog::error("NVSDK_NGX_D3D12_CreateFeature: Failed to initialize: {}", e.what());
		device->Release();

		return NGX_FEATURE_NOT_FOUND;
	}

	spdlog::info("NVSDK_NGX_D3D12_CreateFeature: Succeeded.");
	device->Release();

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!CommandList || !InstanceHandle || !Parameters)
		return NGX_INVALID_PARAMETER;

	const auto instance = [&]()
	{
		std::shared_lock lock(FeatureInstanceHandleLock);
		auto itr = FeatureInstanceHandles.find(InstanceHandle->InternalId);

		if (itr == FeatureInstanceHandles.end())
			return decltype(itr->second) {};

		return itr->second;
	}();

	if (!instance)
		return NGX_FEATURE_NOT_FOUND;

	const auto status = instance->Dispatch(CommandList, Parameters);

	switch (status)
	{
	case FFX_OK:
	{
		static bool once = [&]()
		{
			spdlog::info("NVSDK_NGX_D3D12_EvaluateFeature: Succeeded.");
			return true;
		}();

		return NGX_SUCCESS;
	}

	default:
	{
		static bool once = [&]()
		{
			spdlog::error("Evaluation call failed with status {:X}.", static_cast<uint32_t>(status));
			return true;
		}();

		return NGX_INVALID_PARAMETER;
	}
	}
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetFeatureRequirements(IDXGIAdapter *Adapter, void *FeatureDiscoveryInfo, NGXFeatureRequirementInfo *RequirementInfo)
{
	if (!FeatureDiscoveryInfo || !RequirementInfo)
		return NGX_INVALID_PARAMETER;

	RequirementInfo->Flags = 0;
	RequirementInfo->RequiredGPUArchitecture = NGXHardcodedArchitecture;
	strcpy_s(RequirementInfo->RequiredOperatingSystemVersion, "10.0.0");

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	if (!OutBufferSize)
		return NGX_INVALID_PARAMETER;

	*OutBufferSize = 0;
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init_Ext(void *Unknown1, const wchar_t *Path, ID3D12Device *D3DDevice, uint32_t Unknown2, NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	if (!D3DDevice)
		return NGX_INVALID_PARAMETER;

	auto isHAGSEnabled = [&]()
	{
		D3DKMT_OPENADAPTERFROMLUID openAdapter = {
			.AdapterLuid = D3DDevice->GetAdapterLuid(),
		};

		if (D3DKMTOpenAdapterFromLuid(&openAdapter) == 0)
		{
			D3DKMT_WDDM_2_7_CAPS caps = {};
			D3DKMT_QUERYADAPTERINFO info = {
				.hAdapter = openAdapter.hAdapter,
				.Type = KMTQAITYPE_WDDM_2_7_CAPS,
				.pPrivateDriverData = &caps,
				.PrivateDriverDataSize = sizeof(caps),
			};

			D3DKMTQueryAdapterInfo(&info);

			D3DKMT_CLOSEADAPTER closeAdapter = {
				.hAdapter = openAdapter.hAdapter,
			};

			D3DKMTCloseAdapter(&closeAdapter);
			return caps.HwSchEnabled != 0;
		}

		return false;
	};

	if (isHAGSEnabled())
		spdlog::info("Hardware accelerated GPU scheduling is enabled on this adapter.");
	else
		spdlog::warn("Hardware accelerated GPU scheduling is disabled on this adapter.");

	auto hasPresentMeteringAPI = [&]()
	{
		const auto handle = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

		if (!handle)
			return false;

		const auto queryInterface = reinterpret_cast<void *(__stdcall *)(uint32_t)>(GetProcAddress(handle, "nvapi_QueryInterface"));
		const auto setFlipConfig = queryInterface ? queryInterface(0xF3148C42) : nullptr;

		FreeLibrary(handle);
		return setFlipConfig != nullptr;
	};

	if (hasPresentMeteringAPI())
		spdlog::info("Present metering interface is available.");
	else
		spdlog::info("Present metering interface is unimplemented. This is not an error.");

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init(void *Unknown1, const wchar_t *Path, ID3D12Device *D3DDevice, uint32_t Unknown2)
{
	spdlog::info(__FUNCTION__);

	return NVSDK_NGX_D3D12_Init_Ext(Unknown1, Path, D3DDevice, Unknown2, nullptr);
}

static NGXResult GetCurrentSettingsCallback(NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!InstanceHandle || !Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->Set4("DLSSG.MustCallEval", 1);
	Parameters->Set4("DLSSG.BurstCaptureRunning", 0);

	return NGX_SUCCESS;
}

static NGXResult EstimateVRAMCallback(
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	size_t *EstimatedSize)
{
	// Assume 300MB
	if (EstimatedSize)
		*EstimatedSize = 300 * 1024 * 1024;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl(ID3D12Device *D3DDevice, NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	if (!D3DDevice || !Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);
	Parameters->Set5("DLSSG.MultiFrameCountMax", 1);
	Parameters->Set4("DLSSG.ReflexWarp.Available", 0);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	if (!Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);
	Parameters->Set5("DLSSG.MultiFrameCountMax", 1);
	Parameters->Set4("DLSSG.ReflexWarp.Available", 0);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_ReleaseFeature(NGXHandle *InstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!InstanceHandle)
		return NGX_INVALID_PARAMETER;

	const auto node = [&]()
	{
		std::scoped_lock lock(FeatureInstanceHandleLock);
		return FeatureInstanceHandles.extract(InstanceHandle->InternalId);
	}();

	if (node.empty())
		return NGX_FEATURE_NOT_FOUND;

	// Node is handled by RAII. Apparently, InstanceHandle isn't supposed to be deleted.
	// NGXHandle::Free(InstanceHandle);
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown()
{
	spdlog::info(__FUNCTION__);
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown1(ID3D12Device *D3DDevice)
{
	spdlog::info(__FUNCTION__);

	if (!D3DDevice)
		return NGX_INVALID_PARAMETER;

	return NGX_SUCCESS;
}
