#include <Windows.h>
#include "Memory.h"

namespace Memory
{
	void Patch(std::uintptr_t Address, const std::uint8_t *Data, std::size_t Size)
	{
		DWORD d = 0;
		VirtualProtect(reinterpret_cast<void *>(Address), Size, PAGE_EXECUTE_READWRITE, &d);

		memcpy(reinterpret_cast<void *>(Address), Data, Size);

		VirtualProtect(reinterpret_cast<void *>(Address), Size, d, &d);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(Address), Size);
	}

	void Patch(std::uintptr_t Address, std::initializer_list<std::uint8_t> Data)
	{
		Patch(Address, Data.begin(), Data.size());
	}

	void Fill(std::uintptr_t Address, std::uint8_t Value, std::size_t Size)
	{
		DWORD d = 0;
		VirtualProtect(reinterpret_cast<void *>(Address), Size, PAGE_EXECUTE_READWRITE, &d);

		memset(reinterpret_cast<void *>(Address), Value, Size);

		VirtualProtect(reinterpret_cast<void *>(Address), Size, d, &d);
		FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(Address), Size);
	}
}
