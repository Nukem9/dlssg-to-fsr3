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

    uintptr_t FindPattern(std::uintptr_t StartAddress, std::uintptr_t MaxSize, const char *Mask)
	{
		std::vector<std::pair<uint8_t, bool>> pattern;

		for (size_t i = 0; i < strlen(Mask);)
		{
			if (Mask[i] != '?')
			{
				pattern.emplace_back(static_cast<uint8_t>(strtoul(&Mask[i], nullptr, 16)), false);
				i += 3;
			}
			else
			{
				pattern.emplace_back(0x00, true);
				i += 2;
			}
		}

		const auto dataStart = reinterpret_cast<const uint8_t *>(StartAddress);
		const auto dataEnd = dataStart + MaxSize + 1;

		const auto ret = std::search(
			dataStart,
			dataEnd,
			pattern.begin(),
			pattern.end(),
			[](uint8_t CurrentByte, const std::pair<uint8_t, bool>& Pattern)
		{
			return Pattern.second || (CurrentByte == Pattern.first);
		});

		if (ret == dataEnd)
			return 0;

		return std::distance(dataStart, ret) + StartAddress;
	}
}
