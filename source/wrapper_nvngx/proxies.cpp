#include <windows.h>
#include <string>
#include <array>

auto CustomLibraryResolverCallback();

constexpr auto DLL_PROXY_EXPORT_LISTING_FILE = "ExportListing.inc";                   // List of exported functions
constexpr auto DLL_PROXY_TLS_CALLBACK_AUTOINIT = true;                                     // Enable automatic initialization through a thread local storage callback
constexpr auto DLL_PROXY_DECLARE_IMPLEMENTATION = true;                                    // Define the whole implementation
auto DLL_PROXY_LIBRARY_RESOLVER_CALLBACK = CustomLibraryResolverCallback;   // Custom DLL path resolver
#include "DllProxy.h"

auto CustomLibraryResolverCallback()
{
	// Avoid using the CRT at all, including thread-local variable init
	constinit static HMODULE NVNGXModuleHandle = nullptr;

	if (!NVNGXModuleHandle)
	{
		std::array<wchar_t, MAX_PATH> filePath = {};
		DWORD filePathSize = sizeof(filePath); // Nvidia screwed this up with an ARRAYSIZE() instead

		HKEY key = nullptr;
		auto status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", 0, KEY_READ, &key);

		if (status == ERROR_SUCCESS)
		{
			status = RegGetValueW(key, nullptr, L"NGXPath", RRF_RT_ANY, nullptr, filePath.data(), &filePathSize);
			RegCloseKey(key);
		}
		else
		{
			status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", 0, KEY_READ, &key);

			if (status == ERROR_SUCCESS)
			{
				status = RegGetValueW(key, nullptr, L"FullPath", RRF_RT_ANY, nullptr, filePath.data(), &filePathSize);
				RegCloseKey(key);
			}
		}

		if (status == ERROR_SUCCESS)
		{
			std::wstring filePathStr(filePath.data());
			filePathStr += L"\\_nvngx.dll";
			NVNGXModuleHandle = LoadLibraryW(filePathStr.c_str());
		}

		if (!NVNGXModuleHandle)
			MessageBoxW(nullptr, L"Failed to load System32 NGXCore library.", L"dlssg-to-fsr3", MB_OK);
	}

	return NVNGXModuleHandle;
}
