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

struct RecordedInput
{
	enum Type {press, release};

	unsigned char key;
	uint32 frame;
	Type type;
};

struct AppData
{
	std::vector<RecordedInput> recording;
	Mode mode;
	unsigned char startRecordingKey;
	unsigned char playbackRecordingKey;
	uint32 recordingFrameNumber;
	uint nextPlaybackInputIndex;
	// Booleans
	int enabled;
	int trimStartup;
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
	uint scanCode = MapVirtualKey(keyCode, MAPVK_VK_TO_VSC);
	char buffer[64];
	GetKeyNameTextA(scanCode << 16, buffer, 64);
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
		nk_layout_row_static(ctx, 30, windowWidth - 25, 1);
		std::string label = "Record key: " + keyCodeToString(data->startRecordingKey);
		bool highlight = false;
		if (data->mode == Mode_waitingForRecordKey)
		{
			highlight = true;
			label = "Press any key";
		}
		if (doButton(ctx, label, highlight))
		{
			data->mode = Mode_waitingForRecordKey;
		}

		label = "Playback key: " + keyCodeToString(data->playbackRecordingKey);
		highlight = false;
		if (data->mode == Mode_waitingForPlaybackKey)
		{
			highlight = true;
			label = "Press any key";
		}
		if (doButton(ctx, label, highlight))
		{
			data->mode = Mode_waitingForPlaybackKey;
		}

		nk_layout_row_static(ctx, 20, windowWidth - 25, 1);
		nk_checkbox_label(ctx, "Enabled", &data->enabled);
		nk_checkbox_label(ctx, "Trim startup", &data->trimStartup);
	}
	nk_end(ctx);
}

void recordInputs(AppData* data, SystemInput input)
{
	// Skip over the first 7 keycodes for mouse buttons
	for (uint i=8; i<input.supportedKeyCount; ++i)
	{
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

		if (inputIndex == 0 && data->trimStartup)
		{
			// Skip ahead to first input
			data->recordingFrameNumber = data->recording[0].frame;
		}

		if (data->recording[inputIndex].frame > data->recordingFrameNumber) return;
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
	createWindow(&win, 280, 140);
	setWindowTitle(&win, "- Keyboard Recorder");
	SystemInput input;
	AppData data;
	GUI gui = {0};
	initGUI(&gui);
	bool run = true;

	data.mode = Mode_idle;
	data.startRecordingKey = VK_F1;
	data.playbackRecordingKey = VK_F2;
	data.recordingFrameNumber = 0;
	data.enabled = true;
	data.trimStartup = false;

	while (run)
	{
		if (!data.enabled) waitForWindowMessages(win); // Save power if we don't need to update every frame
		WindowMessages msg = processWindowMessages(&win);
		if (msg.quit) run = false;

		bool windowActive = win.hwnd == GetActiveWindow();
		int windowWidth = getWindowWidth(win);
		int windowHeight = getWindowHeight(win);

		updateSystemInput(&input, win);
		
		if (data.enabled && (data.mode == Mode_idle || data.mode == Mode_recording || data.mode == Mode_playback))
		{
			if (input.keyboard[data.startRecordingKey].pressed)
			{
				data.mode = Mode_recording;
				data.recordingFrameNumber = 0;
				data.recording.clear();
				setWindowTitle(&win, "O Keyboard Recorder");
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
			if (getAnyKeyDown(input, &data.startRecordingKey))
			{
				data.mode = Mode_idle;
			}
		}

		if (windowActive && data.mode == Mode_waitingForPlaybackKey)
		{
			if (getAnyKeyDown(input, &data.playbackRecordingKey))
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

		updateGUI(&gui, &data, input, windowWidth, windowHeight, windowActive);
		render(&gui, windowWidth, windowHeight);
		++data.recordingFrameNumber;
		swapBuffers(&win);
	}

	return 0;
}