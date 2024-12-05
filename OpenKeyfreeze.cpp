#define WIN32_LEAN_AND_MEAN
// Windows Header Files
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#define MAX_LOADSTRING 100
#define WM_TRAYICON (WM_USER + 1)

HINSTANCE g_instance;
WCHAR g_title[MAX_LOADSTRING];
WCHAR g_window_class[MAX_LOADSTRING];
NOTIFYICONDATA g_notify_icon_data;
HHOOK g_keyboard_hook = NULL;
bool g_is_keyboard_locked = false;
HICON g_icon_unlocked;
HICON g_icon_locked;
HANDLE g_mutex = NULL;

ATOM MyRegisterClass(HINSTANCE instance);
BOOL InitInstance(HINSTANCE instance, int cmd_show);
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param, LPARAM l_param);

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance, _In_ LPWSTR cmd_line, _In_ int cmd_show) {
	UNREFERENCED_PARAMETER(prev_instance);
	UNREFERENCED_PARAMETER(cmd_line);
	g_mutex = CreateMutex(NULL, FALSE, L"KeyfreezeMutexChamp");
	if (g_mutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		MessageBox(NULL, L"There is already an instance of OpenKeyfreeze running.", L"OpenKeyfreeze", MB_OK | MB_ICONEXCLAMATION);
		return 1;
	}

	LoadStringW(instance, IDS_APP_TITLE, g_title, MAX_LOADSTRING);
	LoadStringW(instance, IDC_OPENKEYFREEZE, g_window_class, MAX_LOADSTRING);
	MyRegisterClass(instance);

	if (!InitInstance(instance, cmd_show)) {
		return FALSE;
	}

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	if (g_mutex) {
		ReleaseMutex(g_mutex);
		CloseHandle(g_mutex);
	}

	return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE instance) {
	WNDCLASSEXW wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, instance, LoadIcon(instance, MAKEINTRESOURCE(IDI_OPENKEYFREEZE)), LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), MAKEINTRESOURCEW(IDC_OPENKEYFREEZE), g_window_class, LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL)) };
	return RegisterClassExW(&wcex);
}
	
BOOL InitInstance(HINSTANCE instance, int cmd_show) {
	g_instance = instance;
	HWND hwnd = CreateWindowW(g_window_class, g_title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, instance, nullptr);
	if (!hwnd) return FALSE; // hwnd is a fake (hidden) window

	/*** DEFINE THE SYSTEM TRAY ICONS. These are SHELL32's built in icons. :-) ***/
	g_icon_unlocked = LoadIcon(GetModuleHandle(L"SHELL32.dll"), MAKEINTRESOURCE(246)); // 246 = blue play triangle
	g_icon_locked = LoadIcon(GetModuleHandle(L"SHELL32.dll"), MAKEINTRESOURCE(200)); // 200 = red stop/"no" sign

	g_notify_icon_data = { sizeof(NOTIFYICONDATA), hwnd, IDI_OPENKEYFREEZE, NIF_ICON | NIF_MESSAGE | NIF_TIP, WM_TRAYICON, g_icon_unlocked };
	wcscpy_s(g_notify_icon_data.szTip, L"Keyboard Unlocked");
	Shell_NotifyIcon(NIM_ADD, &g_notify_icon_data);

	ShowWindow(hwnd, SW_HIDE);
	UpdateWindow(hwnd);
	return TRUE;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
	switch (message) {
	case WM_TRAYICON:
		if (l_param == WM_LBUTTONUP) {
			if (g_is_keyboard_locked) {
				UnhookWindowsHookEx(g_keyboard_hook);
				g_is_keyboard_locked = false;
				g_notify_icon_data.hIcon = g_icon_unlocked;
				wcscpy_s(g_notify_icon_data.szTip, L"Keyboard Unlocked");
			}
			else {
				g_keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, g_instance, 0);
				g_is_keyboard_locked = true;
				g_notify_icon_data.hIcon = g_icon_locked;
				wcscpy_s(g_notify_icon_data.szTip, L"Keyboard Locked");
			}
			Shell_NotifyIcon(NIM_MODIFY, &g_notify_icon_data);
		}
		break;
	case WM_COMMAND:
		if (LOWORD(w_param) == IDM_EXIT) {
			Shell_NotifyIcon(NIM_DELETE, &g_notify_icon_data);
			DestroyWindow(hwnd);
		}
		else {
			return DefWindowProc(hwnd, message, w_param, l_param);
		}
		break;
	case WM_DESTROY:
		if (g_is_keyboard_locked) { // attempt to free up keyboard
			UnhookWindowsHookEx(g_keyboard_hook);
			g_is_keyboard_locked = false;
		}
		Shell_NotifyIcon(NIM_DELETE, &g_notify_icon_data);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, message, w_param, l_param);
	}
	return 0;
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param, LPARAM l_param) {
	return (code == HC_ACTION) ? 1 : CallNextHookEx(g_keyboard_hook, code, w_param, l_param);
}
