#pragma once

namespace Util
{
	void InitializeLog();
	bool GetSetting(const wchar_t *Key, bool DefaultValue);
	float GetSetting(const wchar_t *Key, float DefaultValue);
}
