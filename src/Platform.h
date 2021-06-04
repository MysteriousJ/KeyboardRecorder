#include <Windows.h>
#include <gl/GL.h>
#include <wingdi.h>
#include <stdint.h>
#include <string>
#include "DynamicArray.h"

typedef int32_t int32;
typedef int64_t int64;
typedef unsigned int uint;
typedef uint32_t uint32;
typedef uint64_t uint64;

struct Window
{
	HWND hwnd;
	uint width;
	uint height;
};

struct KeyInput
{
	enum Type { press, release };

	unsigned short scancode;
	unsigned int extended;
	Type type;
};

struct WindowInput
{
	struct Button {
		bool pressed;  // True for one update when the button is first pressed.
		bool down;
		bool released; // True for one update when the button is release.
	};
	struct Mouse {
		int x, y; // In window client space.
		int xDelta, yDelta;
		Button leftButton, middleButton, rightButton;
	};

	bool quit;
	bool resized;
	DynamicArray<KeyInput> keyEvents;
	Mouse mouse;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	WindowInput* input = (WindowInput*)GetProp(hwnd, TEXT("messages"));
	if (msg == WM_INPUT) {
		RAWINPUT raw;
		unsigned int size = sizeof(RAWINPUT);
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
		KeyInput key = {0};
		key.scancode = raw.data.keyboard.MakeCode;
		key.extended = raw.data.keyboard.Flags & RI_KEY_E0;
		if (raw.data.keyboard.Flags & RI_KEY_BREAK) key.type = KeyInput::release;
		
		// 0x45 is an extra code that's generated from numpad keys and not needed.
		if (key.scancode != 0x45) input->keyEvents.push_back(key);
	}
	if (msg == WM_DESTROY) {
		input->quit = true;
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void createWindow(Window* window, uint width, uint height)
{
	HINSTANCE hInstance = GetModuleHandle(0);
	WNDCLASS wnd = {};
	wnd.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wnd.hInstance = hInstance;
	wnd.lpfnWndProc = WindowProc;
	wnd.lpszClassName = "GoblinWindowClass";
	wnd.hCursor = LoadCursor(0, IDC_ARROW);

	RegisterClass(&wnd);

	// CreateWindowEx takes the total size of the window, so we need to calculate how big the window should be to produce the desired client area
	RECT clientArea = {0, 0, (LONG)width, (LONG)height};
	AdjustWindowRect(&clientArea, WS_OVERLAPPEDWINDOW, FALSE);

	window->hwnd = CreateWindowEx(
		0,
		wnd.lpszClassName,
		NULL,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		clientArea.right - clientArea.left,
		clientArea.bottom - clientArea.top,
		0,
		0,
		hInstance,
		0);

	window->width = width;
	window->height = height;

	// Create graphics context
	// Create pixel buffer for OpenGL
	PIXELFORMATDESCRIPTOR pfd = {0};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 32;

	HDC deviceContext = GetDC(window->hwnd);
	int pixelFormat = ChoosePixelFormat(deviceContext, &pfd);
	SetPixelFormat(deviceContext, pixelFormat, &pfd);

	HGLRC renderingContext = wglCreateContext(deviceContext);
	if (renderingContext) {
		wglMakeCurrent(deviceContext, renderingContext);
	}
	ReleaseDC(window->hwnd, deviceContext);

	// Turn on VSync
	BOOL(__stdcall *wglSwapIntervalEXT)(int interval) = (BOOL(__stdcall*)(int)) wglGetProcAddress("wglSwapIntervalEXT");
	wglSwapIntervalEXT(1);

	// Setup RawInput. Gets keyboard messages even when the window is not focused.
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x06;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.hwndTarget = window->hwnd;
	RegisterRawInputDevices(&rid, 1, sizeof(RAWINPUTDEVICE));
}

void updateButton(WindowInput::Button* inout_button, unsigned int isDown)
{
	if (isDown) {
		inout_button->released = false;
		inout_button->pressed = !inout_button->down;
		inout_button->down = true;
	}
	else {
		inout_button->released = inout_button->down;
		inout_button->pressed = false;
		inout_button->down = false;
	}
}

void updateWindowInput(Window* win, WindowInput* input, bool waitForMessages)
{
	input->quit = false;
	input->resized = false;
	input->keyEvents.clear();

	SetProp(win->hwnd, TEXT("messages"), input);
	MSG msg;
	if (waitForMessages) {
		GetMessage(&msg, NULL, 0, 0);
		DispatchMessage(&msg);
	}
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		DispatchMessage(&msg);
	}

	// Check if the window has been resized
	RECT windowClientRect;
	GetClientRect(win->hwnd, &windowClientRect);
	if (win->width != windowClientRect.right || win->height != windowClientRect.bottom) {
		win->width = windowClientRect.right;
		win->height = windowClientRect.bottom;
		input->resized = true;
	}

	// Mouse
	POINT mousePosition;
	GetCursorPos(&mousePosition);
	ScreenToClient(win->hwnd, &mousePosition);
	input->mouse.xDelta = mousePosition.x - input->mouse.x;
	input->mouse.yDelta = mousePosition.y - input->mouse.y;
	input->mouse.x = mousePosition.x;
	input->mouse.y = mousePosition.y;

	updateButton(&input->mouse.leftButton, (1 << 16) & GetKeyState(VK_LBUTTON));
	updateButton(&input->mouse.rightButton, (1 << 16) & GetKeyState(VK_RBUTTON));
	updateButton(&input->mouse.middleButton, (1 << 16) & GetKeyState(VK_MBUTTON));
}

void swapBuffers(Window* window)
{
	HDC deviceContext = GetDC(window->hwnd);
	SwapBuffers(deviceContext);
	ReleaseDC(window->hwnd, deviceContext);
}

void setWindowTitle(Window* window, const char* string)
{
	SetWindowTextA(window->hwnd, string);
}

int getWindowWidth(Window window)
{
	RECT windowClientRect;
	GetClientRect(window.hwnd, &windowClientRect);
	return (int)windowClientRect.right;
}

int getWindowHeight(Window window)
{
	RECT windowClientRect;
	GetClientRect(window.hwnd, &windowClientRect);
	return (int)windowClientRect.bottom;
}

void simulateInput(KeyInput input)
{
	INPUT simulatedKey = { 0 };
	simulatedKey.type = INPUT_KEYBOARD;
	simulatedKey.ki.wScan = input.scancode;
	simulatedKey.ki.dwFlags |= KEYEVENTF_SCANCODE;
	if (input.extended) simulatedKey.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
	if (input.type == KeyInput::release) simulatedKey.ki.dwFlags |= KEYEVENTF_KEYUP;

	SendInput(1, &simulatedKey, sizeof(INPUT));
}

std::string keyToString(KeyInput key)
{
	uint extendedKeysFlag = 0;
	if (key.extended) {
		extendedKeysFlag = 1 << 24;
	}
	LONG fullScancode = (key.scancode<<16) | extendedKeysFlag;
	char buffer[64];
	int result = GetKeyNameTextA(fullScancode, buffer, 64);
	return buffer;
}

FILE* openFileFromSaveDialog()
{
	char path[MAX_PATH] = {0};
	OPENFILENAMEA ofn = {sizeof(OPENFILENAMEA)};
	ofn.lpstrFilter = "Recording (.rec)\0*.rec\0\0";
	ofn.lpstrDefExt = "rec";
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	if (GetSaveFileNameA(&ofn)) return fopen(path, "w");
	return 0;
}

FILE* openFileFromLoadDialog()
{
	char path[MAX_PATH] = {0};
	OPENFILENAMEA ofn = {sizeof(OPENFILENAMEA)};
	ofn.lpstrFilter = "Recording (.rec)\0*.rec\0\0";
	ofn.lpstrDefExt = "rec";
	ofn.lpstrFile = path;
	ofn.nMaxFile = MAX_PATH;
	if (GetOpenFileNameA(&ofn)) return fopen(path, "r");
	return 0;
}
