typedef LONG NTSTATUS;
#include <d3dkmthk.h>

enum class NV_STATUS : uint32_t
{
	Success = 0,
	Error = 0xFFFFFFFF,
};

enum class NV_INTERFACE : uint32_t
{
	GPU_GetArchInfo = 0xD8265D24,
	D3D12_CreateCubinComputeShaderExV2 = 0x299F5FDC,
	D3D12_SetFlipConfig = 0xF3148C42,
	D3D12_SetRawScgPriority = 0x5DB3048A,
};

struct NV_ARCH_INFO
{
	uint32_t Version;		 // 0
	uint32_t Architecture;	 // 4
	uint32_t Implementation; // 8
	uint32_t Revision;		 // C
};

struct NV_SCG_PRIORITY_INFO
{
	void *CommandList; // 0
	uint32_t Unknown2; // 8
	uint32_t Unknown3; // C
	uint8_t Unknown4;  // 10
	uint8_t Unknown5;  // 11
	uint8_t Unknown6;  // 12
	uint8_t Unknown7;  // 13
	uint32_t Unknown8; // 14
};

struct NV_CREATE_CUBIN_SHADER_INFO
{
	uint64_t SizeOfStruct;	// 0
	uint64_t Unknown1;		// 8
	void *Unknown2;			// 10
	void *CubinData;		// 18
	uint32_t CubinDataSize; // 20
};

using PfnNvAPI_QueryInterface = void *(__stdcall *)(NV_INTERFACE InterfaceId);
using PfnNvAPI_GPU_GetArchInfo = NV_STATUS(__stdcall *)(void *GPUHandle, NV_ARCH_INFO *ArchInfo);
using PfnNvAPI_D3D12_CreateCubinComputeShaderExV2 = NV_STATUS(__stdcall *)(NV_CREATE_CUBIN_SHADER_INFO *CreateInfo);

PfnNvAPI_QueryInterface OriginalNvAPI_QueryInterface = nullptr;
PfnNvAPI_GPU_GetArchInfo OriginalNvAPI_GPU_GetArchInfo = nullptr;
PfnNvAPI_D3D12_CreateCubinComputeShaderExV2 OriginalNvAPI_D3D12_CreateCubinComputeShaderExV2 = nullptr;
decltype(&D3DKMTQueryAdapterInfo) OriginalD3DKMTQueryAdapterInfo = nullptr;

NV_STATUS __stdcall HookedNvAPI_GPU_GetArchInfo(void *GPUHandle, NV_ARCH_INFO *ArchInfo)
{
	if (!OriginalNvAPI_GPU_GetArchInfo)
		return NV_STATUS::Error;

	const auto status = OriginalNvAPI_GPU_GetArchInfo(GPUHandle, ArchInfo);

	if (status == NV_STATUS::Success && ArchInfo && (ArchInfo->Version == 0x10010 || ArchInfo->Version == 0x20010))
	{
		// if (arch < Ada or arch >= special)
		if (ArchInfo->Architecture < 0x190 || ArchInfo->Architecture >= 0xE0000000)
		{
			ArchInfo->Architecture = 0x190;	 // Force Ada
			ArchInfo->Implementation = 4;	 // Force GA104
			ArchInfo->Revision = 0xFFFFFFFF; // Force ...unknown?
		}
	}

	return status;
}

NV_STATUS __stdcall HookedNvAPI_D3D12_SetRawScgPriority(NV_SCG_PRIORITY_INFO *PriorityInfo)
{
	// SCG or "Simultaneous Compute and Graphics" is their fancy term for async compute. This is a completely
	// undocumented driver hack used in early versions of sl.dlss_g.dll. Not a single hit on Google.
	//
	// Architecture-specific call. Ada or newer only.
	//
	// This function must be stubbed. Otherwise expect undebuggable device removals.
	return NV_STATUS::Success;
}

#if 0
#include "Hooking/Memory.h"

NV_STATUS __stdcall HookedNvAPI_D3D12_CreateCubinComputeShaderExV2(NV_CREATE_CUBIN_SHADER_INFO *CreateInfo)
{
	if (!OriginalNvAPI_D3D12_CreateCubinComputeShaderExV2)
		return NV_STATUS::Error;

	auto result = OriginalNvAPI_D3D12_CreateCubinComputeShaderExV2(CreateInfo);

	if (!CreateInfo)
		return result;

	if (result == NV_STATUS::Error && CreateInfo && CreateInfo->SizeOfStruct == 0x50)
	{
		const bool isFatbin = (CreateInfo->CubinDataSize >= 4) && (memcmp(CreateInfo->CubinData, "\x50\xED\x55\xBA", 4) == 0);

		if (isFatbin)
		{
			std::vector cubinData(
				static_cast<uint8_t *>(CreateInfo->CubinData),
				static_cast<uint8_t *>(CreateInfo->CubinData) + CreateInfo->CubinDataSize);

			cubinData[0x2C] = 0x59; // sm_89

			while (true)
			{
				auto target = Memory::FindPattern(
					reinterpret_cast<uintptr_t>(cubinData.data()),
					cubinData.size(),
					"2E 74 61 72 67 65 74 20 73 6D 5F 31 32 30 0A");

				if (!target)
					break;

				memcpy(reinterpret_cast<void *>(target), ".target sm_89 \n", 15);
			}

			CreateInfo->CubinData = cubinData.data();
			result = OriginalNvAPI_D3D12_CreateCubinComputeShaderExV2(CreateInfo);
		}
	}

	return result;
}
#endif

void *__stdcall HookedNvAPI_QueryInterface(NV_INTERFACE InterfaceId)
{
	const auto result = OriginalNvAPI_QueryInterface(InterfaceId);

	if (result)
	{
		switch (InterfaceId)
		{
		case NV_INTERFACE::GPU_GetArchInfo:
			OriginalNvAPI_GPU_GetArchInfo = static_cast<PfnNvAPI_GPU_GetArchInfo>(result);
			return &HookedNvAPI_GPU_GetArchInfo;

		case NV_INTERFACE::D3D12_SetRawScgPriority:
			return &HookedNvAPI_D3D12_SetRawScgPriority;

#if 0
		case NV_INTERFACE::D3D12_CreateCubinComputeShaderExV2:
			OriginalNvAPI_D3D12_CreateCubinComputeShaderExV2 = static_cast<PfnNvAPI_D3D12_CreateCubinComputeShaderExV2>(result);
			return &HookedNvAPI_D3D12_CreateCubinComputeShaderExV2;

		case NV_INTERFACE::D3D12_SetFlipConfig:
			return nullptr;
#endif
		}
	}

	return result;
}

#if 0
NTSTATUS WINAPI HookedD3DKMTQueryAdapterInfo(const D3DKMT_QUERYADAPTERINFO *Info)
{
	const auto result = OriginalD3DKMTQueryAdapterInfo(Info);

	if (result == 0 && Info && Info->Type == KMTQAITYPE_UMDRIVERPRIVATE)
	{
		struct NV_D3DKMT_PRIVATE_DRIVER_DATA // nvwgf2umx.dll (546.33)
		{
			uint32_t Header;				 // 0 NVDA
			char Padding[0xE4];				 // 4
			uint32_t Architecture;			 // E8
		};

		if (Info->pPrivateDriverData && Info->PrivateDriverDataSize >= sizeof(NV_D3DKMT_PRIVATE_DRIVER_DATA))
		{
			auto driverData = static_cast<NV_D3DKMT_PRIVATE_DRIVER_DATA *>(Info->pPrivateDriverData);

			if (driverData->Header == 0x4E564441)
				driverData->Architecture = 0x150;
		}
	}

	return result;
}
#endif

bool TryInterceptNvAPIFunction(void *ModuleHandle, const void *FunctionName, void **FunctionPointer)
{
	if (!FunctionName || !*FunctionPointer || reinterpret_cast<uintptr_t>(FunctionName) < 0x10000)
		return false;

	if (_stricmp(static_cast<const char *>(FunctionName), "nvapi_QueryInterface") == 0)
	{
		OriginalNvAPI_QueryInterface = static_cast<PfnNvAPI_QueryInterface>(*FunctionPointer);
		*FunctionPointer = &HookedNvAPI_QueryInterface;

		return true;
	}

	return false;
}
