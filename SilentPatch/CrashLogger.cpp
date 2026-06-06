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
		g_previousFilter = SetUnhandledExceptionFilter(UnhandledExceptionFilter);
	}

	void Uninstall()
	{
		if (g_previousFilter != nullptr)
		{
			SetUnhandledExceptionFilter(g_previousFilter);
			g_previousFilter = nullptr;
		}
	}
}
