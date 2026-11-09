#pragma once

#include <windows.h>
#include <Objbase.h>
#include <ShObjIdl.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

// Let the Shell put the taskbar behind an active, screen-covering game window,
// including one with a frame. This does not make the game always-on-top.
class WindowedModeTaskbar
{
public:
	void OnWindowCreated(HWND hwnd)
	{
		if (TaskbarCreatedMessage == 0)
		{
			TaskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
		}
		MarkFullscreen(hwnd);
	}

	void HandleMessage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		// Hiding the window or restarting Explorer loses the marking. Resizing
		// and finishing a move can also change which monitor the window covers.
		// Activation retries a previous Shell failure without forcing focus.
		bool refresh = (TaskbarCreatedMessage != 0 && message == TaskbarCreatedMessage) ||
			(message == WM_SHOWWINDOW && wParam != FALSE) ||
			(message == WM_ACTIVATEAPP && wParam != FALSE) || message == WM_EXITSIZEMOVE;
		if (message == WM_WINDOWPOSCHANGED)
		{
			const WINDOWPOS* position = reinterpret_cast<const WINDOWPOS*>(lParam);
			refresh = position != nullptr &&
				((position->flags & SWP_SHOWWINDOW) != 0 || (position->flags & SWP_NOSIZE) == 0);
		}
		if (refresh)
		{
			MarkFullscreen(hwnd);
		}
	}

private:
	void MarkFullscreen(HWND hwnd)
	{
		if (Updating || hwnd == nullptr || IsWindowVisible(hwnd) == FALSE || IsIconic(hwnd) != FALSE)
		{
			return;
		}

		// COM can dispatch window messages. Avoid recursively creating another
		// taskbar object while applying the marking.
		Updating = true;
		const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE)
		{
			// Use an existing apartment of either type, and keep the interface local
			// so there is no stale Explorer object or COM lifetime tied to DLL unload.
			ITaskbarList2* taskbar = nullptr;
			if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))))
			{
				if (SUCCEEDED(taskbar->HrInit()))
				{
					taskbar->MarkFullscreenWindow(hwnd, TRUE);
				}
				taskbar->Release();
			}
			if (SUCCEEDED(initialized))
			{
				CoUninitialize();
			}
		}
		Updating = false;
	}

	UINT TaskbarCreatedMessage = 0;
	bool Updating = false;
};
