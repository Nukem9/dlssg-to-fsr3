#include <Windows.h>

void *CustomLibraryResolverCallback();

#define DLL_PROXY_EXPORT_LISTING_FILE "ExportListing.inc"                   // List of exported functions
#define DLL_PROXY_TLS_CALLBACK_AUTOINIT                                     // Enable automatic initialization through a thread local storage callback
#define DLL_PROXY_DECLARE_IMPLEMENTATION                                    // Define the whole implementation
#define DLL_PROXY_LIBRARY_RESOLVER_CALLBACK CustomLibraryResolverCallback   // Custom DLL path resolver
#include "DllProxy.h"

extern bool EnableAggressiveHooking;

void *TryResolveSystemLibrary(const wchar_t *RealLibraryName)
{
	// Build the full system32 path
	wchar_t fullSystemPath[2048] = {};

	if (GetSystemDirectoryW(fullSystemPath, ARRAYSIZE(fullSystemPath) - 1) <= 0)
		return nullptr;

	wcscat_s(fullSystemPath, L"\\");
	wcscat_s(fullSystemPath, RealLibraryName);

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

	// Use the current working directory if registry keys aren't present
	if (status == ERROR_SUCCESS)
		wcscat_s(filePath, L"\\_nvngx.dll");
	else
		wcscpy_s(filePath, L"_nvngx.dll");

	const auto moduleHandle = LoadLibraryW(filePath);

	if (!moduleHandle)
		MessageBoxW(nullptr, L"Failed to load NGXCore library.", L"dlssg-to-fsr3", MB_OK);

	return moduleHandle;
}

void *CustomLibraryResolverCallback()
{
	// Avoid using the CRT at all, including thread-local variable init
	constinit static void *moduleHandle = nullptr;

	if (!moduleHandle)
	{
		// Grab the file name and extension of this dll
		wchar_t temp[2048] = {};
		const auto targetLibraryName = [&]() -> const wchar_t *
		{
			HMODULE thisModuleHandle = nullptr;

			if (!GetModuleHandleExW(
					GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCWSTR>(&CustomLibraryResolverCallback),
					&thisModuleHandle))
				return nullptr;

			if (GetModuleFileNameW(thisModuleHandle, temp, ARRAYSIZE(temp)) <= 0)
				return nullptr;

			for (auto i = static_cast<ptrdiff_t>(wcslen(temp)) - 1; i > 0; i--)
			{
				// Split the name out
				if (temp[i] == L'\\' || temp[i] == L'/')
					return &temp[i + 1];
			}

			return temp;
		}();

		if (targetLibraryName)
		{
			if (_wcsicmp(targetLibraryName, L"nvngx.dll") == 0)
			{
				// Check the registry
				EnableAggressiveHooking = false;
				moduleHandle = TryResolveNGXLibrary();
			}
			else if (_wcsicmp(targetLibraryName, L"dbghelp.dll") == 0 ||
				_wcsicmp(targetLibraryName, L"winhttp.dll") == 0 ||
				_wcsicmp(targetLibraryName, L"version.dll") == 0)
			{
				// Check system32
				EnableAggressiveHooking = true;
				moduleHandle = TryResolveSystemLibrary(targetLibraryName);
			}
			else
			{
				// Not a system DLL and not NGX. We're either an ASI variant or some arbitrary DLL. Don't
				// bother resolving exports properly.
				EnableAggressiveHooking = true;
				moduleHandle = GetModuleHandleW(nullptr);
			}
		}
	}

	return moduleHandle;
}
