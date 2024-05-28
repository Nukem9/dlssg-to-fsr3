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
	D3D12_SetRawScgPriority = 0x5DB3048A,
};

struct NV_ARCH_INFO
{
	uint32_t Version;
	uint32_t Architecture;
	uint32_t Implementation;
	uint32_t Unknown[1];
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

struct NV_D3DKMT_PRIVATE_DRIVER_DATA // nvwgf2umx.dll (546.33)
{
	uint32_t Header;				 // 0 NVDA
	char Padding[0xE4];				 // 4
	uint32_t Architecture;			 // E8
};

using PfnNvAPI_QueryInterface = void *(__stdcall *)(NV_INTERFACE InterfaceId);
using PfnNvAPI_GPU_GetArchInfo = NV_STATUS(__stdcall *)(void *GPUHandle, NV_ARCH_INFO *ArchInfo);

PfnNvAPI_QueryInterface OriginalNvAPI_QueryInterface = nullptr;
PfnNvAPI_GPU_GetArchInfo OriginalNvAPI_GPU_GetArchInfo = nullptr;
decltype(&D3DKMTQueryAdapterInfo) OriginalD3DKMTQueryAdapterInfo = nullptr;

NV_STATUS __stdcall HookedNvAPI_GPU_GetArchInfo(void *GPUHandle, NV_ARCH_INFO *ArchInfo)
{
	if (OriginalNvAPI_GPU_GetArchInfo)
	{
		const auto status = OriginalNvAPI_GPU_GetArchInfo(GPUHandle, ArchInfo);

		if (status == NV_STATUS::Success && ArchInfo)
		{
			// if (arch < ada or arch >= special)
			if (ArchInfo->Architecture < 0x190 || ArchInfo->Architecture >= 0xE0000000)
			{
				ArchInfo->Architecture = 0x190; // Force Ada
				ArchInfo->Implementation = 4;	// Force GA104
			}
		}

		return status;
	}

	return NV_STATUS::Error;
}

NV_STATUS __stdcall HookedNvAPI_D3D12_SetRawScgPriority(NV_SCG_PRIORITY_INFO *PriorityInfo)
{
	// SCG or "Simultaneous Compute and Graphics" is their fancy term for async compute. This is a completely
	// undocumented driver hack used in early versions of sl.dlss_g.dll. Not a single hit on Google.
	//
	// nvngx_dlssg.dll feature evaluation somehow prevents crashes. Architecture-specific call? Literally no
	// clue.
	//
	// This function must be stubbed. Otherwise expect undebuggable device removals.
	return NV_STATUS::Success;
}

void *__stdcall HookedNvAPI_QueryInterface(NV_INTERFACE InterfaceId)
{
	const auto result = OriginalNvAPI_QueryInterface(InterfaceId);

	if (result)
	{
		if (InterfaceId == NV_INTERFACE::GPU_GetArchInfo)
		{
			OriginalNvAPI_GPU_GetArchInfo = static_cast<PfnNvAPI_GPU_GetArchInfo>(result);
			return &HookedNvAPI_GPU_GetArchInfo;
		}

		if (InterfaceId == NV_INTERFACE::D3D12_SetRawScgPriority)
			return &HookedNvAPI_D3D12_SetRawScgPriority;
	}

	return result;
}

#if 0
NTSTATUS WINAPI HookedD3DKMTQueryAdapterInfo(const D3DKMT_QUERYADAPTERINFO *Info)
{
	const auto result = OriginalD3DKMTQueryAdapterInfo(Info);

	if (result == 0 && Info && Info->Type == KMTQAITYPE_UMDRIVERPRIVATE)
	{
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
