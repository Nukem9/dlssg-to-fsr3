#include <Windows.h>
#include "NvNGX.h"

__declspec(noinline) void *GetImplementationDll()
{
	const static auto moduleHandle = []()
	{
		// Use the same directory as the current DLL
		wchar_t path[2048] = {};
		HMODULE thisModuleHandle = nullptr;

		GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&GetImplementationDll),
			&thisModuleHandle);

		if (GetModuleFileNameW(thisModuleHandle, path, ARRAYSIZE(path)))
		{
			// Chop off the file name
			for (auto i = (ptrdiff_t)wcslen(path) - 1; i > 0; i--)
			{
				if (path[i] == L'\\' || path[i] == L'/')
				{
					path[i + 1] = 0;
					break;
				}
			}
		}

		wcscat_s(path, L"dlssg_to_fsr3_amd_is_better.dll");
		const auto mod = LoadLibraryW(path);

		if (!mod)
			MessageBoxW(nullptr, path, L"dlssg-to-fsr3 failed to load implementation DLL.", MB_ICONERROR);

		return mod;
	}();

	return moduleHandle;
}

__declspec(noinline) void *GetOriginalExport(const char *name)
{
	return GetProcAddress(reinterpret_cast<HMODULE>(GetImplementationDll()), name);
}
