#pragma once

#include <windows.h>

namespace CrashLogger
{
	void Install(HINSTANCE module);
	void Uninstall();

#if defined(_GTA_SA)
	// Enables version-specific script context logging once the executable has
	// been identified. Until this is called, early crashes are still logged,
	// but the script layout is reported as unavailable.
	void SetSAGameVersion(int version);
#endif
}
