#pragma once

namespace Util
{
	void Log(const char *Format, ...);
	bool GetSetting(const wchar_t *Category, const wchar_t *Key, bool Default);
}
