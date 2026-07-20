#pragma once

#include <windows.h>

namespace WindowedModeDDraw
{
	bool InstallIII10(HINSTANCE module);
	bool InstallIII11(HINSTANCE module);
	bool InstallVC10(HINSTANCE module);
	bool InstallVC11(HINSTANCE module);
	bool InstallVCJP(HINSTANCE module);
	bool IsFramedMode();
}
