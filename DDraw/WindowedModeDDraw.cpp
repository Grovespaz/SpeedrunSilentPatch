#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "WindowedModeDDraw.h"

#include <Shlwapi.h>
#include <cstdint>
#include <vector>

#include "Utils/MemoryMgr.h"

#pragma comment(lib, "shlwapi.lib")

namespace
{
	enum class Game
	{
		III,
		VC,
	};

	struct D3D8PresentParameters
	{
		uint32_t BackBufferWidth;
		uint32_t BackBufferHeight;
		uint32_t BackBufferFormat;
		uint32_t BackBufferCount;
		uint32_t MultiSampleType;
		uint32_t SwapEffect;
		HWND hDeviceWindow;
		BOOL Windowed;
		BOOL EnableAutoDepthStencil;
		uint32_t AutoDepthStencilFormat;
		uint32_t Flags;
		uint32_t FullScreen_RefreshRateInHz;
		uint32_t FullScreen_PresentationInterval;
	};

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
		int32_t screenWidth;
		int32_t screenHeight;
		uint32_t frameLimit;
		BOOL quit;
		PsGlobalType* ps;
	};

	struct DisplayMode
	{
		uint32_t width;
		uint32_t height;
		uint32_t refreshRate;
		uint32_t format;
		uint32_t flags;
	};

	struct Addresses
	{
		Game game;
		const wchar_t* iniName;
		uintptr_t rsGlobal;
		uintptr_t d3dDevice;
		uintptr_t d3dPresentParams;
		uintptr_t rwVideoModes;
		uintptr_t RwEngineGetNumVideoModes;
		uintptr_t RwEngineGetCurrentVideoMode;
		uintptr_t createWindow;
		size_t createWindowSize;
		uintptr_t initPresentationParams;
		uintptr_t initPresentationParamsStore;
		enum class PresentationStore { Ebp, Ebx } presentationStore;
		uintptr_t initD3dDevice;
		uintptr_t initD3dDeviceStore;
		enum class DeviceStore { Eax, Ebp } deviceStore;
		uintptr_t resolutionColoring;
		uintptr_t resolutionDisabling;
		size_t resolutionDisablingSize;
		uintptr_t resolutionHook;
	};

	constexpr uint32_t D3DFMT_X8R8G8B8 = 22;
	constexpr uint32_t D3DSWAPEFFECT_DISCARD = 1;
	constexpr POINT ResolutionMin = { 160, 112 };
	constexpr DWORD WindowedModeWindowStyle = (WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX)) | WS_VISIBLE | WS_CLIPSIBLINGS;

	const Addresses III_10 = {
		Game::III, L"SpeedrunSilentPatchIII.ini",
		0x8F4360, 0x662EF0, 0x943010, 0x662F18, 0x5A0ED0, 0x5A0F30,
		0x580F20, 6,
		0x5B7DA1, 0x943038, Addresses::PresentationStore::Ebp,
		0x5B76B8, 0x662F04, Addresses::DeviceStore::Eax,
		0x47C6B8, 0x4882CA, 6, 0x487842
	};

	const Addresses III_11 = {
		Game::III, L"SpeedrunSilentPatchIII.ini",
		0x8F4414, 0x662EF0, 0x9431C8, 0x662F18, 0x5A1190, 0x5A11F0,
		0x581270, 6,
		0x5B8061, 0x9431F0, Addresses::PresentationStore::Ebp,
		0x5B7978, 0x662F04, Addresses::DeviceStore::Eax,
		0x47C788, 0x4883CA, 6, 0x487942
	};

	const Addresses VC_10 = {
		Game::VC, L"SpeedrunSilentPatchVC.ini",
		0x9B48D8, 0x7897A8, 0xA0FD04, 0x7897D0, 0x642B40, 0x642BA0,
		0x5FFC75, 6,
		0x65C0B4, 0xA0FD24, Addresses::PresentationStore::Ebx,
		0x65C4E2, 0x789BF4, Addresses::DeviceStore::Ebp,
		0x49EDBC, 0x499F57, 2, 0x4999D0
	};

	const Addresses VC_11 = {
		Game::VC, L"SpeedrunSilentPatchVC.ini",
		0x9B48E0, 0x7897B0, 0xA0FD0C, 0x7897D8, 0x642B90, 0x642BF0,
		0x5FFC95, 6,
		0x65C104, 0xA0FD2C, Addresses::PresentationStore::Ebx,
		0x65C532, 0x789BFC, Addresses::DeviceStore::Ebp,
		0x49EDDD, 0x499F78, 2, 0x4999F1
	};

	const Addresses VC_JP = {
		Game::VC, L"SpeedrunSilentPatchVC.ini",
		0x9B18E8, 0x7867B0, 0xA0CD14, 0x7867D8, 0x641B70, 0x641BD0,
		0x5FF826, 5,
		0x65B0E4, 0xA0CD34, Addresses::PresentationStore::Ebx,
		0x65B512, 0x786BFC, Addresses::DeviceStore::Ebp,
		0x49EC68, 0x499EF0, 2, 0x499969
	};

	extern "C" void InitPresentationParams_StoreEbp();
	extern "C" void InitPresentationParams_StoreEbx();
	extern "C" void InitD3dDevice_StoreEax();
	extern "C" void InitD3dDevice_StoreEbp();

	const Addresses* Current = nullptr;
	HWND Window = nullptr;
	WNDPROC OriginalWndProc = nullptr;
	POINT ClientSize = { 640, 480 };
	int ForcedCursorShowCount = 0;
	uintptr_t InitPresentationReturn = 0;
	uintptr_t InitPresentationStore = 0;
	uintptr_t InitD3dDeviceReturn = 0;
	uintptr_t InitD3dDeviceStore = 0;
	std::vector<DisplayMode> VideoModesBackup;
	uint32_t PreviousVideoMode = UINT32_MAX;

	using ResetFunc = HRESULT(__stdcall*)(void*, D3D8PresentParameters*);
	ResetFunc ResetOriginal = nullptr;
	using ChangeVideoModeFunc = void(__cdecl*)(uint32_t);
	ChangeVideoModeFunc ChangeVideoModeOriginal = nullptr;

	template<typename T>
	T* Dyn(uintptr_t address)
	{
		return reinterpret_cast<T*>(Memory::DynBaseAddress(address));
	}

	bool ReadWindowedModeOption(HINSTANCE module, const wchar_t* iniName)
	{
		wchar_t path[MAX_PATH];
		if (GetModuleFileNameW(module, path, _countof(path)) == 0)
		{
			return false;
		}

		PathRemoveFileSpecW(path);
		PathAppendW(path, iniName);
		return GetPrivateProfileIntW(L"SilentPatch", L"WindowedMode", 0, path) != 0;
	}

	RECT GetNearestMonitorRect(POINT point)
	{
		HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info = { sizeof(info) };
		if (GetMonitorInfoW(monitor, &info) == FALSE)
		{
			info.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
		}
		return info.rcWork;
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

	LRESULT CALLBACK WindowedModeWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		if (message == WM_SETCURSOR)
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
			RestoreSystemCursorVisibility();
		}

		return OriginalWndProc != nullptr
			? CallWindowProcA(OriginalWndProc, hwnd, message, wParam, lParam)
			: DefWindowProcA(hwnd, message, wParam, lParam);
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

	struct MonitorResolution
	{
		LONG width;
		LONG height;
		LONG area;
	};

	BOOL CALLBACK FindLargestMonitorResolution(HMONITOR monitor, HDC, LPRECT, LPARAM data)
	{
		MONITORINFO info = { sizeof(info) };
		if (GetMonitorInfoW(monitor, &info) != FALSE)
		{
			const LONG width = info.rcMonitor.right - info.rcMonitor.left;
			const LONG height = info.rcMonitor.bottom - info.rcMonitor.top;
			const LONG area = width * height;
			MonitorResolution* largest = reinterpret_cast<MonitorResolution*>(data);
			if (area > largest->area)
			{
				largest->width = width;
				largest->height = height;
				largest->area = area;
			}
		}
		return TRUE;
	}

	POINT GetLargestMonitorResolution()
	{
		static const POINT largest = []() {
			MonitorResolution result = {
				GetSystemMetrics(SM_CXSCREEN),
				GetSystemMetrics(SM_CYSCREEN),
				GetSystemMetrics(SM_CXSCREEN) * GetSystemMetrics(SM_CYSCREEN)
			};
			EnumDisplayMonitors(nullptr, nullptr, FindLargestMonitorResolution, reinterpret_cast<LPARAM>(&result));
			return POINT{ result.width, result.height };
		}();
		return largest;
	}

	bool IsLargestMonitorResolution(uint32_t width, uint32_t height)
	{
		const POINT largest = GetLargestMonitorResolution();
		return width == static_cast<uint32_t>(largest.x) && height == static_cast<uint32_t>(largest.y);
	}

	bool IsUsableWindowedVideoMode(const DisplayMode& mode)
	{
		return mode.width != 0 && mode.height != 0 && !IsLargestMonitorResolution(mode.width, mode.height);
	}

	bool GetVideoModes(DisplayMode*& videoModes, uint32_t& modeCount)
	{
		if (Current == nullptr)
		{
			return false;
		}

		videoModes = *Dyn<DisplayMode*>(Current->rwVideoModes);
		if (videoModes == nullptr)
		{
			return false;
		}

		auto numVideoModes = reinterpret_cast<uint32_t(__cdecl*)()>(Memory::DynBaseAddress(Current->RwEngineGetNumVideoModes));
		modeCount = numVideoModes();
		return modeCount != 0;
	}

	void BackupVideoModes(DisplayMode* videoModes, uint32_t modeCount)
	{
		if (VideoModesBackup.empty())
		{
			VideoModesBackup.assign(videoModes, videoModes + modeCount);
		}
	}

	void RestoreVideoMode(uint32_t modeIndex)
	{
		DisplayMode* videoModes = nullptr;
		uint32_t modeCount = 0;
		if (!GetVideoModes(videoModes, modeCount) || modeIndex >= modeCount || modeIndex >= VideoModesBackup.size())
		{
			return;
		}

		videoModes[modeIndex] = VideoModesBackup[modeIndex];
	}

	bool SetClientSizeFromVideoMode(uint32_t modeIndex)
	{
		DisplayMode* videoModes = nullptr;
		uint32_t modeCount = 0;
		if (!GetVideoModes(videoModes, modeCount) || modeIndex >= modeCount)
		{
			return false;
		}

		BackupVideoModes(videoModes, modeCount);
		const DisplayMode& mode = modeIndex < VideoModesBackup.size() ? VideoModesBackup[modeIndex] : videoModes[modeIndex];
		if (!IsUsableWindowedVideoMode(mode))
		{
			return false;
		}

		ClientSize.x = static_cast<LONG>(mode.width);
		ClientSize.y = static_cast<LONG>(mode.height);
		ClientSize = ClampClientSize(ClientSize);
		return true;
	}

	bool SetClientSizeFromFallbackVideoMode()
	{
		DisplayMode* videoModes = nullptr;
		uint32_t modeCount = 0;
		if (!GetVideoModes(videoModes, modeCount))
		{
			return false;
		}

		BackupVideoModes(videoModes, modeCount);
		for (const DisplayMode& mode : VideoModesBackup)
		{
			if (mode.width == 800 && mode.height == 600 && IsUsableWindowedVideoMode(mode))
			{
				ClientSize = { static_cast<LONG>(mode.width), static_cast<LONG>(mode.height) };
				return true;
			}
		}
		for (const DisplayMode& mode : VideoModesBackup)
		{
			if (IsUsableWindowedVideoMode(mode))
			{
				ClientSize = { static_cast<LONG>(mode.width), static_cast<LONG>(mode.height) };
				return true;
			}
		}
		return false;
	}

	bool SetClientSizeFromCurrentVideoMode()
	{
		if (Current == nullptr)
		{
			return false;
		}

		auto currentVideoMode = reinterpret_cast<uint32_t(__cdecl*)()>(Memory::DynBaseAddress(Current->RwEngineGetCurrentVideoMode));
		return SetClientSizeFromVideoMode(currentVideoMode());
	}

	bool SetClientSizeFromPresentationParams()
	{
		if (Current == nullptr)
		{
			return false;
		}

		const D3D8PresentParameters* params = Dyn<D3D8PresentParameters>(Current->d3dPresentParams);
		if (params->BackBufferWidth < static_cast<uint32_t>(ResolutionMin.x) ||
			params->BackBufferHeight < static_cast<uint32_t>(ResolutionMin.y) ||
			IsLargestMonitorResolution(params->BackBufferWidth, params->BackBufferHeight))
		{
			return false;
		}

		ClientSize.x = static_cast<LONG>(params->BackBufferWidth);
		ClientSize.y = static_cast<LONG>(params->BackBufferHeight);
		ClientSize = ClampClientSize(ClientSize);
		return true;
	}

	void UpdateVideoMode()
	{
		if (Current == nullptr)
		{
			return;
		}

		DisplayMode* videoModes = nullptr;
		uint32_t modeCount = 0;
		if (!GetVideoModes(videoModes, modeCount))
		{
			return;
		}

		BackupVideoModes(videoModes, modeCount);

		auto currentVideoMode = reinterpret_cast<uint32_t(__cdecl*)()>(Memory::DynBaseAddress(Current->RwEngineGetCurrentVideoMode));
		const uint32_t modeIndex = currentVideoMode();
		if (modeIndex >= modeCount)
		{
			return;
		}
		if (PreviousVideoMode != UINT32_MAX && PreviousVideoMode != modeIndex)
		{
			RestoreVideoMode(PreviousVideoMode);
		}
		PreviousVideoMode = modeIndex;

		DisplayMode& mode = videoModes[modeIndex];
		mode.width = static_cast<uint32_t>(ClientSize.x);
		mode.height = static_cast<uint32_t>(ClientSize.y);
		mode.format = D3DFMT_X8R8G8B8;
		mode.refreshRate = 0;
		mode.flags &= ~1u;
	}

	void ApplyWindowedState()
	{
		if (Current == nullptr)
		{
			return;
		}

		ClientSize = ClampClientSize(ClientSize);

		RsGlobalType* rsGlobal = Dyn<RsGlobalType>(Current->rsGlobal);
		if (rsGlobal->ps != nullptr)
		{
			rsGlobal->ps->fullScreen = FALSE;
			rsGlobal->ps->window = Window;
		}
		rsGlobal->screenWidth = ClientSize.x;
		rsGlobal->screenHeight = ClientSize.y;
		rsGlobal->MaximumWidth = ClientSize.x;
		rsGlobal->MaximumHeight = ClientSize.y;

		D3D8PresentParameters* params = Dyn<D3D8PresentParameters>(Current->d3dPresentParams);
		params->Windowed = TRUE;
		params->hDeviceWindow = Window;
		params->BackBufferWidth = static_cast<uint32_t>(ClientSize.x);
		params->BackBufferHeight = static_cast<uint32_t>(ClientSize.y);
		params->BackBufferFormat = D3DFMT_X8R8G8B8;
		params->SwapEffect = D3DSWAPEFFECT_DISCARD;
		params->FullScreen_RefreshRateInHz = 0;
		params->FullScreen_PresentationInterval = 0;

		UpdateVideoMode();
	}

	void ApplyWindowedStateFromPresentationParams()
	{
		if (!SetClientSizeFromPresentationParams() && !SetClientSizeFromCurrentVideoMode())
		{
			SetClientSizeFromFallbackVideoMode();
		}
		ApplyWindowedState();
	}

	void ApplyWindowHandleState()
	{
		if (Current == nullptr)
		{
			return;
		}

		RsGlobalType* rsGlobal = Dyn<RsGlobalType>(Current->rsGlobal);
		if (rsGlobal->ps != nullptr)
		{
			rsGlobal->ps->fullScreen = FALSE;
			rsGlobal->ps->window = Window;
		}

		D3D8PresentParameters* params = Dyn<D3D8PresentParameters>(Current->d3dPresentParams);
		params->Windowed = TRUE;
		params->hDeviceWindow = Window;
		params->FullScreen_RefreshRateInHz = 0;
		params->FullScreen_PresentationInterval = 0;
	}

	void ResizeWindowToClient()
	{
		if (Window == nullptr)
		{
			return;
		}

		DWORD style = WindowedModeWindowStyle;
		DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(Window, GWL_EXSTYLE));
		RECT rect = WindowRectForClient(ClientSize, style, exStyle);

		RECT currentRect;
		GetWindowRect(Window, &currentRect);
		const LONG windowWidth = rect.right - rect.left;
		const LONG windowHeight = rect.bottom - rect.top;
		const POINT windowCenter = {
			(currentRect.left + currentRect.right) / 2,
			(currentRect.top + currentRect.bottom) / 2
		};
		const RECT monitorRect = GetNearestMonitorRect(windowCenter);
		const int x = monitorRect.left + ((monitorRect.right - monitorRect.left) - windowWidth) / 2;
		const int y = monitorRect.top + ((monitorRect.bottom - monitorRect.top) - windowHeight) / 2;

		SetWindowLongPtrW(Window, GWL_STYLE, style);
		SetWindowPos(Window, nullptr, x, y, windowWidth, windowHeight,
			SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
	}

	HRESULT __stdcall ResetHook(void* self, D3D8PresentParameters* params)
	{
		if (params != nullptr && params->BackBufferWidth != 0 && params->BackBufferHeight != 0)
		{
			ClientSize.x = static_cast<LONG>(params->BackBufferWidth);
			ClientSize.y = static_cast<LONG>(params->BackBufferHeight);
		}

		ApplyWindowedState();
		ResizeWindowToClient();
		return ResetOriginal(self, Dyn<D3D8PresentParameters>(Current->d3dPresentParams));
	}

	void HookD3dDevice()
	{
		if (Current == nullptr)
		{
			return;
		}

		void* device = *Dyn<void*>(Current->d3dDevice);
		if (device == nullptr)
		{
			return;
		}

		ApplyWindowedStateFromPresentationParams();
		ResizeWindowToClient();

		uintptr_t* vtable = *static_cast<uintptr_t**>(device);
		if (vtable[14] == reinterpret_cast<uintptr_t>(&ResetHook))
		{
			return;
		}

		DWORD oldProtect;
		if (VirtualProtect(&vtable[14], sizeof(vtable[14]), PAGE_EXECUTE_READWRITE, &oldProtect) != FALSE)
		{
			ResetOriginal = reinterpret_cast<ResetFunc>(vtable[14]);
			vtable[14] = reinterpret_cast<uintptr_t>(&ResetHook);
			VirtualProtect(&vtable[14], sizeof(vtable[14]), oldProtect, &oldProtect);
		}
	}

	HWND __stdcall CreateWindowHook(DWORD, LPCSTR className, LPCSTR windowName, DWORD, int, int, int width, int height,
		HWND, HMENU, HINSTANCE instance, LPVOID param)
	{
		const POINT initialClientSize = ClampClientSize({ width, height });

		const DWORD style = WindowedModeWindowStyle;
		const DWORD exStyle = 0;
		RECT rect = WindowRectForClient(initialClientSize, style, exStyle);
		const LONG windowWidth = rect.right - rect.left;
		const LONG windowHeight = rect.bottom - rect.top;

		RECT monitorRect = GetNearestMonitorRect({ GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2 });
		const int x = monitorRect.left + ((monitorRect.right - monitorRect.left) - windowWidth) / 2;
		const int y = monitorRect.top + ((monitorRect.bottom - monitorRect.top) - windowHeight) / 2;

		Window = CreateWindowExA(exStyle, className, windowName, style, x, y, windowWidth, windowHeight,
			nullptr, nullptr, instance, param);
		HookWindowProc();
		ApplyWindowHandleState();
		return Window;
	}

	void __cdecl ChangeVideoModeHook(uint32_t modeIndex)
	{
		RestoreVideoMode(modeIndex);
		if (ChangeVideoModeOriginal != nullptr)
		{
			ChangeVideoModeOriginal(modeIndex);
		}
		SetClientSizeFromVideoMode(modeIndex);
		ApplyWindowedState();
		ResizeWindowToClient();
	}

	void PatchJump(uintptr_t address, void* hook, size_t size)
	{
		using namespace Memory::DynBase;
		InjectHook(address, hook, HookType::Jump);
		if (size > 5)
		{
			Nop(address + 5, size - 5);
		}
	}

	void PatchCall(uintptr_t address, void* hook, size_t size)
	{
		using namespace Memory::DynBase;
		InjectHook(address, hook, HookType::Call);
		if (size > 5)
		{
			Nop(address + 5, size - 5);
		}
	}

	size_t InitD3dDevicePatchSize(const Addresses& addresses)
	{
		return addresses.deviceStore == Addresses::DeviceStore::Eax ? 5 : 6;
	}

	bool Install(const Addresses& addresses, HINSTANCE module)
	{
		if (!ReadWindowedModeOption(module, addresses.iniName))
		{
			return false;
		}

		Current = &addresses;
		OriginalWndProc = nullptr;
		ForcedCursorShowCount = 0;
		VideoModesBackup.clear();
		PreviousVideoMode = UINT32_MAX;
		ChangeVideoModeOriginal = nullptr;
		InitPresentationReturn = Memory::DynBaseAddress(addresses.initPresentationParams + 6);
		InitPresentationStore = Memory::DynBaseAddress(addresses.initPresentationParamsStore);
		InitD3dDeviceReturn = Memory::DynBaseAddress(addresses.initD3dDevice + InitD3dDevicePatchSize(addresses));
		InitD3dDeviceStore = Memory::DynBaseAddress(addresses.initD3dDeviceStore);

		PatchCall(addresses.createWindow, &CreateWindowHook, addresses.createWindowSize);

		if (addresses.presentationStore == Addresses::PresentationStore::Ebp)
		{
			PatchJump(addresses.initPresentationParams, &InitPresentationParams_StoreEbp, 6);
		}
		else
		{
			PatchJump(addresses.initPresentationParams, &InitPresentationParams_StoreEbx, 6);
		}

		if (addresses.deviceStore == Addresses::DeviceStore::Eax)
		{
			PatchJump(addresses.initD3dDevice, &InitD3dDevice_StoreEax, InitD3dDevicePatchSize(addresses));
		}
		else
		{
			PatchJump(addresses.initD3dDevice, &InitD3dDevice_StoreEbp, InitD3dDevicePatchSize(addresses));
		}

		if (addresses.game == Game::III)
		{
			Memory::DynBase::Patch<uint8_t>(addresses.resolutionColoring, 0xEB);
		}
		else
		{
			Memory::DynBase::Patch<uint16_t>(addresses.resolutionColoring, 0xE990);
		}
		Memory::DynBase::Nop(addresses.resolutionDisabling, addresses.resolutionDisablingSize);
		Memory::DynBase::InterceptCall(addresses.resolutionHook, ChangeVideoModeOriginal, ChangeVideoModeHook);

		return true;
	}
}

extern "C" __declspec(naked) void InitPresentationParams_StoreEbp()
{
	__asm
	{
		push eax
		mov eax, InitPresentationStore
		mov [eax], ebp
		pop eax
		pushfd
		pushad
		call ApplyWindowedStateFromPresentationParams
		popad
		popfd
		jmp dword ptr [InitPresentationReturn]
	}
}

extern "C" __declspec(naked) void InitPresentationParams_StoreEbx()
{
	__asm
	{
		push eax
		mov eax, InitPresentationStore
		mov [eax], ebx
		pop eax
		pushfd
		pushad
		call ApplyWindowedStateFromPresentationParams
		popad
		popfd
		jmp dword ptr [InitPresentationReturn]
	}
}

extern "C" __declspec(naked) void InitD3dDevice_StoreEax()
{
	__asm
	{
		push edx
		mov edx, InitD3dDeviceStore
		mov [edx], eax
		pop edx
		pushfd
		test eax, eax
		jz nohook
		pushad
		call HookD3dDevice
		popad
	nohook:
		popfd
		jmp dword ptr [InitD3dDeviceReturn]
	}
}

extern "C" __declspec(naked) void InitD3dDevice_StoreEbp()
{
	__asm
	{
		push eax
		mov eax, InitD3dDeviceStore
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

namespace WindowedModeDDraw
{
	bool InstallIII10(HINSTANCE module)
	{
		return Install(III_10, module);
	}

	bool InstallIII11(HINSTANCE module)
	{
		return Install(III_11, module);
	}

	bool InstallVC10(HINSTANCE module)
	{
		return Install(VC_10, module);
	}

	bool InstallVC11(HINSTANCE module)
	{
		return Install(VC_11, module);
	}

	bool InstallVCJP(HINSTANCE module)
	{
		return Install(VC_JP, module);
	}
}
