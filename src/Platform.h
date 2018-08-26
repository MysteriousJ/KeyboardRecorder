#include <Windows.h>
#include <gl/GL.h>
#include <stdint.h>

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

struct WindowMessages
{
	bool quit;
	bool resized;
};

struct SystemInput
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

	static const uint supportedKeyCount = 0xFF;
	Button keyboard[supportedKeyCount];
	Mouse mouse;
	uint64 systemTime;
	double runTime;
	float deltaTime;
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY) {
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
}

WindowMessages processWindowMessages(Window* inout_win)
{
	WindowMessages result = {};
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			result.quit = true;
		}
	}

	// Check if the window has been resized
	RECT windowClientRect;
	GetClientRect(inout_win->hwnd, &windowClientRect);
	if (inout_win->width != windowClientRect.right || inout_win->height != windowClientRect.bottom) {
		inout_win->width = windowClientRect.right;
		inout_win->height = windowClientRect.bottom;
		result.resized = true;
	}

	return result;
}

void waitForWindowMessages(Window window)
{
	//WaitMessage();
	MSG msg;
	GetMessage(&msg, NULL, 0, 0);
	PostMessage(window.hwnd, msg.message, msg.wParam, msg.lParam);
}

void swapBuffers(Window* window)
{
	//wglSwapIntervalEXT(vSynch ? 1 : 0);
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

int64 getGlobalTime()
{
	LARGE_INTEGER time;
	QueryPerformanceCounter(&time);
	return time.QuadPart;
}

int64 getTicksPerSecond()
{
	LARGE_INTEGER ticksPerSecond;
	QueryPerformanceFrequency(&ticksPerSecond);
	return ticksPerSecond.QuadPart;
}

void updateButton(SystemInput::Button* inout_button, unsigned int isDown)
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

void updateSystemInput(SystemInput* input, Window window)
{
	// Mouse
	POINT mousePosition;
	GetCursorPos(&mousePosition);
	ScreenToClient(window.hwnd, &mousePosition);
	input->mouse.xDelta = mousePosition.x - input->mouse.x;
	input->mouse.yDelta = mousePosition.y - input->mouse.y;
	input->mouse.x = mousePosition.x;
	input->mouse.y = mousePosition.y;

	updateButton(&input->mouse.leftButton, (1 << 16)&GetKeyState(VK_LBUTTON));
	updateButton(&input->mouse.rightButton, (1 << 16)&GetKeyState(VK_RBUTTON));
	updateButton(&input->mouse.middleButton, (1 << 16)&GetKeyState(VK_MBUTTON));

	// Keyborad
	for (uint i=0; i<SystemInput::supportedKeyCount; ++i)
	{
		int isKeyDown = (1 << 16)&GetKeyState(i);
		updateButton(&input->keyboard[i], isKeyDown);
	}

	// Time
	uint64 newTime = getGlobalTime();
	uint64 previousTime = input->systemTime;
	uint64 ticksPerSecond = getTicksPerSecond();
	if (newTime > previousTime) {
		double deltaTime = double(newTime - previousTime) / double(ticksPerSecond);
		input->runTime += deltaTime;
		input->deltaTime = (float)deltaTime;
	}
	else {
		// If time is uninitialized or wrapped around, don't change delta time
		input->runTime += input->deltaTime;
	}
	input->systemTime = newTime;
}