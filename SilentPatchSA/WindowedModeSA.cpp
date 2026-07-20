#include "StdAfxSA.h"

#include "WindowedModeSA.h"

#include <d3d9.h>
#include <Shlwapi.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

extern char* GetMyDocumentsPathSA();

namespace
{
	enum class WindowedMode
	{
		Off,
		Framed,
		Borderless,
	};

	constexpr POINT ResolutionMin = { 160, 112 };
	constexpr DWORD FramedWindowStyle = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)) | WS_VISIBLE | WS_CLIPSIBLINGS;
	constexpr DWORD BorderlessWindowStyle = WS_POPUP | WS_VISIBLE | WS_CLIPSIBLINGS;
	constexpr char WindowStateIniName[] = "SpeedrunSilentPatchSA.WindowedMode.ini";
	constexpr char WindowStateSection[] = "WindowedMode";
	constexpr char WindowStateLeftKey[] = "Left";
	constexpr char WindowStateTopKey[] = "Top";

	struct PsGlobalType
	{
		HWND window;
		HINSTANCE instance;
		uint32_t fullScreen;
	};

	struct RsGlobalType
	{
		const char* AppName;
		int32_t MaximumWidth;
		int32_t MaximumHeight;
		uint32_t frameLimit;
		BOOL quit;
		PsGlobalType* ps;
	};

	struct DisplayMode
	{
		uint32_t width;
		uint32_t height;
		uint32_t refreshRate;
		D3DFORMAT format;
		uint32_t flags;
	};

	HWND Window = nullptr;
	WNDPROC OriginalWndProc = nullptr;
	POINT ClientSize = { 800, 600 };
	int ForcedCursorShowCount = 0;
	uintptr_t InitPresentationReturn = 0;
	uintptr_t InitD3dDeviceReturn = 0;
	std::vector<DisplayMode> VideoModesBackup;
	uint32_t PreviousVideoMode = UINT32_MAX;
	WindowedMode CurrentWindowedMode = WindowedMode::Off;
	char WindowStatePath[MAX_PATH] = {};

	using ResetFunc = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
	ResetFunc ResetOriginal = nullptr;
	template<std::size_t Index>
	void(*SetCurrentVideoModeOriginal)(int32_t modeIndex) = nullptr;

	template<typename T>
	T* Ptr(uintptr_t address)
	{
		return reinterpret_cast<T*>(address);
	}

	WindowedMode ParseWindowedModeOption(const wchar_t* value)
	{
		if (lstrcmpiW(value, L"1") == 0 || lstrcmpiW(value, L"Framed") == 0)
		{
			return WindowedMode::Framed;
		}
		if (lstrcmpiW(value, L"2") == 0 || lstrcmpiW(value, L"Borderless") == 0)
		{
			return WindowedMode::Borderless;
		}
		return WindowedMode::Off;
	}

	WindowedMode ReadWindowedModeOption(const wchar_t* iniPath)
	{
		wchar_t value[32];
		GetPrivateProfileStringW(L"SilentPatch", L"WindowedMode", L"0", value, _countof(value), iniPath);
		return ParseWindowedModeOption(value);
	}

	DWORD GetWindowedModeWindowStyle()
	{
		return CurrentWindowedMode == WindowedMode::Borderless ? BorderlessWindowStyle : FramedWindowStyle;
	}

	bool InitWindowStatePath()
	{
		WindowStatePath[0] = '\0';

		const char* userFilesPath = GetMyDocumentsPathSA();
		if (userFilesPath == nullptr || userFilesPath[0] == '\0')
		{
			return false;
		}

		strcpy_s(WindowStatePath, userFilesPath);
		if (PathAppendA(WindowStatePath, WindowStateIniName) == FALSE)
		{
			WindowStatePath[0] = '\0';
			return false;
		}
		return true;
	}

	bool HasWindowStatePath()
	{
		return WindowStatePath[0] != '\0';
	}

	bool ParseProfileLong(const char* value, LONG* result)
	{
		char* end = nullptr;
		const long parsed = strtol(value, &end, 10);
		if (end == value)
		{
			return false;
		}
		while (*end == ' ' || *end == '\t')
		{
			++end;
		}
		if (*end != '\0')
		{
			return false;
		}
		*result = static_cast<LONG>(parsed);
		return true;
	}

	bool ReadProfileLong(const char* key, LONG* value)
	{
		if (!HasWindowStatePath())
		{
			return false;
		}

		char buffer[32];
		if (GetPrivateProfileStringA(WindowStateSection, key, "", buffer, _countof(buffer), WindowStatePath) == 0)
		{
			return false;
		}
		return ParseProfileLong(buffer, value);
	}

	void WriteProfileLong(const char* key, LONG value)
	{
		if (!HasWindowStatePath())
		{
			return;
		}

		char buffer[32];
		sprintf_s(buffer, "%ld", value);
		WritePrivateProfileStringA(WindowStateSection, key, buffer, WindowStatePath);
	}

	POINT ClampClientSize(POINT size)
	{
		if (size.x < ResolutionMin.x)
		{
			size.x = ResolutionMin.x;
		}
		if (size.y < ResolutionMin.y)
		{
			size.y = ResolutionMin.y;
		}
		return size;
	}

	RECT GetNearestMonitorRect(POINT point)
	{
		HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info = { sizeof(info) };
		if (GetMonitorInfoW(monitor, &info) == FALSE)
		{
			info.rcMonitor = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
			info.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		}
		return CurrentWindowedMode == WindowedMode::Borderless ? info.rcMonitor : info.rcWork;
	}

	LONG ClampLong(LONG value, LONG minValue, LONG maxValue)
	{
		if (value < minValue)
		{
			return minValue;
		}
		if (value > maxValue)
		{
			return maxValue;
		}
		return value;
	}

	POINT ClampWindowPosition(POINT position, LONG windowWidth, LONG windowHeight, const RECT& monitorRect)
	{
		const LONG monitorWidth = monitorRect.right - monitorRect.left;
		const LONG monitorHeight = monitorRect.bottom - monitorRect.top;

		if (windowWidth >= monitorWidth)
		{
			position.x = monitorRect.left;
		}
		else
		{
			position.x = ClampLong(position.x, monitorRect.left, monitorRect.right - windowWidth);
		}

		if (windowHeight >= monitorHeight)
		{
			position.y = monitorRect.top;
		}
		else
		{
			position.y = ClampLong(position.y, monitorRect.top, monitorRect.bottom - windowHeight);
		}

		return position;
	}

	bool IsWindowRectSane(const RECT& rect)
	{
		if (rect.right <= rect.left || rect.bottom <= rect.top)
		{
			return false;
		}

		HMONITOR monitor = MonitorFromRect(&rect, MONITOR_DEFAULTTONULL);
		if (monitor == nullptr)
		{
			return false;
		}

		MONITORINFO info = { sizeof(info) };
		if (GetMonitorInfoW(monitor, &info) == FALSE)
		{
			return false;
		}

		RECT visibleRect;
		if (IntersectRect(&visibleRect, &rect, &info.rcWork) == FALSE)
		{
			return false;
		}

		const LONG windowWidth = rect.right - rect.left;
		const LONG windowHeight = rect.bottom - rect.top;
		const LONG minVisibleWidth = windowWidth < 64 ? windowWidth : 64;
		const LONG minVisibleHeight = windowHeight < 64 ? windowHeight : 64;
		return (visibleRect.right - visibleRect.left) >= minVisibleWidth &&
			(visibleRect.bottom - visibleRect.top) >= minVisibleHeight;
	}

	bool GetSavedWindowPosition(LONG windowWidth, LONG windowHeight, POINT* position)
	{
		LONG left = 0;
		LONG top = 0;
		if (!ReadProfileLong(WindowStateLeftKey, &left) || !ReadProfileLong(WindowStateTopKey, &top))
		{
			return false;
		}

		const RECT savedRect = { left, top, left + windowWidth, top + windowHeight };
		if (!IsWindowRectSane(savedRect))
		{
			return false;
		}

		const POINT savedCenter = {
			left + windowWidth / 2,
			top + windowHeight / 2
		};
		*position = ClampWindowPosition({ left, top }, windowWidth, windowHeight, GetNearestMonitorRect(savedCenter));
		return true;
	}

	POINT GetDefaultWindowPosition(LONG windowWidth, LONG windowHeight, POINT centerPoint, bool useSavedPosition = true)
	{
		POINT position = {};
		if (useSavedPosition && GetSavedWindowPosition(windowWidth, windowHeight, &position))
		{
			return position;
		}

		const RECT monitorRect = GetNearestMonitorRect(centerPoint);
		position.x = monitorRect.left + ((monitorRect.right - monitorRect.left) - windowWidth) / 2;
		position.y = monitorRect.top + ((monitorRect.bottom - monitorRect.top) - windowHeight) / 2;
		return ClampWindowPosition(position, windowWidth, windowHeight, monitorRect);
	}

	POINT GetWindowClientCenter(HWND hwnd)
	{
		RECT clientRect;
		if (GetClientRect(hwnd, &clientRect) != FALSE)
		{
			POINT clientTopLeft = { clientRect.left, clientRect.top };
			POINT clientBottomRight = { clientRect.right, clientRect.bottom };
			if (ClientToScreen(hwnd, &clientTopLeft) != FALSE && ClientToScreen(hwnd, &clientBottomRight) != FALSE)
			{
				return {
					(clientTopLeft.x + clientBottomRight.x) / 2,
					(clientTopLeft.y + clientBottomRight.y) / 2
				};
			}
		}

		RECT windowRect;
		if (GetWindowRect(hwnd, &windowRect) != FALSE)
		{
			return {
				(windowRect.left + windowRect.right) / 2,
				(windowRect.top + windowRect.bottom) / 2
			};
		}

		return { GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2 };
	}

	void SaveWindowPosition(HWND hwnd)
	{
		if (CurrentWindowedMode != WindowedMode::Framed || !HasWindowStatePath() || hwnd == nullptr || IsIconic(hwnd) != FALSE)
		{
			return;
		}

		RECT rect;
		if (GetWindowRect(hwnd, &rect) == FALSE || !IsWindowRectSane(rect))
		{
			return;
		}

		WriteProfileLong(WindowStateLeftKey, rect.left);
		WriteProfileLong(WindowStateTopKey, rect.top);
	}

	RECT WindowRectForClient(POINT clientSize, DWORD style, DWORD exStyle)
	{
		RECT rect = { 0, 0, clientSize.x, clientSize.y };
		AdjustWindowRectEx(&rect, style, FALSE, exStyle);
		return rect;
	}

	void ForceSystemCursorVisible()
	{
		int cursorShowCount = ShowCursor(TRUE);
		if (cursorShowCount > 0)
		{
			ShowCursor(FALSE);
		}
		else
		{
			++ForcedCursorShowCount;
			while (cursorShowCount < 0)
			{
				cursorShowCount = ShowCursor(TRUE);
				++ForcedCursorShowCount;
			}
		}
		SetCursor(LoadCursorW(nullptr, IDC_ARROW));
	}

	void RestoreSystemCursorVisibility()
	{
		while (ForcedCursorShowCount > 0)
		{
			ShowCursor(FALSE);
			--ForcedCursorShowCount;
		}
	}

	void ResizeWindowToClient(bool activateWindow = false, bool useSavedPosition = true, const POINT* windowPosition = nullptr);

	bool IsToggleBorderModeShortcut(UINT message, WPARAM wParam, LPARAM lParam)
	{
		return message == WM_SYSKEYDOWN && wParam == 'F' && (lParam & (1 << 29)) != 0;
	}

	bool IsToggleBorderModeSysChar(UINT message, WPARAM wParam, LPARAM lParam)
	{
		return message == WM_SYSCHAR && (wParam == 'f' || wParam == 'F') && (lParam & (1 << 29)) != 0;
	}

	bool IsAltKey(WPARAM wParam)
	{
		return wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU;
	}

	bool IsAltKeySystemMessage(UINT message, WPARAM wParam)
	{
		return (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) && IsAltKey(wParam);
	}

	bool IsSystemMenuCommand(UINT message, WPARAM wParam)
	{
		return message == WM_SYSCOMMAND && (wParam & 0xFFF0) == SC_KEYMENU;
	}

	LPARAM RegularKeyLParam(LPARAM lParam)
	{
		return lParam & ~(static_cast<LPARAM>(1) << 29);
	}

	LRESULT CallOriginalWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		return OriginalWndProc != nullptr
			? CallWindowProcA(OriginalWndProc, hwnd, message, wParam, lParam)
			: DefWindowProcA(hwnd, message, wParam, lParam);
	}

	void ToggleBorderMode(HWND hwnd)
	{
		if (CurrentWindowedMode == WindowedMode::Off)
		{
			return;
		}

		RECT currentRect;
		const POINT windowPosition = GetWindowRect(hwnd, &currentRect) != FALSE
			? POINT{ currentRect.left, currentRect.top }
			: GetWindowClientCenter(hwnd);
		const bool switchingToBorderless = CurrentWindowedMode == WindowedMode::Framed;
		if (switchingToBorderless)
		{
			SaveWindowPosition(hwnd);
			CurrentWindowedMode = WindowedMode::Borderless;
		}
		else
		{
			CurrentWindowedMode = WindowedMode::Framed;
		}

		ResizeWindowToClient(CurrentWindowedMode == WindowedMode::Borderless, false, &windowPosition);
	}

	LRESULT CALLBACK WindowedModeWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (IsToggleBorderModeShortcut(message, wParam, lParam))
		{
			ToggleBorderMode(hwnd);
			return 0;
		}
		else if (IsToggleBorderModeSysChar(message, wParam, lParam))
		{
			return 0;
		}
		else if (IsAltKeySystemMessage(message, wParam))
		{
			return CallOriginalWndProc(hwnd, message == WM_SYSKEYDOWN ? WM_KEYDOWN : WM_KEYUP, wParam, RegularKeyLParam(lParam));
		}
		else if (IsSystemMenuCommand(message, wParam))
		{
			return 0;
		}
		else if (message == WM_SETCURSOR)
		{
			const WORD hitTest = LOWORD(lParam);
			if (hitTest != HTCLIENT)
			{
				ForceSystemCursorVisible();
				return TRUE;
			}
			RestoreSystemCursorVisibility();
		}
		else if (message == WM_NCDESTROY)
		{
			SaveWindowPosition(hwnd);
			RestoreSystemCursorVisibility();
		}
		else if (message == WM_EXITSIZEMOVE)
		{
			SaveWindowPosition(hwnd);
		}

		return CallOriginalWndProc(hwnd, message, wParam, lParam);
	}

	void HookWindowProc()
	{
		if (Window == nullptr)
		{
			return;
		}

		WNDPROC currentWndProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(Window, GWLP_WNDPROC));
		if (currentWndProc == WindowedModeWndProc)
		{
			return;
		}

		OriginalWndProc = reinterpret_cast<WNDPROC>(
			SetWindowLongPtrW(Window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowedModeWndProc)));
	}

	bool BackupVideoModes(DisplayMode* videoModes, uint32_t modeCount)
	{
		if (videoModes == nullptr || modeCount == 0)
		{
			return false;
		}
		if (VideoModesBackup.empty())
		{
			VideoModesBackup.assign(videoModes, videoModes + modeCount);
		}
		return true;
	}

	bool IsUsableWindowedVideoMode(const DisplayMode& mode)
	{
		return mode.width != 0 && mode.height != 0;
	}

	bool SetClientSizeFromMode(const DisplayMode& mode)
	{
		if (!IsUsableWindowedVideoMode(mode))
		{
			return false;
		}
		ClientSize.x = static_cast<LONG>(mode.width);
		ClientSize.y = static_cast<LONG>(mode.height);
		ClientSize = ClampClientSize(ClientSize);
		return true;
	}

	void UpdateVideoMode()
	{
		DisplayMode* videoModes = *Ptr<DisplayMode*>(0xC97C48);
		if (videoModes == nullptr)
		{
			return;
		}

		auto numVideoModes = reinterpret_cast<uint32_t(__cdecl*)()>(0x7F2CC0);
		auto currentVideoMode = reinterpret_cast<uint32_t(__cdecl*)()>(0x7F2D20);
		const uint32_t modeCount = numVideoModes();
		const uint32_t modeIndex = currentVideoMode();
		if (modeCount == 0 || modeIndex >= modeCount)
		{
			return;
		}

		BackupVideoModes(videoModes, modeCount);
		if (PreviousVideoMode != UINT32_MAX && PreviousVideoMode != modeIndex && PreviousVideoMode < VideoModesBackup.size())
		{
			videoModes[PreviousVideoMode] = VideoModesBackup[PreviousVideoMode];
		}
		PreviousVideoMode = modeIndex;

		DisplayMode& mode = videoModes[modeIndex];
		mode.width = static_cast<uint32_t>(ClientSize.x);
		mode.height = static_cast<uint32_t>(ClientSize.y);
		mode.format = D3DFMT_A8R8G8B8;
		mode.refreshRate = 0;
		// The original windowed mode does it, but it causes the game to calculate the POV differently and drifts the crosshair
		//mode.flags &= ~1u;
	}

	void RestoreVideoMode(uint32_t modeIndex)
	{
		DisplayMode* videoModes = *Ptr<DisplayMode*>(0xC97C48);
		if (videoModes == nullptr || modeIndex >= VideoModesBackup.size())
		{
			return;
		}

		videoModes[modeIndex] = VideoModesBackup[modeIndex];
	}

	bool GetVideoMode(uint32_t modeIndex, const DisplayMode** mode)
	{
		DisplayMode* videoModes = *Ptr<DisplayMode*>(0xC97C48);
		if (videoModes == nullptr)
		{
			return false;
		}

		auto numVideoModes = reinterpret_cast<uint32_t(__cdecl*)()>(0x7F2CC0);
		const uint32_t modeCount = numVideoModes();
		if (modeCount == 0 || modeIndex >= modeCount)
		{
			return false;
		}

		BackupVideoModes(videoModes, modeCount);
		*mode = modeIndex < VideoModesBackup.size() ? &VideoModesBackup[modeIndex] : &videoModes[modeIndex];
		return true;
	}

	bool SetClientSizeFromVideoMode(uint32_t modeIndex)
	{
		const DisplayMode* mode = nullptr;
		return GetVideoMode(modeIndex, &mode) && SetClientSizeFromMode(*mode);
	}

	bool FindFallbackVideoMode(uint32_t* modeIndex)
	{
		DisplayMode* videoModes = *Ptr<DisplayMode*>(0xC97C48);
		if (videoModes == nullptr)
		{
			return false;
		}

		auto numVideoModes = reinterpret_cast<uint32_t(__cdecl*)()>(0x7F2CC0);
		const uint32_t modeCount = numVideoModes();
		if (!BackupVideoModes(videoModes, modeCount))
		{
			return false;
		}

		for (uint32_t i = 0; i < VideoModesBackup.size(); ++i)
		{
			const DisplayMode& mode = VideoModesBackup[i];
			if (mode.width == 800 && mode.height == 600 && IsUsableWindowedVideoMode(mode))
			{
				*modeIndex = i;
				return true;
			}
		}

		for (uint32_t i = 0; i < VideoModesBackup.size(); ++i)
		{
			if (IsUsableWindowedVideoMode(VideoModesBackup[i]))
			{
				*modeIndex = i;
				return true;
			}
		}
		return false;
	}

	bool ResolveRequestedVideoMode(int32_t requestedMode, uint32_t* resolvedMode)
	{
		if (requestedMode >= 0)
		{
			const DisplayMode* mode = nullptr;
			if (GetVideoMode(static_cast<uint32_t>(requestedMode), &mode) && IsUsableWindowedVideoMode(*mode))
			{
				*resolvedMode = static_cast<uint32_t>(requestedMode);
				return true;
			}
		}
		return FindFallbackVideoMode(resolvedMode);
	}

	void SetFrontendVideoMode(uint32_t modeIndex)
	{
		*Ptr<int32_t>(0xBA6748 + 0xD4) = static_cast<int32_t>(modeIndex);
		*Ptr<int32_t>(0xBA6748 + 0xD8) = static_cast<int32_t>(modeIndex);
	}

	bool SetClientSizeFromCurrentVideoMode()
	{
		auto currentVideoMode = reinterpret_cast<uint32_t(__cdecl*)()>(0x7F2D20);
		return SetClientSizeFromVideoMode(currentVideoMode());
	}

	bool SetClientSizeFromPrefsVideoMode()
	{
		const int32_t modeIndex = *Ptr<int32_t>(0xBA6748 + 0xD4);
		return modeIndex >= 0 && SetClientSizeFromVideoMode(static_cast<uint32_t>(modeIndex));
	}

	void ApplyWindowedState()
	{
		ClientSize = ClampClientSize(ClientSize);

		RsGlobalType* rsGlobal = Ptr<RsGlobalType>(0xC17040);
		if (rsGlobal->ps != nullptr)
		{
			rsGlobal->ps->fullScreen = FALSE;
			rsGlobal->ps->window = Window;
		}
		rsGlobal->MaximumWidth = ClientSize.x;
		rsGlobal->MaximumHeight = ClientSize.y;

		D3DPRESENT_PARAMETERS* params = Ptr<D3DPRESENT_PARAMETERS>(0xC9C040);
		params->Windowed = TRUE;
		params->hDeviceWindow = Window;
		params->BackBufferWidth = static_cast<UINT>(ClientSize.x);
		params->BackBufferHeight = static_cast<UINT>(ClientSize.y);
		params->BackBufferFormat = D3DFMT_A8R8G8B8;
		params->SwapEffect = D3DSWAPEFFECT_DISCARD;
		params->FullScreen_RefreshRateInHz = 0;
		params->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

		UpdateVideoMode();
	}

	void ApplyWindowedStateFromCurrentVideoMode()
	{
		if (!SetClientSizeFromPrefsVideoMode() && !SetClientSizeFromCurrentVideoMode())
		{
			uint32_t fallbackMode = 0;
			if (FindFallbackVideoMode(&fallbackMode))
			{
				SetClientSizeFromVideoMode(fallbackMode);
				SetFrontendVideoMode(fallbackMode);
			}
		}
		ApplyWindowedState();
	}

	void ApplyWindowHandleState()
	{
		RsGlobalType* rsGlobal = Ptr<RsGlobalType>(0xC17040);
		if (rsGlobal->ps != nullptr)
		{
			rsGlobal->ps->fullScreen = FALSE;
			rsGlobal->ps->window = Window;
		}

		D3DPRESENT_PARAMETERS* params = Ptr<D3DPRESENT_PARAMETERS>(0xC9C040);
		params->Windowed = TRUE;
		params->hDeviceWindow = Window;
		params->FullScreen_RefreshRateInHz = 0;
		params->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
	}

	void ResizeWindowToClient(bool activateWindow, bool useSavedPosition, const POINT* windowPosition)
	{
		if (Window == nullptr)
		{
			return;
		}

		const DWORD style = GetWindowedModeWindowStyle();
		const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(Window, GWL_EXSTYLE));
		RECT rect = WindowRectForClient(ClientSize, style, exStyle);
		const LONG windowWidth = rect.right - rect.left;
		const LONG windowHeight = rect.bottom - rect.top;
		const POINT position = windowPosition != nullptr
			? *windowPosition
			: GetDefaultWindowPosition(windowWidth, windowHeight, GetWindowClientCenter(Window), useSavedPosition);
		const bool isBorderless = CurrentWindowedMode == WindowedMode::Borderless;
		const bool shouldActivate = activateWindow && CurrentWindowedMode == WindowedMode::Borderless;

		SetWindowLongPtrW(Window, GWL_STYLE, style);
		SetWindowPos(Window, isBorderless ? HWND_TOPMOST : HWND_NOTOPMOST,
			position.x, position.y, windowWidth, windowHeight,
			SWP_NOOWNERZORDER | (shouldActivate ? 0 : SWP_NOACTIVATE) | SWP_FRAMECHANGED);
		if (shouldActivate)
		{
			SetForegroundWindow(Window);
			SetActiveWindow(Window);
			SetFocus(Window);
			SetWindowPos(Window, HWND_TOPMOST, position.x, position.y, windowWidth, windowHeight,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
		else if (!isBorderless)
		{
			SetWindowPos(Window, HWND_NOTOPMOST, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
		}
	}

	HRESULT __stdcall ResetHook(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* params)
	{
		UNREFERENCED_PARAMETER(params);
		ApplyWindowedState();
		ResizeWindowToClient();
		return ResetOriginal(self, Ptr<D3DPRESENT_PARAMETERS>(0xC9C040));
	}

	template<std::size_t Index>
	void SetCurrentVideoModeHook(int32_t modeIndex)
	{
		uint32_t resolvedMode = 0;
		const bool hasResolvedMode = ResolveRequestedVideoMode(modeIndex, &resolvedMode);
		const int32_t modeToApply = hasResolvedMode ? static_cast<int32_t>(resolvedMode) : modeIndex;

		if (hasResolvedMode)
		{
			if (PreviousVideoMode != UINT32_MAX)
			{
				RestoreVideoMode(PreviousVideoMode);
			}
			RestoreVideoMode(resolvedMode);
			SetClientSizeFromVideoMode(resolvedMode);
			SetFrontendVideoMode(resolvedMode);
			ApplyWindowedState();
			ResizeWindowToClient();
		}

		SetCurrentVideoModeOriginal<Index>(modeToApply);

		if (hasResolvedMode)
		{
			SetClientSizeFromVideoMode(resolvedMode);
			SetFrontendVideoMode(resolvedMode);
			ApplyWindowedState();
			ResizeWindowToClient();
		}
	}

	void HookSetCurrentVideoMode()
	{
		Memory::InterceptCall(0x574509, SetCurrentVideoModeOriginal<0>, SetCurrentVideoModeHook<0>);
		Memory::InterceptCall(0x57D096, SetCurrentVideoModeOriginal<1>, SetCurrentVideoModeHook<1>);
		Memory::InterceptCall(0x57D16E, SetCurrentVideoModeOriginal<2>, SetCurrentVideoModeHook<2>);
		Memory::InterceptCall(0x57D2B2, SetCurrentVideoModeOriginal<3>, SetCurrentVideoModeHook<3>);
	}

	void HookD3dDevice()
	{
		IDirect3DDevice9* device = *Ptr<IDirect3DDevice9*>(0xC97C28);
		if (device == nullptr)
		{
			return;
		}

		ApplyWindowedStateFromCurrentVideoMode();
		ResizeWindowToClient();

		uintptr_t* vtable = *reinterpret_cast<uintptr_t**>(device);
		if (vtable[16] == reinterpret_cast<uintptr_t>(&ResetHook))
		{
			return;
		}

		DWORD oldProtect;
		if (VirtualProtect(&vtable[16], sizeof(vtable[16]), PAGE_EXECUTE_READWRITE, &oldProtect) != FALSE)
		{
			ResetOriginal = reinterpret_cast<ResetFunc>(vtable[16]);
			vtable[16] = reinterpret_cast<uintptr_t>(&ResetHook);
			VirtualProtect(&vtable[16], sizeof(vtable[16]), oldProtect, &oldProtect);
		}
	}

	HWND __stdcall CreateWindowHook(DWORD, LPCSTR className, LPCSTR windowName, DWORD, int, int, int width, int height,
		HWND, HMENU, HINSTANCE instance, LPVOID param)
	{
		const POINT initialClientSize = ClampClientSize({ width, height });

		const DWORD style = GetWindowedModeWindowStyle();
		const DWORD exStyle = 0;
		RECT rect = WindowRectForClient(initialClientSize, style, exStyle);
		const LONG windowWidth = rect.right - rect.left;
		const LONG windowHeight = rect.bottom - rect.top;

		const POINT position = GetDefaultWindowPosition(windowWidth, windowHeight,
			{ GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2 });

		Window = CreateWindowExA(exStyle, className, windowName, style, position.x, position.y, windowWidth, windowHeight,
			nullptr, nullptr, instance, param);
		HookWindowProc();
		ApplyWindowHandleState();
		return Window;
	}

	void PatchJump(uintptr_t address, void* hook, size_t size)
	{
		Memory::InjectHook(address, hook, Memory::HookType::Jump);
		if (size > 5)
		{
			Memory::Nop(address + 5, size - 5);
		}
	}

	void PatchCall(uintptr_t address, void* hook, size_t size)
	{
		Memory::InjectHook(address, hook, Memory::HookType::Call);
		if (size > 5)
		{
			Memory::Nop(address + 5, size - 5);
		}
	}
}

extern "C" __declspec(naked) void WindowedModeSA_InitPresentationParams()
{
	__asm
	{
		pushfd
		pushad
		call ApplyWindowedStateFromCurrentVideoMode
		popad
		popfd
		mov ecx, dword ptr [0xC97C4C]
		jmp dword ptr [InitPresentationReturn]
	}
}

extern "C" __declspec(naked) void WindowedModeSA_InitD3dDevice()
{
	__asm
	{
		push eax
		mov eax, 0xC9808C
		mov [eax], ebp
		pop eax
		pushfd
		pushad
		call HookD3dDevice
		popad
		popfd
		jmp dword ptr [InitD3dDeviceReturn]
	}
}

namespace WindowedModeSA
{
	bool IsFramedMode()
	{
		return CurrentWindowedMode == WindowedMode::Framed;
	}

	bool Install(const wchar_t* iniPath)
	{
		CurrentWindowedMode = ReadWindowedModeOption(iniPath);
		if (CurrentWindowedMode == WindowedMode::Off)
		{
			return false;
		}

		InitPresentationReturn = 0x7F6710;
		InitD3dDeviceReturn = 0x7F6806;
		OriginalWndProc = nullptr;
		ForcedCursorShowCount = 0;
		InitWindowStatePath();

		Memory::Patch<uint8_t>(0x746225, 0xEB);
		PatchCall(0x7455D5, &CreateWindowHook, 6);
		PatchJump(0x7F670A, &WindowedModeSA_InitPresentationParams, 6);
		PatchJump(0x7F6800, &WindowedModeSA_InitD3dDevice, 6);
		HookSetCurrentVideoMode();
		return true;
	}
}
