#include "Platform.h"
#include "GUI.h"
#include <vector>

enum Mode {
	Mode_idle,
	Mode_recording,
	Mode_playback,
	Mode_waitingForRecordKey,
	Mode_waitingForPlaybackKey
};

enum PlaybackSpeed {
	PlaybackSpeed_normal,
	PlaybackSpeed_trimStartup,
	PlaybackSpeed_fast
};

struct RecordedInput
{
	enum Type {press, release};

	unsigned char key;
	uint32 frame;
	Type type;
};

// Persistent data that needs to get passed around
struct AppData
{
	std::vector<RecordedInput> recording;
	Mode mode;
	PlaybackSpeed playbackSpeed;
	unsigned char startRecordingKey;
	unsigned char playbackRecordingKey;
	uint32 recordingFrameNumber;
	uint nextPlaybackInputIndex;
	// Booleans
	int enabled;
};


void simulateInput(RecordedInput input)
{
	if (input.type == RecordedInput::press)
	{
		keybd_event(input.key, 0, 0, NULL);
	}
	else if (input.type == RecordedInput::release)
	{
		keybd_event(input.key, 0, KEYEVENTF_KEYUP, NULL);
	}
}

std::string keyCodeToString(unsigned char keyCode)
{
	// GetKeyNameText names the arrow keys and pageup/pagedown/home/end/insert/delete
	// "Numpad x". The extended key flag will give these keys their proper names, but
	// messes up names for other keys, so we have to set it only for these specific
	// keys. A windows function that can determine whether the flag should be set
	// based on a virtual key code would be nice, but I haven't found one.
	uint extendedKeysFlag = 0;
	switch (keyCode) {
	case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_PRIOR: case VK_NEXT:
	case VK_HOME: case VK_END: case VK_INSERT: case VK_DELETE:
		extendedKeysFlag = 1 << 24;
	}

	HKL layout = GetKeyboardLayout(0);
	uint scanCode = MapVirtualKeyEx(keyCode, MAPVK_VK_TO_VSC_EX, layout);
	scanCode = MapVirtualKeyA(keyCode, MAPVK_VK_TO_VSC);
	
	char buffer[64];
	int result = GetKeyNameTextA((scanCode<<16) | extendedKeysFlag, buffer, 64);
	return buffer;
}

void updateGUI(GUI* gui, AppData* data, SystemInput input, int windowWidth, int windowHeight, bool windowActive)
{
	nk_context *ctx = &gui->ctx;

	// Copy input over to GUI
	nk_input_begin(ctx);
	if (windowActive)
	{
		nk_input_motion(ctx, input.mouse.x, input.mouse.y);
		nk_input_button(ctx, NK_BUTTON_LEFT, input.mouse.x, input.mouse.y, input.mouse.leftButton.down);
	}
	nk_input_end(ctx);

	// Layout GUI
	if (nk_begin(ctx, "GUI", nk_rect(0, 0, (float)windowWidth, (float)windowHeight), 0))
	{
		// Key setting buttons
		nk_layout_row_dynamic(ctx, 30, 1);
		std::string label = "Record key: " + keyCodeToString(data->startRecordingKey);
		bool highlight = false;
		if (data->mode == Mode_waitingForRecordKey)
		{
			highlight = true;
			label = "Press any key";
		}
		if (doButton(ctx, label, highlight)) data->mode = Mode_waitingForRecordKey;

		label = "Playback key: " + keyCodeToString(data->playbackRecordingKey);
		highlight = false;
		if (data->mode == Mode_waitingForPlaybackKey)
		{
			highlight = true;
			label = "Press any key";
		}
		if (doButton(ctx, label, highlight)) data->mode = Mode_waitingForPlaybackKey;

		// Playback speed radio buttons
		nk_layout_row_dynamic(ctx, 20, 1);
		nk_label(ctx, "Playback speed:", NK_TEXT_LEFT);
		nk_layout_row_begin(ctx, NK_STATIC, 20, 3);
		nk_layout_row_push(ctx, 50);
		if (nk_option_label(ctx, "1:1", data->playbackSpeed == PlaybackSpeed_normal)) data->playbackSpeed = PlaybackSpeed_normal;
		nk_layout_row_push(ctx, 110);
		if (nk_option_label(ctx, "Trim Startup", data->playbackSpeed == PlaybackSpeed_trimStartup)) data->playbackSpeed = PlaybackSpeed_trimStartup;
		nk_layout_row_push(ctx, 50);
		if (nk_option_label(ctx, "Fast", data->playbackSpeed == PlaybackSpeed_fast)) data->playbackSpeed = PlaybackSpeed_fast;
		nk_layout_row_end(ctx);

		// Enable checkbox
		nk_layout_row_dynamic(ctx, 30, 1);
		nk_checkbox_label(ctx, "Enabled", &data->enabled);
	}
	nk_end(ctx);
}

void recordInputs(AppData* data, SystemInput input)
{
	// Record all keys that were pressed this frame
	// Skip over the first 7 keycodes for mouse buttons
	for (uint i=8; i<input.supportedKeyCount; ++i)
	{
		// Skip keys used for recording and playback
		if (i != data->startRecordingKey
			&& i != data->playbackRecordingKey)
		{
			RecordedInput action = {0};
			action.key = i;
			action.frame = data->recordingFrameNumber;

			if (input.keyboard[i].pressed)
			{
				action.type = RecordedInput::Type::press;
				data->recording.push_back(action);
			}

			if (input.keyboard[i].released)
			{
				action.type = RecordedInput::Type::release;
				data->recording.push_back(action);
			}
		}
	}
}

void playbackInputs(AppData* data)
{
	while (data->nextPlaybackInputIndex < data->recording.size())
	{
		uint inputIndex = data->nextPlaybackInputIndex;

		// If trimming startup, skip ahead to first input
		if (inputIndex == 0 && data->playbackSpeed == PlaybackSpeed_trimStartup)
		{
			data->recordingFrameNumber = data->recording[0].frame;
		}

		if (data->playbackSpeed == PlaybackSpeed_fast)
		{
			simulateInput(data->recording[inputIndex]);
			data->nextPlaybackInputIndex += 1;
			return;
		}
		
		// Normal playback speed
		if (data->recording[inputIndex].frame > data->recordingFrameNumber)
		{
			return;
		}
		simulateInput(data->recording[inputIndex]);
		data->nextPlaybackInputIndex += 1;
	}
	// Reached the end
	data->mode = Mode_idle;
}

bool getAnyKeyDown(SystemInput input, unsigned char* out_key)
{
	// Skip over the first 7 keycodes for mouse buttons
	for (uint i=8; i<input.supportedKeyCount; ++i)
	{
		if (input.keyboard[i].pressed) {
			*out_key = i;
			return true;
		}
	}
	return false;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	Window win = {0};
	createWindow(&win, 280, 165);
	setWindowTitle(&win, "- Keyboard Recorder");
	SystemInput input;
	AppData data;
	GUI gui = {0};
	initGUI(&gui);
	bool run = true;

	data.mode = Mode_idle;
	data.playbackSpeed = PlaybackSpeed_normal;
	data.startRecordingKey = VK_F1;
	data.playbackRecordingKey = VK_F2;
	data.recordingFrameNumber = 0;
	data.enabled = true;

	// Frame-based loop like in a game
	while (run)
	{
		// Save power if we don't need to update every frame
		if (!data.enabled) waitForWindowMessages(win);

		// Handle window messages
		WindowMessages msg = processWindowMessages(&win);
		if (msg.quit) run = false;

		bool windowActive = win.hwnd == GetActiveWindow();
		int windowWidth = getWindowWidth(win);
		int windowHeight = getWindowHeight(win);

		updateSystemInput(&input, win);
		
		// Update based on which mode the app is in
		if (data.enabled && (data.mode == Mode_idle || data.mode == Mode_recording || data.mode == Mode_playback))
		{
			if (input.keyboard[data.startRecordingKey].pressed)
			{
				if (data.mode == Mode_recording)
				{
					data.mode = Mode_idle;
				}
				else
				{
					data.mode = Mode_recording;
					data.recordingFrameNumber = 0;
					data.recording.clear();
					setWindowTitle(&win, "O Keyboard Recorder");
				}
			}

			if (input.keyboard[data.playbackRecordingKey].pressed)
			{
				data.mode = Mode_playback;
				data.recordingFrameNumber = 0;
				data.nextPlaybackInputIndex = 0;
				setWindowTitle(&win, "> Keyboard Recorder");
			}
		}

		if (data.mode == Mode_idle)
		{
			setWindowTitle(&win, "- Keyboard Recorder");
		}

		if (windowActive && data.mode == Mode_waitingForRecordKey)
		{
			if (input.mouse.leftButton.pressed || getAnyKeyDown(input, &data.startRecordingKey))
			{
				data.mode = Mode_idle;
			}
		}

		if (windowActive && data.mode == Mode_waitingForPlaybackKey)
		{
			if (input.mouse.leftButton.pressed || getAnyKeyDown(input, &data.playbackRecordingKey))
			{
				data.mode = Mode_idle;
			}
		}

		if (data.mode == Mode_recording)
		{
			recordInputs(&data, input);
		}

		if (data.mode == Mode_playback)
		{
			playbackInputs(&data);
		}

		// GUI
		updateGUI(&gui, &data, input, windowWidth, windowHeight, windowActive);
		renderGUI(&gui, windowWidth, windowHeight);

		// Finish frame
		++data.recordingFrameNumber;
		swapBuffers(&win);
	}

	return 0;
}