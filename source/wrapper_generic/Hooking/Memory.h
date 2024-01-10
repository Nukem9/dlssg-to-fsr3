#pragma once

#include <cstdint>
#include <initializer_list>

namespace Memory
{
	void Patch(std::uintptr_t Address, const std::uint8_t *Data, std::size_t Size);
	void Patch(std::uintptr_t Address, std::initializer_list<std::uint8_t> Data);
	void Fill(std::uintptr_t Address, std::uint8_t Value, std::size_t Size);
}
