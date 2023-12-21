#include <Windows.h>
#include <stdio.h>
#include "Util.h"

namespace Util
{
	const std::wstring& GetThisDllPath()
	{
		const static std::wstring finalPath = [&]()
		{
			wchar_t path[2048] = {};
			HMODULE thisModuleHandle = nullptr;

			GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&GetThisDllPath),
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

			return std::wstring(path);
		}();

		return finalPath;
	}

	void Log(const char *Format, ...)
	{
		const static auto logPath = GetThisDllPath() + L"\\dlssg_to_fsr3.log";
		static FILE *f = nullptr;

		if (!f)
		{
			f = _wfsopen(logPath.c_str(), L"w", _SH_DENYWR);

			if (f)
				setvbuf(f, NULL, _IONBF, 0);
		}

		if (f)
		{
			va_list args;
			char buffer[2048];

			va_start(args, Format);
			const auto len = _vsnprintf_s(buffer, _TRUNCATE, Format, args);
			va_end(args);

			fwrite(buffer, 1, len, f);
		}
	}

	bool GetSetting(const wchar_t *Category, const wchar_t *Key, bool Default)
	{
		const static auto iniPath = GetThisDllPath() + L"\\dlssg_to_fsr3.ini";

		return GetPrivateProfileIntW(Category, Key, Default, iniPath.c_str()) != 0;
	}
}
