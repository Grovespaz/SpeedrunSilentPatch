#include "StdAfx.h"
#include "CrashLogger.h"

#pragma warning(push)
#pragma warning(disable:4091)
#include <DbgHelp.h>
#pragma warning(pop)
#include <TlHelp32.h>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace CrashLogger
{
	namespace
	{
#if defined(_GTA_III)
		const char* const GAME_NAME = "SilentPatchIII";
#if defined(SILENTPATCH_SPEEDRUN)
		const char* const LOG_FILE_NAME = "SpeedrunSilentPatchIII_crash.log";
#else
		const char* const LOG_FILE_NAME = "SilentPatchIII_crash.log";
#endif
#elif defined(_GTA_VC)
		const char* const GAME_NAME = "SilentPatchVC";
#if defined(SILENTPATCH_SPEEDRUN)
		const char* const LOG_FILE_NAME = "SpeedrunSilentPatchVC_crash.log";
#else
		const char* const LOG_FILE_NAME = "SilentPatchVC_crash.log";
#endif
#elif defined(_GTA_SA)
		const char* const GAME_NAME = "SilentPatchSA";
#if defined(SILENTPATCH_SPEEDRUN)
		const char* const LOG_FILE_NAME = "SpeedrunSilentPatchSA_crash.log";
#else
		const char* const LOG_FILE_NAME = "SilentPatchSA_crash.log";
#endif
#else
#error CrashLogger needs a GTA target define.
#endif

		HINSTANCE g_module = nullptr;
		LPTOP_LEVEL_EXCEPTION_FILTER g_previousFilter = nullptr;
		volatile LONG g_loggedCrash = 0;

#if defined(_GTA_SA)
		volatile LONG g_saGameVersion = -1;
#endif

		class LogFile
		{
		public:
			explicit LogFile(const char* path)
				: m_file(CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr))
			{
			}

			~LogFile()
			{
				if (m_file != INVALID_HANDLE_VALUE)
				{
					CloseHandle(m_file);
				}
			}

			bool IsOpen() const
			{
				return m_file != INVALID_HANDLE_VALUE;
			}

			void Write(const char* text)
			{
				if (!IsOpen() || text == nullptr)
				{
					return;
				}

				DWORD bytesWritten = 0;
				WriteFile(m_file, text, static_cast<DWORD>(std::strlen(text)), &bytesWritten, nullptr);
			}

			void Printf(const char* format, ...)
			{
				char buffer[2048];

				va_list args;
				va_start(args, format);
				vsprintf_s(buffer, format, args);
				va_end(args);

				Write(buffer);
			}

		private:
			HANDLE m_file;
		};

		const char* ExceptionName(DWORD code)
		{
			switch (code)
			{
			case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
			case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
			case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
			case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
			case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
			case EXCEPTION_FLT_INEXACT_RESULT: return "EXCEPTION_FLT_INEXACT_RESULT";
			case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
			case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
			case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
			case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
			case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
			case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
			case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
			case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
			case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
			case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
			case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
			case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
			default: return "UNKNOWN_EXCEPTION";
			}
		}

		void BuildLogPath(char* path, size_t pathSize)
		{
			path[0] = '\0';

			char gamePath[MAX_PATH];
			const DWORD length = GetModuleFileNameA(GetModuleHandle(nullptr), gamePath, static_cast<DWORD>(_countof(gamePath)));
			if (length == 0 || length >= _countof(gamePath))
			{
				strcpy_s(path, pathSize, LOG_FILE_NAME);
				return;
			}

			char* slash = std::strrchr(gamePath, '\\');
			char* forwardSlash = std::strrchr(gamePath, '/');
			if (forwardSlash != nullptr && (slash == nullptr || forwardSlash > slash))
			{
				slash = forwardSlash;
			}

			if (slash != nullptr)
			{
				*(slash + 1) = '\0';
				strcpy_s(path, pathSize, gamePath);
				strcat_s(path, pathSize, LOG_FILE_NAME);
			}
			else
			{
				strcpy_s(path, pathSize, LOG_FILE_NAME);
			}
		}

		void WriteModuleForAddress(LogFile& log, DWORD64 address)
		{
			HMODULE module = nullptr;
			const auto addressPtr = reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(address));
			if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, addressPtr, &module) != 0)
			{
				char modulePath[MAX_PATH];
				if (GetModuleFileNameA(module, modulePath, static_cast<DWORD>(_countof(modulePath))) != 0)
				{
					log.Printf(" (%s+0x%08" PRIX64 ")", modulePath, address - reinterpret_cast<DWORD64>(module));
				}
			}
		}

		void WriteExceptionInformation(LogFile& log, EXCEPTION_RECORD* record)
		{
			log.Printf("Exception: %s (0x%08X)\r\n", ExceptionName(record->ExceptionCode), record->ExceptionCode);
			log.Printf("Address:   0x%p", record->ExceptionAddress);
			WriteModuleForAddress(log, reinterpret_cast<DWORD64>(record->ExceptionAddress));
			log.Write("\r\n");
			log.Printf("Flags:     0x%08X\r\n", record->ExceptionFlags);

			if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
			{
				const ULONG_PTR accessType = record->ExceptionInformation[0];
				const char* accessName = accessType == 0 ? "read" : accessType == 1 ? "write" : accessType == 8 ? "DEP" : "unknown";
				log.Printf("Access:    %s at 0x%p\r\n", accessName, reinterpret_cast<void*>(record->ExceptionInformation[1]));
			}
			else if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && record->NumberParameters >= 3)
			{
				log.Printf("Access:    in-page error at 0x%p, NTSTATUS 0x%08X\r\n", reinterpret_cast<void*>(record->ExceptionInformation[1]), static_cast<DWORD>(record->ExceptionInformation[2]));
			}
		}

		void WriteRegisters(LogFile& log, const CONTEXT* context)
		{
#if defined(_M_IX86)
			log.Write("\r\nRegisters:\r\n");
			log.Printf("EAX=%08X EBX=%08X ECX=%08X EDX=%08X\r\n", context->Eax, context->Ebx, context->Ecx, context->Edx);
			log.Printf("ESI=%08X EDI=%08X EBP=%08X ESP=%08X\r\n", context->Esi, context->Edi, context->Ebp, context->Esp);
			log.Printf("EIP=%08X EFLAGS=%08X\r\n", context->Eip, context->EFlags);
#else
			UNREFERENCED_PARAMETER(log);
			UNREFERENCED_PARAMETER(context);
#endif
		}

#if defined(_GTA_SA) && defined(_M_IX86)
		struct SAScriptLayout
		{
			uintptr_t scriptSpace;
			size_t scriptSpaceSize;
			uintptr_t scripts;
			size_t scriptCount;
			size_t scriptSize;
			uintptr_t idleScripts;
			uintptr_t activeScripts;
		};

		// Classic GTA SA 1.0 layout. Other executable versions deliberately do
		// not fall back to these addresses, as reporting no context is preferable
		// to interpreting unrelated memory as a CRunningScript.
		constexpr SAScriptLayout SA_10_SCRIPT_LAYOUT = {
			0x00A49960, 200000,
			0x00A8B430, 96, 0xE0,
			0x00A8B428, 0x00A8B42C
		};

		constexpr size_t SA_SCRIPT_OFFSET_PREVIOUS = 0x00;
		constexpr size_t SA_SCRIPT_OFFSET_NEXT = 0x04;
		constexpr size_t SA_SCRIPT_OFFSET_NAME = 0x08;
		constexpr size_t SA_SCRIPT_OFFSET_BASE_IP = 0x10;
		constexpr size_t SA_SCRIPT_OFFSET_CURRENT_IP = 0x14;
		constexpr size_t SA_SCRIPT_OFFSET_STACK_POINTER = 0x38;
		constexpr size_t SA_SCRIPT_OFFSET_IS_ACTIVE = 0xC4;
		constexpr size_t SA_SCRIPT_OFFSET_CONDITION_RESULT = 0xC5;
		constexpr size_t SA_SCRIPT_OFFSET_USE_MISSION_CLEANUP = 0xC6;
		constexpr size_t SA_SCRIPT_OFFSET_IS_EXTERNAL = 0xC7;
		constexpr size_t SA_SCRIPT_OFFSET_WAKE_TIME = 0xCC;
		constexpr size_t SA_SCRIPT_OFFSET_IS_MISSION = 0xDC;

		bool IsReadableProtection(DWORD protection)
		{
			if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
			{
				return false;
			}

			switch (protection & 0xFF)
			{
			case PAGE_READONLY:
			case PAGE_READWRITE:
			case PAGE_WRITECOPY:
			case PAGE_EXECUTE_READ:
			case PAGE_EXECUTE_READWRITE:
			case PAGE_EXECUTE_WRITECOPY:
				return true;
			default:
				return false;
			}
		}

		bool SafeCopyMemory(void* destination, const void* source, size_t size)
		{
			if (size == 0)
			{
				return true;
			}
			if (destination == nullptr || source == nullptr)
			{
				return false;
			}

			const uintptr_t firstAddress = reinterpret_cast<uintptr_t>(source);
			if (firstAddress > uintptr_t(-1) - size)
			{
				return false;
			}

			uintptr_t currentAddress = firstAddress;
			auto* output = static_cast<unsigned char*>(destination);
			size_t remaining = size;

			while (remaining != 0)
			{
				MEMORY_BASIC_INFORMATION memoryInfo = {};
				if (VirtualQuery(reinterpret_cast<const void*>(currentAddress), &memoryInfo, sizeof(memoryInfo)) != sizeof(memoryInfo) ||
					memoryInfo.State != MEM_COMMIT || !IsReadableProtection(memoryInfo.Protect))
				{
					return false;
				}

				const uintptr_t regionStart = reinterpret_cast<uintptr_t>(memoryInfo.BaseAddress);
				if (currentAddress < regionStart)
				{
					return false;
				}

				const size_t offsetInRegion = currentAddress - regionStart;
				if (offsetInRegion >= memoryInfo.RegionSize)
				{
					return false;
				}

				const size_t available = memoryInfo.RegionSize - offsetInRegion;
				const size_t toCopy = remaining < available ? remaining : available;
				__try
				{
					std::memcpy(output, reinterpret_cast<const void*>(currentAddress), toCopy);
				}
				__except (EXCEPTION_EXECUTE_HANDLER)
				{
					return false;
				}

				currentAddress += toCopy;
				output += toCopy;
				remaining -= toCopy;
			}

			return true;
		}

		template <typename T>
		bool SafeRead(uintptr_t address, T& value)
		{
			return SafeCopyMemory(&value, reinterpret_cast<const void*>(address), sizeof(value));
		}

		template <typename T>
		T ReadSnapshotValue(const unsigned char* snapshot, size_t offset)
		{
			T value = {};
			std::memcpy(&value, snapshot + offset, sizeof(value));
			return value;
		}

		bool GetSAScriptLayout(SAScriptLayout& layout, LONG& version)
		{
			version = InterlockedCompareExchange(&g_saGameVersion, 0, 0);
			if (version != 0)
			{
				return false;
			}

			layout = SA_10_SCRIPT_LAYOUT;
			return true;
		}

		bool GetSAScriptSlot(const SAScriptLayout& layout, uintptr_t address, size_t& slot)
		{
			if (address < layout.scripts || layout.scriptSize == 0)
			{
				return false;
			}

			const uintptr_t offset = address - layout.scripts;
			if ((offset % layout.scriptSize) != 0)
			{
				return false;
			}

			const uintptr_t candidateSlot = offset / layout.scriptSize;
			if (candidateSlot >= layout.scriptCount)
			{
				return false;
			}

			slot = static_cast<size_t>(candidateSlot);
			return true;
		}

		bool GetOffsetInRange(uintptr_t address, uintptr_t rangeStart, size_t rangeSize, size_t& offset)
		{
			if (address < rangeStart)
			{
				return false;
			}

			const uintptr_t rangeOffset = address - rangeStart;
			if (rangeOffset >= rangeSize)
			{
				return false;
			}

			offset = static_cast<size_t>(rangeOffset);
			return true;
		}

		bool AddSignedOffset(uintptr_t address, int offset, uintptr_t& result)
		{
			if (offset < 0)
			{
				const uintptr_t distance = static_cast<uintptr_t>(-offset);
				if (address < distance)
				{
					return false;
				}
				result = address - distance;
				return true;
			}

			const uintptr_t distance = static_cast<uintptr_t>(offset);
			if (address > uintptr_t(-1) - distance)
			{
				return false;
			}
			result = address + distance;
			return true;
		}

		void WriteScriptListPointer(LogFile& log, const char* name, uintptr_t storageAddress, const SAScriptLayout& layout)
		{
			DWORD pointerValue = 0;
			if (!SafeRead(storageAddress, pointerValue))
			{
				log.Printf("  %-7s <unreadable at 0x%p>\r\n", name, reinterpret_cast<void*>(storageAddress));
				return;
			}

			size_t slot = 0;
			if (pointerValue == 0)
			{
				log.Printf("  %-7s 0x00000000 (null)\r\n", name);
			}
			else if (GetSAScriptSlot(layout, pointerValue, slot))
			{
				log.Printf("  %-7s 0x%08X (slot %zu)\r\n", name, pointerValue, slot);
			}
			else
			{
				log.Printf("  %-7s 0x%08X (outside script array)\r\n", name, pointerValue);
			}
		}

		void WriteMemoryWindow(LogFile& log, uintptr_t currentIP)
		{
			if (currentIP == 0)
			{
				log.Write("    Bytes around CurrentIP: unavailable (null pointer)\r\n");
				return;
			}

			log.Write("    Bytes around CurrentIP (-16..+31; ?? = unreadable):\r\n");
			for (int lineOffset = -16; lineOffset <= 16; lineOffset += 16)
			{
				uintptr_t lineAddress = 0;
				if (!AddSignedOffset(currentIP, lineOffset, lineAddress))
				{
					log.Printf("      %c <address overflow>\r\n", lineOffset == 0 ? '>' : ' ');
					continue;
				}

				char bytes[(16 * 3) + 1] = {};
				size_t outputOffset = 0;
				for (int byteIndex = 0; byteIndex < 16; ++byteIndex)
				{
					uintptr_t byteAddress = 0;
					unsigned char value = 0;
					const bool readable = AddSignedOffset(lineAddress, byteIndex, byteAddress) && SafeRead(byteAddress, value);
					const int written = readable
						? sprintf_s(bytes + outputOffset, _countof(bytes) - outputOffset, "%02X ", value)
						: sprintf_s(bytes + outputOffset, _countof(bytes) - outputOffset, "?? ");
					if (written <= 0)
					{
						break;
					}
					outputOffset += static_cast<size_t>(written);
				}

				log.Printf("      %c 0x%p: %s\r\n", lineOffset == 0 ? '>' : ' ',
					reinterpret_cast<void*>(lineAddress), bytes);
			}
		}

		struct SAScriptCandidate
		{
			uintptr_t address;
			const char* registerName;
			int stackOffset;
		};

		bool AddSAScriptCandidate(SAScriptCandidate* candidates, size_t& candidateCount, size_t candidateCapacity,
			uintptr_t address, const char* registerName, int stackOffset, const SAScriptLayout& layout)
		{
			size_t ignoredSlot = 0;
			if (!GetSAScriptSlot(layout, address, ignoredSlot))
			{
				return false;
			}

			for (size_t i = 0; i < candidateCount; ++i)
			{
				if (candidates[i].address == address)
				{
					return true;
				}
			}

			if (candidateCount >= candidateCapacity)
			{
				return false;
			}

			candidates[candidateCount++] = { address, registerName, stackOffset };
			return true;
		}

		void WriteSAScriptCandidate(LogFile& log, const SAScriptCandidate& candidate, const SAScriptLayout& layout)
		{
			size_t slot = 0;
			GetSAScriptSlot(layout, candidate.address, slot);
			if (candidate.registerName != nullptr)
			{
				log.Printf("\r\n  Candidate slot %zu from %s = 0x%p:\r\n", slot, candidate.registerName,
					reinterpret_cast<void*>(candidate.address));
			}
			else
			{
				log.Printf("\r\n  Candidate slot %zu from [ESP+0x%02X] = 0x%p:\r\n", slot, candidate.stackOffset,
					reinterpret_cast<void*>(candidate.address));
			}

			unsigned char snapshot[0xE0] = {};
			if (!SafeCopyMemory(snapshot, reinterpret_cast<const void*>(candidate.address), sizeof(snapshot)))
			{
				log.Write("    Script object is not fully readable.\r\n");
				return;
			}

			char printableName[9] = {};
			for (size_t i = 0; i < 8; ++i)
			{
				const unsigned char value = snapshot[SA_SCRIPT_OFFSET_NAME + i];
				printableName[i] = value >= 0x20 && value <= 0x7E ? static_cast<char>(value) : '.';
			}

			const DWORD previous = ReadSnapshotValue<DWORD>(snapshot, SA_SCRIPT_OFFSET_PREVIOUS);
			const DWORD next = ReadSnapshotValue<DWORD>(snapshot, SA_SCRIPT_OFFSET_NEXT);
			const DWORD baseIP = ReadSnapshotValue<DWORD>(snapshot, SA_SCRIPT_OFFSET_BASE_IP);
			const DWORD currentIP = ReadSnapshotValue<DWORD>(snapshot, SA_SCRIPT_OFFSET_CURRENT_IP);
			const WORD stackPointer = ReadSnapshotValue<WORD>(snapshot, SA_SCRIPT_OFFSET_STACK_POINTER);
			const DWORD wakeTime = ReadSnapshotValue<DWORD>(snapshot, SA_SCRIPT_OFFSET_WAKE_TIME);

			log.Printf("    Name:       \"%s\" (hex %02X %02X %02X %02X %02X %02X %02X %02X)\r\n",
				printableName,
				snapshot[SA_SCRIPT_OFFSET_NAME + 0], snapshot[SA_SCRIPT_OFFSET_NAME + 1],
				snapshot[SA_SCRIPT_OFFSET_NAME + 2], snapshot[SA_SCRIPT_OFFSET_NAME + 3],
				snapshot[SA_SCRIPT_OFFSET_NAME + 4], snapshot[SA_SCRIPT_OFFSET_NAME + 5],
				snapshot[SA_SCRIPT_OFFSET_NAME + 6], snapshot[SA_SCRIPT_OFFSET_NAME + 7]);
			log.Printf("    Links:      previous=0x%08X next=0x%08X\r\n", previous, next);
			log.Printf("    BaseIP:     0x%08X\r\n", baseIP);
			log.Printf("    CurrentIP:  0x%08X\r\n", currentIP);
			log.Printf("    State:      active=%u condition=%u missionCleanup=%u external=%u mission=%u SP=%u wakeTime=%u\r\n",
				snapshot[SA_SCRIPT_OFFSET_IS_ACTIVE], snapshot[SA_SCRIPT_OFFSET_CONDITION_RESULT],
				snapshot[SA_SCRIPT_OFFSET_USE_MISSION_CLEANUP], snapshot[SA_SCRIPT_OFFSET_IS_EXTERNAL],
				snapshot[SA_SCRIPT_OFFSET_IS_MISSION], stackPointer, wakeTime);

			size_t scriptSpaceOffset = 0;
			if (GetOffsetInRange(currentIP, layout.scriptSpace, layout.scriptSpaceSize, scriptSpaceOffset))
			{
				log.Printf("    SCM offset: 0x%zX (CurrentIP is inside ScriptSpace)\r\n", scriptSpaceOffset);
			}
			else
			{
				log.Write("    SCM offset: unavailable (CurrentIP is outside ScriptSpace)\r\n");
			}

			if (baseIP != 0 && currentIP >= baseIP && (currentIP - baseIP) <= 0x01000000)
			{
				log.Printf("    IP relative to BaseIP: 0x%X\r\n", currentIP - baseIP);
			}
			else
			{
				log.Write("    IP relative to BaseIP: unavailable or implausible\r\n");
			}

			WriteMemoryWindow(log, currentIP);
		}

		void WriteSAScriptContextImpl(LogFile& log, const CONTEXT* context)
		{
			log.Write("\r\nSA script context:\r\n");

			LONG version = -1;
			SAScriptLayout layout = {};
			if (!GetSAScriptLayout(layout, version))
			{
				if (version < 0)
				{
					log.Write("  Unavailable: the game version was not identified before the crash.\r\n");
				}
				else
				{
					log.Printf("  Unavailable: no safe script layout is known for game version %ld.\r\n", version);
				}
				return;
			}

			log.Write("  Layout: GTA SA 1.0, ScriptSpace=0x00A49960, Scripts=0x00A8B430 (96 x 0xE0)\r\n");
			WriteScriptListPointer(log, "Idle:", layout.idleScripts, layout);
			WriteScriptListPointer(log, "Active:", layout.activeScripts, layout);

			if (context == nullptr)
			{
				log.Write("  No CPU context is available for script candidate discovery.\r\n");
				return;
			}

			SAScriptCandidate candidates[8] = {};
			size_t candidateCount = 0;
			const struct
			{
				const char* name;
				DWORD value;
			} registers[] = {
				{ "ECX", context->Ecx }, { "ESI", context->Esi }, { "EDI", context->Edi },
				{ "EBX", context->Ebx }, { "EAX", context->Eax }, { "EDX", context->Edx },
				{ "EBP", context->Ebp }
			};

			for (const auto& reg : registers)
			{
				AddSAScriptCandidate(candidates, candidateCount, _countof(candidates), reg.value, reg.name, -1, layout);
			}

			// If no register retains `this`, inspect only the first 128 bytes of the
			// captured stack.
			if (candidateCount == 0)
			{
				for (int offset = 0; offset < 0x80 && candidateCount < _countof(candidates); offset += sizeof(DWORD))
				{
					uintptr_t stackAddress = 0;
					DWORD value = 0;
					if (AddSignedOffset(context->Esp, offset, stackAddress) && SafeRead(stackAddress, value))
					{
						AddSAScriptCandidate(candidates, candidateCount, _countof(candidates), value, nullptr, offset, layout);
					}
				}
			}

			if (candidateCount == 0)
			{
				log.Write("  No register or bounded near-stack value points to an aligned script slot.\r\n");
				log.Write("  This is expected for crashes before script startup and for crashes outside the SCM interpreter.\r\n");
				return;
			}

			for (size_t i = 0; i < candidateCount; ++i)
			{
				WriteSAScriptCandidate(log, candidates[i], layout);
			}
		}

		void WriteSAScriptContext(LogFile& log, const CONTEXT* context)
		{
			__try
			{
				WriteSAScriptContextImpl(log, context);
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				log.Write("\r\nSA script context:\r\n  Logging aborted after an unexpected memory access failure.\r\n");
			}
		}
#endif

		struct DbgHelpApi
		{
			HMODULE module = nullptr;
			decltype(&SymInitialize) symInitialize = nullptr;
			decltype(&SymCleanup) symCleanup = nullptr;
			decltype(&SymSetOptions) symSetOptions = nullptr;
			decltype(&StackWalk64) stackWalk64 = nullptr;
			decltype(&SymFunctionTableAccess64) symFunctionTableAccess64 = nullptr;
			decltype(&SymGetModuleBase64) symGetModuleBase64 = nullptr;
			decltype(&SymFromAddr) symFromAddr = nullptr;
			decltype(&SymGetLineFromAddr64) symGetLineFromAddr64 = nullptr;

			~DbgHelpApi()
			{
				if (module != nullptr)
				{
					FreeLibrary(module);
				}
			}

			bool Load()
			{
				module = LoadLibraryA("dbghelp.dll");
				if (module == nullptr)
				{
					return false;
				}

				symInitialize = reinterpret_cast<decltype(symInitialize)>(GetProcAddress(module, "SymInitialize"));
				symCleanup = reinterpret_cast<decltype(symCleanup)>(GetProcAddress(module, "SymCleanup"));
				symSetOptions = reinterpret_cast<decltype(symSetOptions)>(GetProcAddress(module, "SymSetOptions"));
				stackWalk64 = reinterpret_cast<decltype(stackWalk64)>(GetProcAddress(module, "StackWalk64"));
				symFunctionTableAccess64 = reinterpret_cast<decltype(symFunctionTableAccess64)>(GetProcAddress(module, "SymFunctionTableAccess64"));
				symGetModuleBase64 = reinterpret_cast<decltype(symGetModuleBase64)>(GetProcAddress(module, "SymGetModuleBase64"));
				symFromAddr = reinterpret_cast<decltype(symFromAddr)>(GetProcAddress(module, "SymFromAddr"));
				symGetLineFromAddr64 = reinterpret_cast<decltype(symGetLineFromAddr64)>(GetProcAddress(module, "SymGetLineFromAddr64"));

				return symInitialize != nullptr && symCleanup != nullptr && stackWalk64 != nullptr &&
					symFunctionTableAccess64 != nullptr && symGetModuleBase64 != nullptr;
			}
		};

		void WriteStackTrace(LogFile& log, const CONTEXT* context)
		{
#if defined(_M_IX86)
			DbgHelpApi dbgHelp;
			if (!dbgHelp.Load())
			{
				log.Write("\r\nStack trace: dbghelp.dll is not available or is missing required exports.\r\n");
				return;
			}

			const HANDLE process = GetCurrentProcess();
			if (dbgHelp.symSetOptions != nullptr)
			{
				dbgHelp.symSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
			}

			if (dbgHelp.symInitialize(process, nullptr, TRUE) == FALSE)
			{
				log.Printf("\r\nStack trace: SymInitialize failed, GetLastError=%lu\r\n", GetLastError());
				return;
			}

			CONTEXT walkContext = *context;
			STACKFRAME64 frame = {};
			frame.AddrPC.Offset = walkContext.Eip;
			frame.AddrPC.Mode = AddrModeFlat;
			frame.AddrFrame.Offset = walkContext.Ebp;
			frame.AddrFrame.Mode = AddrModeFlat;
			frame.AddrStack.Offset = walkContext.Esp;
			frame.AddrStack.Mode = AddrModeFlat;

			log.Write("\r\nStack trace:\r\n");
			for (int frameIndex = 0; frameIndex < 64; ++frameIndex)
			{
				if (frame.AddrPC.Offset == 0)
				{
					break;
				}

				log.Printf("#%02d 0x%08" PRIX64, frameIndex, frame.AddrPC.Offset);
				WriteModuleForAddress(log, frame.AddrPC.Offset);

				if (dbgHelp.symFromAddr != nullptr)
				{
					alignas(SYMBOL_INFO) char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
					SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
					symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
					symbol->MaxNameLen = MAX_SYM_NAME;

					DWORD64 displacement = 0;
					if (dbgHelp.symFromAddr(process, frame.AddrPC.Offset, &displacement, symbol) != FALSE)
					{
						log.Printf(" %s+0x%" PRIX64, symbol->Name, displacement);
					}
				}

				if (dbgHelp.symGetLineFromAddr64 != nullptr)
				{
					IMAGEHLP_LINE64 line = {};
					line.SizeOfStruct = sizeof(line);
					DWORD displacement = 0;
					if (dbgHelp.symGetLineFromAddr64(process, frame.AddrPC.Offset, &displacement, &line) != FALSE)
					{
						log.Printf(" (%s:%lu)", line.FileName, line.LineNumber);
					}
				}

				log.Write("\r\n");

				if (dbgHelp.stackWalk64(IMAGE_FILE_MACHINE_I386, process, GetCurrentThread(), &frame, &walkContext, nullptr,
					dbgHelp.symFunctionTableAccess64, dbgHelp.symGetModuleBase64, nullptr) == FALSE)
				{
					break;
				}
			}

			dbgHelp.symCleanup(process);
#else
			UNREFERENCED_PARAMETER(log);
			UNREFERENCED_PARAMETER(context);
#endif
		}

		void WriteModuleList(LogFile& log)
		{
			const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
			if (snapshot == INVALID_HANDLE_VALUE)
			{
				log.Printf("\r\nModules: CreateToolhelp32Snapshot failed, GetLastError=%lu\r\n", GetLastError());
				return;
			}

			log.Write("\r\nModules:\r\n");
			MODULEENTRY32 module = {};
			module.dwSize = sizeof(module);
			if (Module32First(snapshot, &module) != FALSE)
			{
				do
				{
					log.Printf("0x%p-0x%p %8lu %S (%S)\r\n",
						module.modBaseAddr,
						module.modBaseAddr + module.modBaseSize,
						module.modBaseSize,
						module.szModule,
						module.szExePath);
				}
				while (Module32Next(snapshot, &module) != FALSE);
			}

			CloseHandle(snapshot);
		}

		void WriteCrashLog(EXCEPTION_POINTERS* exceptionInfo)
		{
			if (InterlockedCompareExchange(&g_loggedCrash, 1, 0) != 0)
			{
				return;
			}

			char logPath[MAX_PATH];
			BuildLogPath(logPath, _countof(logPath));

			LogFile log(logPath);
			if (!log.IsOpen())
			{
				return;
			}

			SYSTEMTIME time;
			GetLocalTime(&time);

			log.Write("\r\n============================================================\r\n");
			log.Printf("%s crash log\r\n", GAME_NAME);
			log.Printf("Time:      %04u-%02u-%02u %02u:%02u:%02u.%03u\r\n",
				time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);
			log.Printf("Process:   %lu\r\n", GetCurrentProcessId());
			log.Printf("Thread:    %lu\r\n", GetCurrentThreadId());
			log.Printf("Module:    0x%p\r\n", g_module);
			log.Printf("Build:     %u.%u\r\n", SILENTPATCH_REVISION_ID, SILENTPATCH_BUILD_ID);
#if defined(SILENTPATCH_SPEEDRUN)
			log.Write("Feature:   Speedrun\r\n");
#else
			log.Write("Feature:   Full\r\n");
#endif

			WriteExceptionInformation(log, exceptionInfo->ExceptionRecord);
			WriteRegisters(log, exceptionInfo->ContextRecord);
#if defined(_GTA_SA) && defined(_M_IX86)
			WriteSAScriptContext(log, exceptionInfo->ContextRecord);
#endif
			WriteStackTrace(log, exceptionInfo->ContextRecord);
			WriteModuleList(log);
			log.Write("============================================================\r\n");
		}

		LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
		{
			WriteCrashLog(exceptionInfo);

			if (g_previousFilter != nullptr && g_previousFilter != UnhandledExceptionFilter)
			{
				const LONG previousResult = g_previousFilter(exceptionInfo);
				if (previousResult != EXCEPTION_CONTINUE_SEARCH)
				{
					return previousResult;
				}
			}

			return EXCEPTION_CONTINUE_SEARCH;
		}
	}

	void Install(HINSTANCE module)
	{
		g_module = module;
#if defined(_GTA_SA)
		InterlockedExchange(&g_saGameVersion, -1);
#endif
		g_previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilter);
	}

#if defined(_GTA_SA)
	void SetSAGameVersion(int version)
	{
		InterlockedExchange(&g_saGameVersion, version);
	}
#endif

	void Uninstall()
	{
#if defined(_GTA_SA)
		InterlockedExchange(&g_saGameVersion, -1);
#endif
		if (g_previousFilter != nullptr)
		{
			SetUnhandledExceptionFilter(g_previousFilter);
			g_previousFilter = nullptr;
		}
	}
}
