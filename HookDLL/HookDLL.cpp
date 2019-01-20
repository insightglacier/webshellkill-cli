// HookDLL.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "global.h"
#include "config.h"
#include <string>
#include <iostream>
#include <shlobj.h>
#include <Windows.h>
#include <tlhelp32.h>
#include "detours.h"
#include <shellapi.h>
#include "CLI11.hpp"
#pragma comment (lib, "detours.lib")

static auto oldSHBrowseForFolderA = SHBrowseForFolderA;
static auto oldRegQueryValueExA = RegQueryValueExA;
static auto oldCreateWindowExA = CreateWindowExA;
static auto oldShowWindow = ShowWindow;
static auto oldGetCommandLineA = GetCommandLineA;
bool isWindowInitialized = false;

LPITEMIDLIST parsePIDLFromPath(LPCSTR path) {
	OLECHAR szOleChar[MAX_PATH];
	LPSHELLFOLDER lpsfDeskTop;
	LPITEMIDLIST lpifq;
	ULONG ulEaten, ulAttribs;
	HRESULT hres;
	SHGetDesktopFolder(&lpsfDeskTop);
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path, -1, szOleChar, sizeof(szOleChar));
	hres = lpsfDeskTop->ParseDisplayName(NULL, NULL, szOleChar, &ulEaten, &lpifq, &ulAttribs);
	hres = lpsfDeskTop->Release();
	if (FAILED(hres)) {
		return NULL;
	}	
	return lpifq;
}

LPITEMIDLIST WINAPI hookSHBrowseForFolderA(LPBROWSEINFOA lpbi)
{
	return parsePIDLFromPath(WebShellKillHook::Global::currentIterator->c_str());
}

LSTATUS WINAPI hookRegQueryValueExA(HKEY hKey, LPSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE  lpData, LPDWORD lpcbData) {
	printf("%s\n", lpValueName);
	// As symbol of window initialized
	if (strcmp(lpValueName, "ALL_Check_dir") == 0) {
		isWindowInitialized = true;
	}

	return oldRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

HWND WINAPI hookCreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
	HWND ret = oldCreateWindowExA(hWndParent == 0 ? WS_EX_TOOLWINDOW : dwExStyle, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	if (hWndParent == 0) {
		SetWindowLong(ret, GWL_STYLE, 0);
	}
	if (lpWindowName != nullptr) {
		if (strcmp(lpWindowName, "自定义扫描") == 0) {
			WebShellKillHook::Global::event.emit<HWND>(WebShellKillHook::Global::EVENT_GET_BUTTON_HWND, ret);
		}
	}
	return ret;
}

BOOL WINAPI hookShowWindow(HWND hWnd, int nCmdShow) {
	if (isWindowInitialized) {
		WebShellKillHook::Global::event.emit(WebShellKillHook::Global::EVENT_READY);
		isWindowInitialized = false; // do not initialize twice
	}
	return oldShowWindow(hWnd, 0);
}

// v2.0.9, 001329FB. Don't let it get argv
LPTSTR WINAPI hookGetCommandLineA()
{
	LPTSTR ret = new char[MAX_PATH];
	LPWSTR *szArglist;
	int nArgs;
	szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
	sprintf_s(ret, lstrlenW(szArglist[0]) + 1, "%ls", szArglist[0]);
	return ret;
}

void Hijack() {
	if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
		AllocConsole();
	}
	freopen("CONOUT$", "w", stdout);
	DetourRestoreAfterWith();
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach((void **)&oldSHBrowseForFolderA, hookSHBrowseForFolderA);
	DetourAttach((void **)&oldRegQueryValueExA, hookRegQueryValueExA);
	DetourAttach((void **)&oldGetCommandLineA, hookGetCommandLineA);
	DetourAttach((void **)&oldCreateWindowExA, hookCreateWindowExA);
	DetourAttach((void **)&oldShowWindow, hookShowWindow);
	DetourTransactionCommit();

	Config::initialize();
	WebShellKillHook::Global::initialize();

}
