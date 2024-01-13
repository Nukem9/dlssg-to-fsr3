#include "Hooking/Hooks.h"
#include "Util.h"

//
// sl.interposer.dll  loads  sl.common.dll
// sl.common.dll      loads  _nvngx.dll       <- we are here
// _nvngx.dll         loads  nvngx_dlssg.dll  <- intercept this stage
//
std::vector<const wchar_t *> TargetLibrariesToHook = { L"sl.interposer.dll", L"sl.common.dll", L"sl.dlss_g.dll", L"_nvngx.dll" };
constinit const wchar_t *TargetImplementationDll = L"nvngx_dlssg.dll";
constinit const wchar_t *RelplacementImplementationDll = L"dlssg_to_fsr3_amd_is_better.dll";

constinit const wchar_t *TargetEGSServicesDll = L"EOSSDK-Win64-Shipping.dll";
constinit const wchar_t *TargetEGSOverlayDll = L"EOSOVH-Win64-Shipping.dll";

bool EnableAggressiveHooking;

bool TryInterceptNvAPIFunction(void *ModuleHandle, const void *FunctionName, void **FunctionPointer);
bool PatchImportsForModule(const wchar_t *Path, HMODULE ModuleHandle);

void *LoadImplementationDll()
{
	// Use the same directory as the current DLL
	wchar_t path[2048];
	Util::GetModulePath(path, true, nullptr);

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

bool RedirectModule(const wchar_t *Path, HMODULE *ModuleHandle)
{
	if (Path)
	{
		if (std::wstring_view(Path).ends_with(TargetImplementationDll))
		{
			*ModuleHandle = static_cast<HMODULE>(LoadImplementationDll());
			return true;
		}

		if (std::wstring_view(Path).ends_with(TargetEGSOverlayDll))
		{
			SetLastError(ERROR_MOD_NOT_FOUND);
			*ModuleHandle = nullptr;

			return true;
		}
	}

	return false;
}

HMODULE WINAPI HookedLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE libraryHandle = nullptr;

	if (RedirectModule(lpLibFileName, &libraryHandle))
		return libraryHandle;
	else
		libraryHandle = LoadLibraryExW(lpLibFileName, hFile, dwFlags);

	PatchImportsForModule(lpLibFileName, libraryHandle);
	return libraryHandle;
}

HMODULE WINAPI HookedLoadLibraryW(LPCWSTR lpLibFileName)
{
	HMODULE libraryHandle = nullptr;

	if (RedirectModule(lpLibFileName, &libraryHandle))
		return libraryHandle;
	else
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

	const bool isMatch = std::any_of(
		TargetLibrariesToHook.begin(),
		TargetLibrariesToHook.end(),
		[path = std::wstring_view(Path)](const wchar_t *Target)
		{
			return path.ends_with(Target);
		});

	if (!isMatch || !ModuleRequiresPatching(ModuleHandle))
		return false;

#if 0
	OutputDebugStringW(L"Patching imports for a new module: ");
	OutputDebugStringW(Path);
	OutputDebugStringW(L"...\n");
#endif

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
		{
			TargetLibrariesToHook.push_back(TargetEGSServicesDll);
			LoadLibraryW(TargetEGSServicesDll);

			//
			// Aggressive hooking tries to force SL's interposer to load early. It's not always present
			// in the local directory. A bit of guesswork is required.
			//
			// "Dying Light 2\                  ph\work\bin\x64\DyingLightGame_x64_rwdi.exe"
			// "Returnal\         Returnal\     Binaries\Win64\Returnal-Win64-Shipping.exe"
			// "Hogwarts Legacy\  Phoenix\      Binaries\Win64\HogwartsLegacy.exe"
			// "SW Jedi Survivor\ SwGame\       Binaries\Win64\JediSurvivor.exe"
			// "Atomic Heart\     AtomicHeart\  Binaries\Win64\AtomicHeart-Win64-Shipping.exe
			// "MMS\              MidnightSuns\ Binaries\Win64\MidnightSuns-Win64-Shipping.exe"
			//
			// "Dying Light 2\    ph\work\bin\x64\sl.interposer.dll"
			// "Returnal\         Engine\Plugins\Streamline\Binaries\ThirdParty\Win64\sl.interposer.dll"
			// "Hogwarts Legacy\  Engine\Plugins\Runtime\Nvidia\Streamline\Binaries\ThirdParty\Win64\sl.interposer.dll"
			// "SW Jedi Survivor\ Engine\Plugins\Runtime\Nvidia\Streamline\Binaries\ThirdParty\Win64\sl.interposer.dll"
			// "Atomic Heart\     Engine\Plugins\Runtime\Nvidia\Streamline\Binaries\ThirdParty\Win64\sl.interposer.dll"
			// "MMS\              Engine\Plugins\Runtime\Nvidia\Streamline\Binaries\ThirdParty\Win64\sl.interposer.dll"
			//
			constinit static const wchar_t *bruteInterposerPaths[] = {
				L"sl.interposer.dll",
				L"..\\..\\..\\Engine\\Plugins\\Streamline\\Binaries\\ThirdParty\\Win64\\sl.interposer.dll",
				L"..\\..\\..\\Engine\\Plugins\\Runtime\\Nvidia\\Streamline\\Binaries\\ThirdParty\\Win64\\sl.interposer.dll",
			};

			if (!LoadLibraryW(bruteInterposerPaths[0]))
			{
				wchar_t path[2048];

				for (auto interposer : bruteInterposerPaths)
				{
					if (!Util::GetModulePath(path, true, GetModuleHandleW(nullptr)))
						break;

					wcscat_s(path, interposer);

					if (LoadLibraryW(path))
						break;
				}
			}
		}

		// We probably loaded after sl.interposer.dll and sl.common.dll. Try patching them up front.
		bool anyPatched = std::count_if(
			TargetLibrariesToHook.begin(),
			TargetLibrariesToHook.end(),
			[](const wchar_t *Target)
		{
			return PatchImportsForModule(Target, GetModuleHandleW(Target)) && _wcsicmp(Target, TargetEGSServicesDll) != 0;
		}) > 0;

		// If zero Streamline dlls were loaded we'll have to hook the game's LoadLibrary calls and wait
		if (!anyPatched && EnableAggressiveHooking)
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
