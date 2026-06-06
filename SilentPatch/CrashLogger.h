#pragma once

#include <windows.h>

namespace CrashLogger
{
	void Install(HINSTANCE module);
	void Uninstall();
}
