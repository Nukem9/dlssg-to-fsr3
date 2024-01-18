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
	uint32_t Unknown[2];
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

using PfnNvAPI_QueryInterface = void *(__stdcall *)(NV_INTERFACE InterfaceId);
using PfnNvAPI_GPU_GetArchInfo = NV_STATUS(__stdcall *)(void *GPUHandle, NV_ARCH_INFO *ArchInfo);

PfnNvAPI_QueryInterface OriginalNvAPI_QueryInterface = nullptr;
PfnNvAPI_GPU_GetArchInfo OriginalNvAPI_GPU_GetArchInfo = nullptr;

NV_STATUS __stdcall HookedNvAPI_GPU_GetArchInfo(void *GPUHandle, NV_ARCH_INFO *ArchInfo)
{
	if (OriginalNvAPI_GPU_GetArchInfo)
	{
		const auto status = OriginalNvAPI_GPU_GetArchInfo(GPUHandle, ArchInfo);

		// Spoof Ada GPU arch
		if (status == NV_STATUS::Success && ArchInfo && ArchInfo->Architecture < 0x190)
			ArchInfo->Architecture = 0x190;

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

	if (InterfaceId == NV_INTERFACE::GPU_GetArchInfo)
	{
		OriginalNvAPI_GPU_GetArchInfo = static_cast<PfnNvAPI_GPU_GetArchInfo>(result);
		return &HookedNvAPI_GPU_GetArchInfo;
	}

	if (InterfaceId == NV_INTERFACE::D3D12_SetRawScgPriority)
		return &HookedNvAPI_D3D12_SetRawScgPriority;

	return result;
}

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
