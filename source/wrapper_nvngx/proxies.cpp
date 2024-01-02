#include <windows.h>

void *CustomLibraryResolverCallback();

#define DLL_PROXY_EXPORT_LISTING_FILE "ExportListing.inc"                   // List of exported functions
#define DLL_PROXY_TLS_CALLBACK_AUTOINIT                                     // Enable automatic initialization through a thread local storage callback
#define DLL_PROXY_DECLARE_IMPLEMENTATION                                    // Define the whole implementation
#define DLL_PROXY_LIBRARY_RESOLVER_CALLBACK CustomLibraryResolverCallback   // Custom DLL path resolver
#include "DllProxy.h"

constinit const wchar_t *AvailableProxies[] = { L"dbghelp.dll", L"winhttp.dll", L"version.dll" };

void *TryResolveSystemLibrary()
{
	// Grab the file name and extension of this dll
	const auto targetLibraryName = []() -> const wchar_t *
	{
		wchar_t path[2048] = {};
		HMODULE thisModuleHandle = nullptr;

		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&TryResolveSystemLibrary),
				&thisModuleHandle))
			return nullptr;

		if (GetModuleFileNameW(thisModuleHandle, path, ARRAYSIZE(path)) <= 0)
			return nullptr;

		for (auto i = static_cast<ptrdiff_t>(wcslen(path)) - 1; i > 0; i--)
		{
			if (path[i] == L'\\' || path[i] == L'/')
			{
				auto fileNamePart = &path[i + 1];

				for (auto proxy : AvailableProxies)
				{
					if (_wcsicmp(fileNamePart, proxy) == 0)
						return proxy;
				}

				break;
			}
		}

		return nullptr;
	}();

	if (!targetLibraryName)
		return nullptr;

	// Build the full system32 path
	wchar_t fullSystemPath[2048] = {};

	if (!GetSystemDirectoryW(fullSystemPath, ARRAYSIZE(fullSystemPath) - 1))
		return nullptr;

	wcscat_s(fullSystemPath, L"\\");
	wcscat_s(fullSystemPath, targetLibraryName);

	return LoadLibraryW(fullSystemPath);
}

void *TryResolveNGXLibrary()
{
	wchar_t filePath[MAX_PATH] = {};
	DWORD filePathSize = sizeof(filePath); // Nvidia screwed this up with an ARRAYSIZE() instead

	HKEY key = nullptr;
	auto status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", 0, KEY_READ, &key);

	if (status == ERROR_SUCCESS)
	{
		status = RegGetValueW(key, nullptr, L"NGXPath", RRF_RT_ANY, nullptr, filePath, &filePathSize);
		RegCloseKey(key);
	}
	else
	{
		status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", 0, KEY_READ, &key);

		if (status == ERROR_SUCCESS)
		{
			status = RegGetValueW(key, nullptr, L"FullPath", RRF_RT_ANY, nullptr, filePath, &filePathSize);
			RegCloseKey(key);
		}
	}

	HANDLE moduleHandle = nullptr;

	if (status == ERROR_SUCCESS)
	{
		wcscat_s(filePath, L"\\_nvngx.dll");
		moduleHandle = LoadLibraryW(filePath);
	}

	if (!moduleHandle)
		MessageBoxW(nullptr, L"Failed to load system32 NGXCore library.", L"dlssg-to-fsr3", MB_OK);

	return moduleHandle;
}

void *CustomLibraryResolverCallback()
{
	// Avoid using the CRT at all, including thread-local variable init
	constinit static void *moduleHandle = nullptr;

	if (!moduleHandle)
	{
		moduleHandle = TryResolveSystemLibrary();

		if (!moduleHandle)
			moduleHandle = TryResolveNGXLibrary();
	}

	return moduleHandle;
}
