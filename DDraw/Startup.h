#pragma once

#include <windows.h>
#include <cstdint>
#include <cstring>

namespace DDrawStartup
{
	inline bool IsGameCodeAddress(HMODULE executable, const void* address)
	{
		const auto base = reinterpret_cast<uintptr_t>(executable);
		const auto offset = reinterpret_cast<uintptr_t>(address) - base;
		const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto headers = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
		const auto sections = IMAGE_FIRST_SECTION(headers);
		for (unsigned i = 0; i < headers->FileHeader.NumberOfSections; ++i)
		{
			if (std::memcmp(sections[i].Name, ".text\0\0", IMAGE_SIZEOF_SHORT_NAME) == 0)
			{
				return offset >= sections[i].VirtualAddress &&
					offset - sections[i].VirtualAddress < sections[i].Misc.VirtualSize;
			}
		}
		return false;
	}
}
