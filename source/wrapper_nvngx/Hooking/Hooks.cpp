#include <Windows.h>
#include <detours/detours.h>
#include "Memory.h"
#include "Hooks.h"

namespace Hooks
{
	struct IATEnumContext
	{
		const char *ModuleName = nullptr;
		std::variant<const char *, int> ImportName;
		const void *CallbackFunction = nullptr;
		void **OriginalFunction = nullptr;
		bool ModuleFound = false;
		bool Succeeded = false;
	};

	bool WriteVirtualFunction(std::uintptr_t TableAddress, uint32_t Index, const void *CallbackFunction, void **OriginalFunction)
	{
		if (!TableAddress)
			return false;

		const auto calculatedAddress = TableAddress + (sizeof(void *) * Index);

		if (OriginalFunction)
			*OriginalFunction = *reinterpret_cast<void **>(calculatedAddress);

		Memory::Patch(calculatedAddress, reinterpret_cast<const std::uint8_t *>(&CallbackFunction), sizeof(void *));
		return true;
	}

	bool RedirectImport(
		void *ModuleHandle,
		const char *ImportModuleName,
		std::variant<const char *, int> ImportName,
		const void *CallbackFunction,
		void **OriginalFunction)
	{
		auto moduleCallback = [](PVOID Context, HMODULE, LPCSTR Name) -> BOOL
		{
			auto c = static_cast<IATEnumContext *>(Context);

			c->ModuleFound = Name && _stricmp(Name, c->ModuleName) == 0;
			return !c->Succeeded;
		};

		auto importCallback = [](PVOID Context, ULONG Ordinal, PCSTR Name, PVOID *Func) -> BOOL
		{
			auto c = static_cast<IATEnumContext *>(Context);

			if (!c->ModuleFound)
				return false;

			// If the import name matches...
			const bool matches = [&]()
			{
				if (!Func)
					return false;

				if (std::holds_alternative<const char *>(c->ImportName))
					return _stricmp(Name, std::get<const char *>(c->ImportName)) == 0;

				return std::cmp_equal(Ordinal, std::get<int>(c->ImportName));
			}();

			if (matches)
			{
				// ...swap out the IAT pointer
				if (c->OriginalFunction)
					*c->OriginalFunction = *Func;

				Memory::Patch(
					reinterpret_cast<std::uintptr_t>(Func),
					reinterpret_cast<const std::uint8_t *>(&c->CallbackFunction),
					sizeof(void *));

				c->Succeeded = true;
				return false;
			}

			return true;
		};

		IATEnumContext context {
			.ModuleName = ImportModuleName,
			.ImportName = ImportName,
			.CallbackFunction = CallbackFunction,
			.OriginalFunction = OriginalFunction,
		};

		if (!ModuleHandle)
			ModuleHandle = GetModuleHandleA(nullptr);

		DetourEnumerateImportsEx(reinterpret_cast<HMODULE>(ModuleHandle), &context, moduleCallback, importCallback);
		return context.Succeeded;
	}
}
