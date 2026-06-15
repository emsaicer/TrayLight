#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

#define ID_TRAY_ICON 1
#define WM_TRAY_MESSAGE (WM_USER + 1)

#define ID_MENU_APPS_ONLY 2
#define ID_MENU_AUTOSTART 3
#define ID_MENU_EXIT 4

NOTIFYICONDATA notify_icon_data;
BOOL is_apps_only = TRUE;

void get_ini_path(char *out_path, DWORD size) {
	char exe_path[MAX_PATH];
	GetModuleFileNameA(NULL, exe_path, MAX_PATH);
	char *last_slash = strrchr(exe_path, '\\');
	if (last_slash != NULL) {
		*(last_slash + 1) = '\0';
	}
	snprintf(out_path, size, "%sconfig.ini", exe_path);
}

void save_settings_to_ini() {
	char ini_path[MAX_PATH];
	get_ini_path(ini_path, MAX_PATH);
	WritePrivateProfileStringA("Settings", "AppsOnly", is_apps_only ? "1" : "0", ini_path);
}

void load_settings_from_ini() {
	char ini_path[MAX_PATH];
	get_ini_path(ini_path, MAX_PATH);
	is_apps_only = GetPrivateProfileIntA("Settings", "AppsOnly", 1, ini_path);
}

BOOL get_startup_folder_path(char *out_path) {
	return SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_STARTUP, NULL, 0, out_path));
}

char *get_startup_shortcut_path(char *out_path, DWORD size) {
	char startup_path[MAX_PATH];

	if (!get_startup_folder_path(startup_path)) return NULL;
	snprintf(out_path, size, "%s\\TrayLight.lnk", startup_path);

	return out_path;
}

BOOL is_autostart_enabled() {
	char buf[MAX_PATH];
	DWORD attributes = GetFileAttributesA(get_startup_shortcut_path(buf, sizeof(buf)));

	return (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY));
}

void enable_autostart() {
	HRESULT h_result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(h_result) && h_result != RPC_E_CHANGED_MODE) return;
	wchar_t exe_path[MAX_PATH];
	wchar_t startup_dir[MAX_PATH];
	wchar_t shortcut_path[MAX_PATH];

	GetModuleFileNameW(NULL, exe_path, MAX_PATH);
	SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startup_dir);
	swprintf_s(shortcut_path, MAX_PATH, L"%s\\TrayLight.lnk", startup_dir);

	IShellLinkW *p_shell_link = NULL;
	h_result = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (LPVOID *)&p_shell_link);

	if (SUCCEEDED(h_result)) {
		IPersistFile *p_persist_file = NULL;

		p_shell_link->lpVtbl->SetPath(p_shell_link, exe_path);
		h_result = p_shell_link->lpVtbl->QueryInterface(p_shell_link, &IID_IPersistFile, (LPVOID *)&p_persist_file);
		if (SUCCEEDED(h_result)) {
			p_persist_file->lpVtbl->Save(p_persist_file, shortcut_path, TRUE);
			p_persist_file->lpVtbl->Release(p_persist_file);
		}
		p_shell_link->lpVtbl->Release(p_shell_link);
	}
	CoUninitialize();
}

void disable_autostart() {
	char buf[MAX_PATH];
	DeleteFileA(get_startup_shortcut_path(buf, sizeof(buf)));
}

void toggle_system_theme() {
	HKEY h_key;
	const char *sub_key = "Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
	if (RegOpenKeyExA(HKEY_CURRENT_USER, sub_key, 0, KEY_QUERY_VALUE | KEY_SET_VALUE, &h_key) != ERROR_SUCCESS) return;

	DWORD current_value = 1;
	DWORD buffer_size = sizeof(current_value);
	RegQueryValueExA(h_key, "AppsUseLightTheme", NULL, NULL, (LPBYTE)&current_value, &buffer_size);

	DWORD new_value = current_value ^ 1;
	RegSetValueExA(h_key, "AppsUseLightTheme", 0, REG_DWORD, (const BYTE *)&new_value, sizeof(new_value));

	if (!is_apps_only) {
		RegSetValueExA(h_key, "SystemUsesLightTheme", 0, REG_DWORD, (const BYTE *)&new_value, sizeof(new_value));
	}
	RegCloseKey(h_key);

	SendNotifyMessageA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM) "ImmersiveColorSet");
}

void show_tray_menu(HWND hwnd) {
	HMENU h_menu = CreatePopupMenu();

	AppendMenuA(h_menu, MF_STRING | is_apps_only ? MF_CHECKED : MF_UNCHECKED, ID_MENU_APPS_ONLY, "For Apps Only");
	AppendMenuA(h_menu, MF_STRING | is_autostart_enabled() ? MF_CHECKED : MF_UNCHECKED, ID_MENU_AUTOSTART, "Run at Windows Startup");
	AppendMenuA(h_menu, MF_SEPARATOR, 0, NULL);
	AppendMenuA(h_menu, MF_STRING, ID_MENU_EXIT, "Exit");

	POINT point;

	GetCursorPos(&point);
	SetForegroundWindow(hwnd);
	TrackPopupMenu(h_menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, point.x, point.y, 0, hwnd, NULL);
	DestroyMenu(h_menu);
}

void handle_tray_message(HWND hwnd, LPARAM lParam) {
	switch (lParam) {

	case WM_RBUTTONUP:
		show_tray_menu(hwnd);
		break;

	case WM_LBUTTONUP:
		toggle_system_theme();
		break;
	}
}

void handle_menu_command(HWND hwnd, WORD menu_id) {
	switch (menu_id) {

	case ID_MENU_APPS_ONLY:
		is_apps_only = !is_apps_only;
		save_settings_to_ini();
		break;

	case ID_MENU_AUTOSTART:
		is_autostart_enabled() ? disable_autostart() : enable_autostart();
		break;

	case ID_MENU_EXIT:
		DestroyWindow(hwnd);
		break;
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {

	case WM_TRAY_MESSAGE:
		handle_tray_message(hwnd, lParam);
		break;

	case WM_COMMAND:
		handle_menu_command(hwnd, LOWORD(wParam));
		break;

	case WM_DESTROY:
		Shell_NotifyIcon(NIM_DELETE, &notify_icon_data);
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	HMODULE h_user_32 = GetModuleHandle("user32.dll");
	if (h_user_32) {
		typedef BOOL(WINAPI * SetProcessDpiAwarenessContextProc)(HANDLE);
		SetProcessDpiAwarenessContextProc set_dpi_context = (SetProcessDpiAwarenessContextProc)GetProcAddress(h_user_32, "SetProcessDpiAwarenessContext");

		if (set_dpi_context) {
			set_dpi_context(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		}
	}

	HANDLE mutex_handle = CreateMutexA(NULL, TRUE, "Local\\TrayLightUniqueMutexName");

	if (mutex_handle == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		if (mutex_handle) CloseHandle(mutex_handle);
		return 0;
	}

	load_settings_from_ini();

	if (is_autostart_enabled()) {
		enable_autostart();
	}

	const char CLASS_NAME[] = "TrayLightWindowClass";
	WNDCLASS window_class = {0};

	window_class.lpfnWndProc = WindowProc;
	window_class.hInstance = hInstance;
	window_class.lpszClassName = CLASS_NAME;
	RegisterClass(&window_class);

	HWND hwnd = CreateWindowEx(0, CLASS_NAME, "TrayLight", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
	if (hwnd == NULL) return 0;

	notify_icon_data.cbSize = sizeof(NOTIFYICONDATA);
	notify_icon_data.hWnd = hwnd;
	notify_icon_data.uID = ID_TRAY_ICON;
	notify_icon_data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	notify_icon_data.uCallbackMessage = WM_TRAY_MESSAGE;
	notify_icon_data.hIcon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
	if (notify_icon_data.hIcon == NULL) {
		notify_icon_data.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	}
	lstrcpyA(notify_icon_data.szTip, "TrayLight");

	Shell_NotifyIcon(NIM_ADD, &notify_icon_data);

	MSG message;
	while (GetMessage(&message, NULL, 0, 0)) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	CloseHandle(mutex_handle);

	return 0;
}