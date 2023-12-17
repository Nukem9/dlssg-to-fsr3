#include <Windows.h>
#include <charconv>

BOOL WINAPI RawDllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		// Start vsjitdebugger.exe if a debugger isn't already attached. VS_DEBUGGER_REQUEST determines the
		// command line and VS_DEBUGGER_PROC is used to hide the CreateProcessA IAT entry.
		if (char cmd[256] = {}, proc[256] = {}; !IsDebuggerPresent() &&
												GetEnvironmentVariableA("VS_DEBUGGER_REQUEST", cmd, ARRAYSIZE(cmd)) > 0 &&
												GetEnvironmentVariableA("VS_DEBUGGER_PROC", proc, ARRAYSIZE(proc)) > 0)
		{
			std::to_chars(cmd + strlen(cmd), std::end(cmd), GetCurrentProcessId());
			auto moduleName = proc;
			auto importName = strchr(proc, '!') + 1;
			importName[-1] = '\0';

			PROCESS_INFORMATION pi = {};
			STARTUPINFOA si = {};
			si.cb = sizeof(si);

			auto c = reinterpret_cast<decltype(&CreateProcessA)>(GetProcAddress(GetModuleHandleA(moduleName), importName));
			c(nullptr, cmd, nullptr, nullptr, false, 0, nullptr, nullptr, &si, &pi);

			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}

	return TRUE;
}

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
		OutputDebugStringW(L"DEBUG: Built with commit ID " BUILD_GIT_COMMIT_HASH "\n");

	return TRUE;
}

extern "C" extern decltype(&RawDllMain) const _pRawDllMain = RawDllMain;
