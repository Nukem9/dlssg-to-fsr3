#include <cstdint>
#include <string.h>

enum class NV_STATUS : uint32_t
{
	Success = 0,
	Error = 0xFFFFFFFF,
};

enum class NV_INTERFACE : uint32_t
{
	GPU_GetArchInfo = 0xD8265D24,
};

struct NV_ARCH_INFO
{
	uint32_t Version;
	uint32_t Architecture;
	uint32_t Unknown[2];
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

void *__stdcall HookedNvAPI_QueryInterface(NV_INTERFACE InterfaceId)
{
	const auto result = OriginalNvAPI_QueryInterface(InterfaceId);

	if (InterfaceId == NV_INTERFACE::GPU_GetArchInfo)
	{
		OriginalNvAPI_GPU_GetArchInfo = static_cast<PfnNvAPI_GPU_GetArchInfo>(result);
		return &HookedNvAPI_GPU_GetArchInfo;
	}

	return result;
}

void TryInterceptNvAPIFunction(void *ModuleHandle, const void *FunctionName, void **FunctionPointer)
{
	if (!FunctionName || !*FunctionPointer || reinterpret_cast<uintptr_t>(FunctionName) < 0x10000)
		return;

	if (_stricmp(static_cast<const char *>(FunctionName), "nvapi_QueryInterface") == 0)
	{
		OriginalNvAPI_QueryInterface = static_cast<PfnNvAPI_QueryInterface>(*FunctionPointer);
		*FunctionPointer = &HookedNvAPI_QueryInterface;
	}
}
