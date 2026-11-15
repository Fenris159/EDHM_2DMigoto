#pragma once

#include <windows.h>

#include <cstring>
#include <stdexcept>

// Replace only the product DLL's clock import. Driver/OS clocks stay real;
// the product sees one frozen tick after initialization without sleeping.
class ClockBoundary
{
	ULONG_PTR *slot = nullptr;
	ULONG_PTR original = 0;
	inline static ULONGLONG tick = 0;

	static ULONGLONG WINAPI now()
	{
		return tick;
	}

  public:
	explicit ClockBoundary(HMODULE module)
	{
		auto *base = reinterpret_cast<unsigned char *>(module);
		const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
		const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
		const auto &imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!imports.VirtualAddress)
			throw std::runtime_error("product has no clock import directory");
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
				if (std::strcmp(reinterpret_cast<const char *>(entry->Name), "GetTickCount64") != 0)
					continue;
				slot = &address->u1.Function;
				DWORD protection = 0;
				if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &protection))
					throw std::runtime_error("product clock import is not writable");
				original = *slot;
				// Initialization completed before this sample. No amount of warm
				// startup can make the frozen product tick equal its launch tick.
				tick = GetTickCount64() + 1;
				auto replacement = &now;
				static_assert(sizeof(replacement) == sizeof(*slot));
				std::memcpy(slot, &replacement, sizeof(replacement));
				DWORD ignored = 0;
				VirtualProtect(slot, sizeof(*slot), protection, &ignored);
				return;
			}
		}
		throw std::runtime_error("product GetTickCount64 import not found");
	}

	ClockBoundary(const ClockBoundary &) = delete;
	ClockBoundary &operator=(const ClockBoundary &) = delete;
	ClockBoundary(ClockBoundary &&) = delete;
	ClockBoundary &operator=(ClockBoundary &&) = delete;

	~ClockBoundary()
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
