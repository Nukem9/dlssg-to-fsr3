#pragma once

namespace Util
{
	template<size_t Size>
	const wchar_t *GetModulePath(wchar_t (&Buffer)[Size], bool DirectoryOnly, HMODULE ModuleHandle)
	{
		memset(Buffer, 0, Size * sizeof(wchar_t));

		if (!ModuleHandle)
		{
			if (!GetModuleHandleExW(
					GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
					reinterpret_cast<LPCWSTR>(&GetModulePath<Size>),
					&ModuleHandle))
				return nullptr;
		}

		const auto len = GetModuleFileNameW(ModuleHandle, Buffer, Size - 1);

		if (len <= 0)
			return nullptr;

		for (auto i = static_cast<ptrdiff_t>(len) - 1; i > 0; i--)
		{
			if (Buffer[i] == L'\\' || Buffer[i] == L'/')
			{
				if (DirectoryOnly)
					Buffer[i + 1] = 0;		// Chop off the file name leaving only the directory
				else
					return &Buffer[i + 1];	// Split the name out

				break;
			}
		}

		return Buffer;
	}
}
