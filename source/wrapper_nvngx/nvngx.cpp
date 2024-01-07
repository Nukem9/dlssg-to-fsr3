#include <Windows.h>
#include <algorithm>
#include <string_view>
#include <mutex>
#include <unordered_set>
#include "Hooking/Hooks.h"

//
// sl.interposer.dll  loads  sl.common.dll
// sl.common.dll      loads  _nvngx.dll       <- we are here
// _nvngx.dll         loads  nvngx_dlssg.dll  <- intercept this stage
//
constinit const wchar_t *TargetLibrariesToHook[] = { L"sl.interposer.dll", L"sl.common.dll", L"sl.dlss_g.dll", L"_nvngx.dll" };
constinit const wchar_t *TargetImplementationDll = L"nvngx_dlssg.dll";
constinit const wchar_t *RelplacementImplementationDll = L"dlssg_to_fsr3_amd_is_better.dll";

bool EnableAggressiveHooking;

void TryInterceptNvAPIFunction(void *ModuleHandle, const void *FunctionName, void **FunctionPointer);
bool PatchImportsForModule(const wchar_t *Path, HMODULE ModuleHandle);

void *LoadImplementationDll()
{
	// Use the same directory as the current DLL
	wchar_t path[2048] = {};
	HMODULE thisModuleHandle = nullptr;

	if (GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&LoadImplementationDll),
			&thisModuleHandle))
	{
		if (GetModuleFileNameW(thisModuleHandle, path, ARRAYSIZE(path)))
		{
			// Chop off the file name
			for (auto i = static_cast<ptrdiff_t>(wcslen(path)) - 1; i > 0; i--)
			{
				if (path[i] == L'\\' || path[i] == L'/')
				{
					path[i + 1] = 0;
					break;
				}
			}
		}
	}

	// Do not cache a handle to the implementation DLL. It might be unloaded and reloaded.
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
	if (Path && std::wstring_view(Path).ends_with(TargetImplementationDll))
		return static_cast<HMODULE>(LoadImplementationDll());

	return nullptr;
}

HMODULE WINAPI HookedLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	auto libraryHandle = RedirectModule(lpLibFileName);

	if (!libraryHandle)
		libraryHandle = LoadLibraryExW(lpLibFileName, hFile, dwFlags);

	PatchImportsForModule(lpLibFileName, libraryHandle);
	return libraryHandle;
}

HMODULE WINAPI HookedLoadLibraryW(LPCWSTR lpLibFileName)
{
	auto libraryHandle = RedirectModule(lpLibFileName);

	if (!libraryHandle)
		libraryHandle = LoadLibraryW(lpLibFileName);

	PatchImportsForModule(lpLibFileName, libraryHandle);
	return libraryHandle;
}

FARPROC WINAPI HookedGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	auto proc = GetProcAddress(hModule, lpProcName);

	TryInterceptNvAPIFunction(hModule, lpProcName, reinterpret_cast<void **>(&proc));
	return proc;
}

bool ModuleRequiresPatching(HMODULE ModuleHandle)
{
	static std::mutex trackerLock;
	static std::unordered_set<HMODULE> trackedModules;

	std::scoped_lock lock(trackerLock);

	if (trackedModules.size() > 100)
		trackedModules.clear();

	return trackedModules.emplace(ModuleHandle).second;
}

bool PatchImportsForModule(const wchar_t *Path, HMODULE ModuleHandle)
{
	if (!Path || !ModuleHandle)
		return false;

	std::wstring_view libFileName(Path);

	const bool isMatch = std::any_of(
		std::begin(TargetLibrariesToHook),
		std::end(TargetLibrariesToHook),
		[&](const wchar_t *Target)
		{
			return libFileName.ends_with(Target);
		});

	if (!isMatch)
		return false;

	if (!ModuleRequiresPatching(ModuleHandle))
		return false;

	OutputDebugStringW(L"Patching imports for a new module: ");
	OutputDebugStringW(Path);
	OutputDebugStringW(L"...\n");

	Hooks::RedirectImport(ModuleHandle, "KERNEL32.dll", "LoadLibraryW", &HookedLoadLibraryW, nullptr);
	Hooks::RedirectImport(ModuleHandle, "KERNEL32.dll", "LoadLibraryExW", &HookedLoadLibraryExW, nullptr);

	if (EnableAggressiveHooking)
		Hooks::RedirectImport(ModuleHandle, "KERNEL32.dll", "GetProcAddress", &HookedGetProcAddress, nullptr);

	return true;
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		OutputDebugStringW(L"DEBUG: Shim built with commit ID " BUILD_GIT_COMMIT_HASH "\n");

		if (EnableAggressiveHooking)
			LoadLibraryW(L"sl.interposer.dll");

		// We probably loaded after sl.interposer.dll and sl.common.dll. Try patching them up front.
		bool anyPatched = std::count_if(
			std::begin(TargetLibrariesToHook),
			std::end(TargetLibrariesToHook),
			[](const wchar_t *Target)
		{
			return PatchImportsForModule(Target, GetModuleHandleW(Target));
		}) > 0;

		// If zero Streamline dlls were loaded we'll have to hook the game's LoadLibrary calls and wait
		if (!anyPatched)
			anyPatched = PatchImportsForModule(TargetLibrariesToHook[0], GetModuleHandleW(nullptr));

		// Hooks can't be removed once they're in place. Pin this DLL in memory.
		if (anyPatched)
		{
			GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
				reinterpret_cast<LPCWSTR>(hInstDLL),
				&hInstDLL);
		}
	}

	return TRUE;
}
