#define WIN32_LEAN_AND_MEAN
#define WINVER 0x0501
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <stdio.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <ShlObj.h>
#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"
#include "Utils/ScopedUnprotect.hpp"
#include "ExternalBindings.hpp"

#include "Common_ddraw.h"
#include "DDrawFeatureConfig.h"
#include "Desktop.h"
#include "WindowedModeDDraw.h"

#pragma comment(lib, "shlwapi.lib")

extern "C" HRESULT WINAPI DirectDrawCreateEx(GUID FAR *lpGUID, LPVOID *lplpDD, REFIID iid, IUnknown FAR *pUnkOuter)
{
	static HRESULT	(WINAPI *pDirectDrawCreateEx)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*);
	if ( pDirectDrawCreateEx == nullptr )
	{
		wchar_t		wcSystemPath[MAX_PATH];
		GetSystemDirectoryW(wcSystemPath, MAX_PATH);
		PathAppendW(wcSystemPath, L"ddraw.dll");

		HMODULE		hLib = LoadLibraryW(wcSystemPath);
		pDirectDrawCreateEx = (HRESULT(WINAPI*)(GUID FAR*, LPVOID*, REFIID, IUnknown FAR*))GetProcAddress(hLib, "DirectDrawCreateEx");
	}
	return pDirectDrawCreateEx(lpGUID, lplpDD, iid, pUnkOuter);
}

ExternalRef<const char[]> ppUserFilesDir;
static HINSTANCE hThisModule;

#if ENABLE_FIX_CPU_AFFINITY
#if defined(SILENTPATCH_SPEEDRUN)
static constexpr const wchar_t* III_INI_NAME = L"SpeedrunSilentPatchIII.ini";
static constexpr const wchar_t* VC_INI_NAME = L"SpeedrunSilentPatchVC.ini";
#else
static constexpr const wchar_t* III_INI_NAME = L"SilentPatchIII.ini";
static constexpr const wchar_t* VC_INI_NAME = L"SilentPatchVC.ini";
#endif

static DWORD_PTR ReadProcessAffinityMaskOption(const wchar_t* iniPath)
{
	constexpr DWORD_PTR DEFAULT_AFFINITY_MASK = 1;

	wchar_t value[32];
	GetPrivateProfileStringW(L"SilentPatch", L"CpuAffinityMask", L"0", value, _countof(value), iniPath);

	int parsedValue = 0;
	if (StrToIntExW(value, STIF_SUPPORT_HEX, &parsedValue) == FALSE || parsedValue < 0)
	{
		return DEFAULT_AFFINITY_MASK;
	}
	return static_cast<DWORD_PTR>(parsedValue);
}

static void ApplyConfiguredProcessAffinity(const wchar_t* iniName)
{
	wchar_t iniPath[MAX_PATH];
	if (GetModuleFileNameW(hThisModule, iniPath, _countof(iniPath)) == 0)
	{
		return;
	}

	PathRemoveFileSpecW(iniPath);
	PathAppendW(iniPath, iniName);

	const DWORD_PTR requestedAffinity = ReadProcessAffinityMaskOption(iniPath);
	if (requestedAffinity == 0)
	{
		return;
	}

	DWORD_PTR processAffinity = 0;
	DWORD_PTR systemAffinity = 0;
	DWORD_PTR affinityToApply = requestedAffinity;
	if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity) != FALSE)
	{
		const DWORD_PTR availableAffinity = processAffinity != 0 ? processAffinity : systemAffinity;
		const DWORD_PTR compatibleAffinity = requestedAffinity & availableAffinity;
		if (compatibleAffinity != 0)
		{
			affinityToApply = compatibleAffinity;
		}
	}

	SetProcessAffinityMask(GetCurrentProcess(), affinityToApply);
}
#endif

#if ENABLE_FIX_VC_JP_NO_CD_BOOTSTRAP
static decltype(LoadLibraryA)* pOrgLoadLibraryA;
static decltype(GetProcAddress)* pOrgGetProcAddress;
static HMODULE hSIntfNT;
static bool useSIntfNTFallback;
using GNOCD32_t = BOOL (WINAPI*)(DWORD* driveCount, DWORD* driveIndex);
static GNOCD32_t pOrgGNOCD32;
static FARPROC pGNOCD32Thunk;

static BOOL WINAPI SIntfNT_Stub0() { return TRUE; }
static BOOL WINAPI SIntfNT_Stub4(ULONG_PTR) { return TRUE; }
static BOOL WINAPI SIntfNT_Stub8(ULONG_PTR, ULONG_PTR) { return TRUE; }
static BOOL WINAPI SIntfNT_Stub12(ULONG_PTR, ULONG_PTR, ULONG_PTR) { return TRUE; }
static BOOL WINAPI SIntfNT_Stub16(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR) { return TRUE; }
static BOOL WINAPI SIntfNT_Stub20(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR) { return TRUE; }
static BOOL WINAPI SIntfNT_Stub24(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR) { return TRUE; }

static BOOL WINAPI TC32_Fallback(BYTE* key)
{
	if (key != nullptr) memset(key, 0, 9);
	return TRUE;
}

static BOOL WINAPI GNOCD32_Fallback(DWORD* driveCount, DWORD* driveIndex)
{
	if (driveCount != nullptr) *driveCount = 0;
	if (driveIndex != nullptr) *driveIndex = 0;
	return TRUE;
}

static BOOL WINAPI GNOCD32_Hook(DWORD* driveCount, DWORD* driveIndex)
{
	const BOOL result = pOrgGNOCD32(driveCount, driveIndex);
	// The JP no-CD executable does not use the selected drive afterwards, but its
	// leftover bootstrap still treats an empty optical-drive list as fatal.
	return result || (driveCount != nullptr && *driveCount == 0);
}

static FARPROC MakeGNOCD32Thunk(FARPROC target)
{
	if (pGNOCD32Thunk == nullptr)
	{
		BYTE* thunk = static_cast<BYTE*>(VirtualAlloc(nullptr, 0x100, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (thunk == nullptr) return nullptr;
		memset(thunk, 0x90, 0x100);
		thunk[0] = 0xE9;
		*reinterpret_cast<DWORD*>(thunk + 1) = static_cast<DWORD>(
			reinterpret_cast<DWORD_PTR>(target) - reinterpret_cast<DWORD_PTR>(thunk + 5));
		FlushInstructionCache(GetCurrentProcess(), thunk, 0x100);
		pGNOCD32Thunk = reinterpret_cast<FARPROC>(thunk);
	}
	return pGNOCD32Thunk;
}

static HMODULE WINAPI LoadLibraryA_VCJPNoCD_Hook(LPCSTR fileName)
{
	HMODULE result = pOrgLoadLibraryA(fileName);
	DWORD error = result != nullptr ? ERROR_SUCCESS : GetLastError();
	if (fileName == nullptr || _stricmp(PathFindFileNameA(fileName), "sintfnt.dll") != 0)
	{
		if (result == nullptr) SetLastError(error);
		return result;
	}

	// Re-entering this 2004 Petite-packed DLL after ERROR_DLL_INIT_FAILED deadlocks
	// the process. The no-CD executable does not need its physical-disc interface,
	// so provide ABI-compatible fallbacks instead of retrying the poisoned load.
	if (result == nullptr && error == ERROR_DLL_INIT_FAILED)
	{
		if (GetModuleHandleEx(0, nullptr, &hSIntfNT))
		{
			useSIntfNTFallback = true;
			SetLastError(ERROR_SUCCESS);
			return hSIntfNT;
		}
	}

	if (result != nullptr)
	{
		hSIntfNT = result;
	}
	else
	{
		SetLastError(error);
	}
	return result;
}

static FARPROC WINAPI GetProcAddress_VCJPNoCD_Hook(HMODULE module, LPCSTR procName)
{
	FARPROC result = pOrgGetProcAddress(module, procName);
	if (module != hSIntfNT || reinterpret_cast<ULONG_PTR>(procName) > 0xFFFF) return result;

	const ULONG_PTR ordinal = reinterpret_cast<ULONG_PTR>(procName);
	if (useSIntfNTFallback)
	{
		switch (ordinal)
		{
		case 2: return reinterpret_cast<FARPROC>(SIntfNT_Stub4);
		case 3:
		case 4: return reinterpret_cast<FARPROC>(SIntfNT_Stub0);
		case 5: return MakeGNOCD32Thunk(reinterpret_cast<FARPROC>(GNOCD32_Fallback));
		case 6: return reinterpret_cast<FARPROC>(SIntfNT_Stub8);
		case 7:
		case 13:
		case 15: return reinterpret_cast<FARPROC>(SIntfNT_Stub20);
		case 8:
		case 10:
		case 11:
		case 17:
		case 18: return reinterpret_cast<FARPROC>(SIntfNT_Stub12);
		case 9: return reinterpret_cast<FARPROC>(SIntfNT_Stub16);
		case 12: return reinterpret_cast<FARPROC>(SIntfNT_Stub24);
		case 14: return reinterpret_cast<FARPROC>(TC32_Fallback);
		default: return result;
		}
	}

	// SIntfNT ordinal 5 is GNOCD32. The executable writes into byte +0x79 of
	// this function, so return a disposable thunk rather than writable patch code.
	if (ordinal == 5 && result != nullptr)
	{
		pOrgGNOCD32 = reinterpret_cast<GNOCD32_t>(result);
		return MakeGNOCD32Thunk(reinterpret_cast<FARPROC>(GNOCD32_Hook));
	}
	return result;
}

static bool PatchVCJPNoCDBootstrapIAT()
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
	PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(
		reinterpret_cast<DWORD_PTR>(hInstance) + reinterpret_cast<PIMAGE_DOS_HEADER>(hInstance)->e_lfanew);
	PIMAGE_IMPORT_DESCRIPTOR imports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(
		reinterpret_cast<DWORD_PTR>(hInstance) +
		ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	for (; imports->Name != 0; ++imports)
	{
		if (_stricmp(reinterpret_cast<const char*>(reinterpret_cast<DWORD_PTR>(hInstance) + imports->Name),
			"KERNEL32.DLL") != 0 || imports->OriginalFirstThunk == 0)
		{
			continue;
		}

		PIMAGE_IMPORT_BY_NAME* functions = reinterpret_cast<PIMAGE_IMPORT_BY_NAME*>(
			reinterpret_cast<DWORD_PTR>(hInstance) + imports->OriginalFirstThunk);
		bool patchedLoadLibrary = false;
		bool patchedGetProcAddress = false;
		for (ptrdiff_t i = 0; functions[i] != nullptr; ++i)
		{
			const char* name = reinterpret_cast<const char*>(
				reinterpret_cast<DWORD_PTR>(hInstance) + functions[i]->Name);
			if (strcmp(name, "LoadLibraryA") == 0)
			{
				DWORD oldProtect;
				DWORD_PTR* address = &reinterpret_cast<DWORD_PTR*>(
					reinterpret_cast<DWORD_PTR>(hInstance) + imports->FirstThunk)[i];
				VirtualProtect(address, sizeof(*address), PAGE_EXECUTE_READWRITE, &oldProtect);
				pOrgLoadLibraryA = reinterpret_cast<decltype(pOrgLoadLibraryA)>(*address);
				*address = reinterpret_cast<DWORD_PTR>(LoadLibraryA_VCJPNoCD_Hook);
				VirtualProtect(address, sizeof(*address), oldProtect, &oldProtect);
				patchedLoadLibrary = true;
			}
			else if (strcmp(name, "GetProcAddress") == 0)
			{
				DWORD oldProtect;
				DWORD_PTR* address = &reinterpret_cast<DWORD_PTR*>(
					reinterpret_cast<DWORD_PTR>(hInstance) + imports->FirstThunk)[i];
				VirtualProtect(address, sizeof(*address), PAGE_EXECUTE_READWRITE, &oldProtect);
				pOrgGetProcAddress = reinterpret_cast<decltype(pOrgGetProcAddress)>(*address);
				*address = reinterpret_cast<DWORD_PTR>(GetProcAddress_VCJPNoCD_Hook);
				VirtualProtect(address, sizeof(*address), oldProtect, &oldProtect);
				patchedGetProcAddress = true;
			}
		}
		return patchedLoadLibrary && patchedGetProcAddress;
	}
	return false;
}
#endif

#if ENABLE_ENHANCEMENT_SKIP_INTRO_SPLASHES
static bool IsIniOptionEnabled(const wchar_t* iniName, const wchar_t* optionName)
{
	wchar_t path[MAX_PATH];
	if (GetModuleFileNameW(hThisModule, path, _countof(path)) == 0)
	{
		return false;
	}

	PathRemoveFileSpecW(path);
	PathAppendW(path, iniName);
	return GetPrivateProfileIntW(L"SilentPatch", optionName, 0, path) != 0;
}

static bool HasCommandLineArgument(const wchar_t* argument)
{
	int argumentCount;
	LPWSTR* arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (arguments == nullptr)
	{
		return false;
	}

	bool found = false;
	for (int i = 1; i < argumentCount; i++)
	{
		if (_wcsicmp(arguments[i], argument) == 0)
		{
			found = true;
			break;
		}
	}

	LocalFree(arguments);
	return found;
}
#endif

void InjectHooks()
{
	static char		aNoDesktopMode[64];

#if ENABLE_FIX_CPU_AFFINITY
	const wchar_t* affinityIniName = nullptr;
#endif

	const auto [width, height] = GetDesktopResolution();
	sprintf_s(aNoDesktopMode, "Cannot find %ux%ux32 video mode", width, height);

	auto Protect = ScopedUnprotect::SectionOrFullModule(GetModuleHandle(nullptr), ".text");

#if ENABLE_ENHANCEMENT_WINDOWED_MODE
#define INSTALL_WINDOWED_MODE(version) WindowedModeDDraw::Install##version(hThisModule)
#else
#define INSTALL_WINDOWED_MODE(version) false
#endif

	if (*(DWORD*)Memory::DynBaseAddress(0x5C1E75) == 0xB85548EC)
	{
		// III 1.0
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = III_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x580C16));
		INSTALL_WINDOWED_MODE(III10);
		Common::Patches::DDraw_III_10( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x5C2135) == 0xB85548EC)
	{
		// III 1.1
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = III_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x580F66));
		INSTALL_WINDOWED_MODE(III11);
		Common::Patches::DDraw_III_11( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x5C6FD5) == 0xB85548EC)
	{
		// III Steam
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = III_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x580E66));
		Common::Patches::DDraw_III_Steam( width, height, aNoDesktopMode );
	}

	else if (*(DWORD*)Memory::DynBaseAddress(0x667BF5) == 0xB85548EC)
	{
		// VC 1.0
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = VC_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x6022AA));
		INSTALL_WINDOWED_MODE(VC10);
		Common::Patches::DDraw_VC_10( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x667C45) == 0xB85548EC)
	{
		// VC 1.1
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = VC_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x60228A));
		INSTALL_WINDOWED_MODE(VC11);
		Common::Patches::DDraw_VC_11( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x666BA5) == 0xB85548EC)
	{
		// VC Steam
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = VC_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x601ECA));
		Common::Patches::DDraw_VC_Steam( width, height, aNoDesktopMode );
	}
	else if (*(DWORD*)Memory::DynBaseAddress(0x601048) == 0x5E5F5D60)
	{
		// VC Japanese
#if ENABLE_FIX_CPU_AFFINITY
		affinityIniName = VC_INI_NAME;
#endif
		ppUserFilesDir.Bind(Memory::DynBaseAddress((const char (**)[])0x60204A));

#if ENABLE_ENHANCEMENT_SKIP_INTRO_SPLASHES
#if defined(SILENTPATCH_SPEEDRUN)
		constexpr const wchar_t* iniName = L"SpeedrunSilentPatchVC.ini";
#else
		constexpr const wchar_t* iniName = L"SilentPatchVC.ini";
#endif
		if (IsIniOptionEnabled(iniName, L"SkipIntroSplashes") ||
				HasCommandLineArgument(L"/SkipIntroSplashes"))
		{
			// Skip the warning, Rockstar, Capcom, and GTA title movies. Redirect the first
			// movie state to GS_INIT_ONCE after its movie/COM cleanup, since no clip was opened.
			Memory::DynBase::Patch<uintptr_t>(0x6D3BF4, Memory::DynBaseAddress<uintptr_t>(0x600108));
		}
#endif

		INSTALL_WINDOWED_MODE(VCJP);
		Common::Patches::DDraw_VC_JP( width, height, aNoDesktopMode );
	}

#undef INSTALL_WINDOWED_MODE

#if ENABLE_FIX_CPU_AFFINITY
	if (affinityIniName != nullptr)
	{
		ApplyConfiguredProcessAffinity(affinityIniName);
	}
#endif

	Common::Patches::DDraw_Common();
	Memory::FlushCodeChanges();
}

static bool rwcsegUnprotected = false;

static void ProcHook()
{
	static bool		bPatched = false;
	if ( !bPatched )
	{
		bPatched = true;

		InjectHooks();

#if ENABLE_FIX_DEP_STARTUP_CRASH
		if ( !rwcsegUnprotected )
		{
			rwcsegUnprotected = Common::Patches::FixRwcseg_Patterns();
		}
#endif
	}
}

static VOID (WINAPI* pOrgGetStartupInfoA)(LPSTARTUPINFOA);
VOID WINAPI GetStartupInfoA_Hook(LPSTARTUPINFOA lpStartupInfo)
{
	ProcHook();
	pOrgGetStartupInfoA(lpStartupInfo);
}

static uint8_t orgCode[5];
static decltype(SystemParametersInfoA)* pOrgSystemParametersInfoA;
BOOL WINAPI SystemParametersInfoA_OverwritingHook( UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni )
{
	ProcHook();
	Memory::VP::Patch( pOrgSystemParametersInfoA, { orgCode[0], orgCode[1], orgCode[2], orgCode[3], orgCode[4] } );
	return pOrgSystemParametersInfoA( uiAction, uiParam, pvParam, fWinIni );
}

static bool FixRwcseg_Header()
{
	HINSTANCE					hInstance = GetModuleHandle(nullptr);
	PIMAGE_NT_HEADERS			ntHeader = (PIMAGE_NT_HEADERS)((DWORD_PTR)hInstance + ((PIMAGE_DOS_HEADER)hInstance)->e_lfanew);

	// Give _rwcseg proper access rights
	PIMAGE_SECTION_HEADER	pSection = IMAGE_FIRST_SECTION(ntHeader);

	for ( SIZE_T i = 0, j = ntHeader->FileHeader.NumberOfSections; i < j; i++, pSection++ )
	{
		if ( *(uint64_t*)(pSection->Name) == 0x006765736377725F )	// _rwcseg
		{
			DWORD	dwProtect;
			VirtualProtect((LPVOID)((DWORD_PTR)hInstance + pSection->VirtualAddress), pSection->Misc.VirtualSize, PAGE_EXECUTE_READ, &dwProtect);

			DWORD Characteristics = pSection->Characteristics;
			if ( (Characteristics & IMAGE_SCN_CNT_CODE) == 0 )
			{
				Characteristics |= IMAGE_SCN_CNT_CODE;
				Memory::VP::Patch( &ntHeader->OptionalHeader.SizeOfCode, ntHeader->OptionalHeader.SizeOfCode + pSection->Misc.VirtualSize );
			}
			if ( (Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA) != 0 )
			{
				Characteristics &= ~(IMAGE_SCN_CNT_INITIALIZED_DATA);
				Memory::VP::Patch( &ntHeader->OptionalHeader.SizeOfInitializedData, ntHeader->OptionalHeader.SizeOfInitializedData - pSection->Misc.VirtualSize );
			}
			if ( (Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA) != 0 )
			{
				Characteristics &= ~(IMAGE_SCN_CNT_UNINITIALIZED_DATA);
				Memory::VP::Patch( &ntHeader->OptionalHeader.SizeOfUninitializedData, ntHeader->OptionalHeader.SizeOfUninitializedData - pSection->Misc.VirtualSize );
			}
			Memory::VP::Patch( &pSection->Characteristics, Characteristics );
			return true;
		}
	}
	return false;
}

static bool PatchIAT()
{
	HINSTANCE					hInstance = GetModuleHandle(nullptr);
	PIMAGE_NT_HEADERS			ntHeader = (PIMAGE_NT_HEADERS)((DWORD_PTR)hInstance + ((PIMAGE_DOS_HEADER)hInstance)->e_lfanew);

	// Find IAT	
	PIMAGE_IMPORT_DESCRIPTOR	pImports = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD_PTR)hInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	// Find kernel32.dll
	for ( ; pImports->Name != 0; pImports++ )
	{
		if ( !_stricmp((const char*)((DWORD_PTR)hInstance + pImports->Name), "KERNEL32.DLL") )
		{
			if ( pImports->OriginalFirstThunk != 0 )
			{
				PIMAGE_IMPORT_BY_NAME*		pFunctions = (PIMAGE_IMPORT_BY_NAME*)((DWORD_PTR)hInstance + pImports->OriginalFirstThunk);

				// kernel32.dll found, find GetStartupInfoA
				for ( ptrdiff_t j = 0; pFunctions[j] != nullptr; j++ )
				{
					if ( !strcmp((const char*)((DWORD_PTR)hInstance + pFunctions[j]->Name), "GetStartupInfoA") )
					{
						// Overwrite the address with the address to a custom GetStartupInfoA
						DWORD			dwProtect[2];
						DWORD_PTR*		pAddress = &((DWORD_PTR*)((DWORD_PTR)hInstance + pImports->FirstThunk))[j];

						VirtualProtect(pAddress, sizeof(DWORD_PTR), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
						pOrgGetStartupInfoA = **(VOID(WINAPI**)(LPSTARTUPINFOA))pAddress;
						*pAddress = (DWORD_PTR)GetStartupInfoA_Hook;
						VirtualProtect(pAddress, sizeof(DWORD_PTR), dwProtect[0], &dwProtect[1]);

						return true;
					}
				}
			}
		}
	}
	return false;
}

static bool PatchIAT_ByPointers()
{
	using namespace Memory::VP;

	pOrgSystemParametersInfoA = SystemParametersInfoA;
	memcpy( orgCode, pOrgSystemParametersInfoA, sizeof(orgCode) );
	InjectHook( pOrgSystemParametersInfoA, SystemParametersInfoA_OverwritingHook, HookType::Jump );
	return true;
}

static void ApplyDDrawHooks()
{
#if ENABLE_FIX_DEP_STARTUP_CRASH
	rwcsegUnprotected = FixRwcseg_Header();
#endif

#if ENABLE_FIX_VC_JP_NO_CD_BOOTSTRAP
	// Install before the protected executable enters its unpacked startup code.
	if (*(DWORD*)Memory::DynBaseAddress(0x601048) == 0x5E5F5D60)
	{
		PatchVCJPNoCDBootstrapIAT();
	}
#endif

	bool getStartupInfoHooked = PatchIAT();
	if ( !getStartupInfoHooked )
	{
		PatchIAT_ByPointers();
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	UNREFERENCED_PARAMETER(hinstDLL);
	UNREFERENCED_PARAMETER(lpvReserved);

	if ( fdwReason == DLL_PROCESS_ATTACH )
	{
		hThisModule = hinstDLL;
		ApplyDDrawHooks();
	}

	return TRUE;
}

extern "C" __declspec(dllexport)
uint32_t GetBuildNumber()
{
	return (SILENTPATCH_REVISION_ID << 8) | SILENTPATCH_BUILD_ID;
}
