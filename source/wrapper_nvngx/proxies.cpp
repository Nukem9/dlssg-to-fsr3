#include <windows.h>

auto CustomLibraryResolverCallback();

#define DLL_PROXY_EXPORT_LISTING_FILE "ExportListing.inc"                   // List of exported functions
#define DLL_PROXY_TLS_CALLBACK_AUTOINIT                                     // Enable automatic initialization through a thread local storage callback
#define DLL_PROXY_DECLARE_IMPLEMENTATION                                    // Define the whole implementation
#define DLL_PROXY_LIBRARY_RESOLVER_CALLBACK CustomLibraryResolverCallback   // Custom DLL path resolver
#include "DllProxy.h"

auto CustomLibraryResolverCallback()
{
	// Avoid using the CRT at all, including thread-local variable init
	constinit static HMODULE NVNGXModuleHandle = nullptr;

	if (!NVNGXModuleHandle)
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

		if (status == ERROR_SUCCESS)
		{
			wcscat_s(filePath, L"\\_nvngx.dll");
			NVNGXModuleHandle = LoadLibraryW(filePath);
		}

		if (!NVNGXModuleHandle)
			MessageBoxW(nullptr, L"Failed to load System32 NGXCore library.", L"dlssg-to-fsr3", MB_OK);
	}

	return NVNGXModuleHandle;
}
