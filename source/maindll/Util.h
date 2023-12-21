#pragma once

namespace Util
{
	void InitializeLog();
	bool GetSetting(const wchar_t *Category, const wchar_t *Key, bool Default);
}
