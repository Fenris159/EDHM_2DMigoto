#pragma once

#include <windows.h>

#include <cstring>
#include <stdexcept>

// Change one import in the caller under test, leaving the OS implementation
// available to the replacement. Restore it before the fixture leaves scope.
class ImportBoundary
{
	ULONG_PTR *slot = nullptr;
	ULONG_PTR original = 0;

  public:
	template <typename Function> ImportBoundary(HMODULE module, const char *symbol, Function replacement)
	{
		auto *base = reinterpret_cast<unsigned char *>(module);
		const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
		const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
		const auto &imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!imports.VirtualAddress)
			throw std::runtime_error("test module has no import directory");
		const auto *descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR *>(base + imports.VirtualAddress);
		for (; descriptor->Name; ++descriptor)
		{
			if (!descriptor->OriginalFirstThunk)
				continue;
			const auto *name = reinterpret_cast<const IMAGE_THUNK_DATA *>(base + descriptor->OriginalFirstThunk);
			auto *address = reinterpret_cast<IMAGE_THUNK_DATA *>(base + descriptor->FirstThunk);
			for (; name->u1.AddressOfData; ++name, ++address)
			{
				if (IMAGE_SNAP_BY_ORDINAL(name->u1.Ordinal))
					continue;
				const auto *entry = reinterpret_cast<const IMAGE_IMPORT_BY_NAME *>(base + name->u1.AddressOfData);
				if (std::strcmp(reinterpret_cast<const char *>(entry->Name), symbol) != 0)
					continue;
				slot = &address->u1.Function;
				DWORD protection = 0;
				if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection))
					throw std::runtime_error("test import is not writable");
				original = *slot;
				static_assert(sizeof(replacement) == sizeof(*slot));
				std::memcpy(slot, &replacement, sizeof(replacement));
				DWORD ignored = 0;
				VirtualProtect(slot, sizeof(*slot), protection, &ignored);
				return;
			}
		}
		throw std::runtime_error("test import not found");
	}

	ImportBoundary(const ImportBoundary &) = delete;
	ImportBoundary &operator=(const ImportBoundary &) = delete;
	ImportBoundary(ImportBoundary &&) = delete;
	ImportBoundary &operator=(ImportBoundary &&) = delete;

	~ImportBoundary()
	{
		DWORD protection = 0;
		if (slot && VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection))
		{
			*slot = original;
			DWORD ignored = 0;
			VirtualProtect(slot, sizeof(*slot), protection, &ignored);
		}
	}
};
