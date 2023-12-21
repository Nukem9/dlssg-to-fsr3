#include <spdlog/sinks/basic_file_sink.h>
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

	void InitializeLog()
	{
		static bool once = []()
		{
			const auto fullPath = GetThisDllPath() + L"\\dlssg_to_fsr3.log";
			char convertedPath[2048] = {};

			if (wcstombs_s(nullptr, convertedPath, fullPath.c_str(), std::size(convertedPath)) == 0)
			{
				auto logger = spdlog::basic_logger_mt("file_logger", convertedPath, true);
				logger->set_level(spdlog::level::level_enum::trace);
				logger->set_pattern("[%H:%M:%S] [%l] %v"); // [HH:MM:SS] [Level] Message
				logger->flush_on(logger->level());
				spdlog::set_default_logger(std::move(logger));
			}

			return true;
		}();
	}

	bool GetSetting(const wchar_t *Category, const wchar_t *Key, bool Default)
	{
		const static auto iniPath = GetThisDllPath() + L"\\dlssg_to_fsr3.ini";

		return GetPrivateProfileIntW(Category, Key, Default, iniPath.c_str()) != 0;
	}
}
