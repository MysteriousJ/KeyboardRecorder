#include "Platform.h"
#include "GUI.h"

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
	KeyInput key;
	uint32 frame;
};

// Persistent data that needs to get passed around
struct AppData
{
	DynamicArray<RecordedInput> recording;
	Mode mode;
	PlaybackSpeed playbackSpeed;
	KeyInput startRecordingKey;
	KeyInput playbackRecordingKey;
	uint32 recordingFrameNumber;
	uint nextPlaybackInputIndex;
	int enabled;
	int loop;
};

void updateGUI(GUI* gui, AppData* data, WindowInput input, int windowWidth, int windowHeight, bool windowActive)
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
		std::string label = "Record key: " + keyToString(data->startRecordingKey);
		bool highlight = false;
		if (data->mode == Mode_waitingForRecordKey)
		{
			highlight = true;
			label = "Press any key";
		}
		if (doButton(ctx, label, highlight)) data->mode = Mode_waitingForRecordKey;

		label = "Playback key: " + keyToString(data->playbackRecordingKey);
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

		// Checkbox for loop
		nk_layout_row_dynamic(ctx, 30, 2);
		nk_checkbox_label(ctx, "Loop", &data->loop);
		// Checkbox for enable toggle
		nk_checkbox_label(ctx, "Enabled", &data->enabled);
	}
	nk_end(ctx);
}

void recordInputs(AppData* data, WindowInput input)
{
	for (uint i=0; i<input.keyEvents.count; ++i)
	{
		KeyInput key = input.keyEvents[i];
		// Skip keys used for recording and playback
		if (!(key.scancode == data->startRecordingKey.scancode && key.extended == data->startRecordingKey.extended)
			&& !(key.scancode == data->playbackRecordingKey.scancode && key.extended == data->playbackRecordingKey.extended))
		{
			RecordedInput action = {0};
			action.key = key;
			action.frame = data->recordingFrameNumber;
			data->recording.push_back(action);
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
			simulateInput(data->recording[inputIndex].key);
			data->nextPlaybackInputIndex += 1;
			return;
		}
		
		// Normal playback speed
		if (data->recording[inputIndex].frame > data->recordingFrameNumber)
		{
			return;
		}
		simulateInput(data->recording[inputIndex].key);
		data->nextPlaybackInputIndex += 1;
	}
	// Reached the end
	if (data->loop) {
		data->nextPlaybackInputIndex = 0;
		data->recordingFrameNumber = 0;
	}
	else {
		data->mode = Mode_idle;
	}
}

void releasePressedKeys(AppData* data)
{
	// If playback is cancelled, keys can get stuck down.
	// Send key-up messages for any keys that could be down when playback ended.
	for (unsigned int i=0; i<data->nextPlaybackInputIndex; ++i) {
		if (data->recording[i].key.type == KeyInput::press) {
			KeyInput release = data->recording[i].key;
			release.type = KeyInput::release;
			simulateInput(release);
		}
	}
}

bool keyWasPressed(DynamicArray<KeyInput> keys, KeyInput target)
{
	for (uint i = 0; i < keys.count; ++i) {
		if (keys[i].type == KeyInput::press && keys[i].scancode == target.scancode && keys[i].extended == target.extended) {
			return true;
		}
	}
	return false;
}

void startRecording(AppData* data, Window* win)
{
	data->mode = Mode_recording;
	data->recordingFrameNumber = 0;
	data->recording.clear();
	setWindowTitle(win, "O Keyboard Recorder");
}

void startPlayback(AppData* data, Window* win)
{
	data->mode = Mode_playback;
	data->recordingFrameNumber = 0;
	data->nextPlaybackInputIndex = 0;
	setWindowTitle(win, "> Keyboard Recorder");
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	Window win = {0};
	createWindow(&win, 280, 165);
	setWindowTitle(&win, "- Keyboard Recorder");
	WindowInput input = {0};
	AppData data = {0};
	GUI gui = {0};
	initGUI(&gui);
	bool run = true;

	data.startRecordingKey.scancode = MapVirtualKey(VK_F1, MAPVK_VK_TO_VSC);
	data.playbackRecordingKey.scancode = MapVirtualKey(VK_F2, MAPVK_VK_TO_VSC);
	data.enabled = true;

	// Frame-based loop like in a game
	while (run)
	{
		// Save power if we don't need to update every frame
		bool waitForMessages = data.mode == Mode_idle;

		// Handle window messages
		updateWindowInput(&win, &input, waitForMessages);
		if (input.quit) run = false;

		bool windowActive = win.hwnd == GetActiveWindow();
		int windowWidth = getWindowWidth(win);
		int windowHeight = getWindowHeight(win);
		
		// Update based on which mode the app is in
		if (data.mode == Mode_idle && data.enabled) {
			setWindowTitle(&win, "- Keyboard Recorder");
			if (keyWasPressed(input.keyEvents, data.startRecordingKey)) {
				startRecording(&data, &win);
			}
			if (keyWasPressed(input.keyEvents, data.playbackRecordingKey)) {
				startPlayback(&data, &win);
			}

		}
		else if (data.mode == Mode_waitingForRecordKey && windowActive) {
			if (input.mouse.leftButton.pressed) {
				data.mode = Mode_idle;
			}
			if (input.keyEvents.count > 0) {
				data.startRecordingKey = input.keyEvents[0];
				data.mode = Mode_idle;
			}
		}
		else if (data.mode == Mode_waitingForPlaybackKey && windowActive) {
			if (input.mouse.leftButton.pressed) {
				data.mode = Mode_idle;
			}
			if (input.keyEvents.count > 0) {
				data.playbackRecordingKey = input.keyEvents[0];
				data.mode = Mode_idle;
			}
		}
		else if (data.mode == Mode_recording) {
			if (keyWasPressed(input.keyEvents, data.startRecordingKey)) {
				data.mode = Mode_idle;
			}
			else if (keyWasPressed(input.keyEvents, data.playbackRecordingKey)) {
				startPlayback(&data, &win);
			}
			else {
				recordInputs(&data, input);
			}
		}
		else if (data.mode == Mode_playback) {
			if (!data.enabled) {
				data.mode = Mode_idle;
				setWindowTitle(&win, "- Keyboard Recorder");
			}
			else if (keyWasPressed(input.keyEvents, data.startRecordingKey)) {
				releasePressedKeys(&data);
				startRecording(&data, &win);
			}
			else if (keyWasPressed(input.keyEvents, data.playbackRecordingKey)) {
				releasePressedKeys(&data);
				startPlayback(&data, &win);
			}
			else {
				playbackInputs(&data);
			}
		}

		// GUI
		if (windowActive) {
			updateGUI(&gui, &data, input, windowWidth, windowHeight, windowActive);
			renderGUI(&gui, windowWidth, windowHeight);
		}

		// Finish frame
		++data.recordingFrameNumber;
		swapBuffers(&win);
	}

	return 0;
}