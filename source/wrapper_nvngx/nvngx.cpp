#include <Windows.h>
#include <string_view>
#include "Hooking/Hooks.h"

//
// sl.interposer.dll  loads  sl.common.dll
// sl.common.dll      loads  _nvngx.dll       <- we are here
// _nvngx.dll         loads  nvngx_dlssg.dll  <- intercept this stage
//
constinit const wchar_t *TargetLibrariesToHook[] = { L"sl.interposer.dll", L"sl.common.dll", L"_nvngx.dll" };
constinit const wchar_t *RelplacementImplementationDll = L"dlssg_to_fsr3_amd_is_better.dll";

void PatchImportForModule(const wchar_t *Path, HMODULE ModuleHandle);

auto LoadImplementationDll()
{
	// Use the same directory as the current DLL
	wchar_t path[2048] = {};
	HMODULE thisModuleHandle = nullptr;

	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&LoadImplementationDll),
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

	// DO NOT cache a handle to the implementation DLL. It might be unloaded and reloaded.
	wcscat_s(path, RelplacementImplementationDll);
	const auto mod = LoadLibraryW(path);

	if (!mod)
	{
		static bool once = [&]()
		{
			MessageBoxW(nullptr, path, L"dlssg-to-fsr3 failed to load implementation DLL.", MB_ICONERROR);
			return false;
		}();
	}

	return mod;
}

HMODULE RedirectModule(const wchar_t *Path)
{
	if (Path && std::wstring_view(Path).ends_with(L"nvngx_dlssg.dll"))
		return static_cast<HMODULE>(LoadImplementationDll());

	return nullptr;
}

HMODULE WINAPI HookedLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	auto libraryHandle = RedirectModule(lpLibFileName);

	if (!libraryHandle)
		libraryHandle = LoadLibraryExW(lpLibFileName, hFile, dwFlags);

	PatchImportForModule(lpLibFileName, libraryHandle);
	return libraryHandle;
}

HMODULE WINAPI HookedLoadLibraryW(LPCWSTR lpLibFileName)
{
	auto libraryHandle = RedirectModule(lpLibFileName);

	if (!libraryHandle)
		libraryHandle = LoadLibraryW(lpLibFileName);

	PatchImportForModule(lpLibFileName, libraryHandle);
	return libraryHandle;
}

void PatchImportForModule(const wchar_t *Path, HMODULE ModuleHandle)
{
	if (!Path || !ModuleHandle)
		return;

	std::wstring_view libFileName(Path);

	for (auto targetName : TargetLibrariesToHook)
	{
		if (!libFileName.ends_with(targetName))
			continue;

		OutputDebugStringW(L"Patching imports for a new module: ");
		OutputDebugStringW(Path);
		OutputDebugStringW(L"...\n");

		Hooks::RedirectImport(ModuleHandle, "KERNEL32.dll", "LoadLibraryW", &HookedLoadLibraryW, nullptr);
		Hooks::RedirectImport(ModuleHandle, "KERNEL32.dll", "LoadLibraryExW", &HookedLoadLibraryExW, nullptr);
	}
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		OutputDebugStringW(L"DEBUG: Built with commit ID " BUILD_GIT_COMMIT_HASH "\n");

		// We probably loaded after sl.interposer.dll and sl.common.dll. Patch them up front.
		for (const auto targetName : TargetLibrariesToHook)
			PatchImportForModule(targetName, GetModuleHandleW(targetName));
	}

	return TRUE;
}
