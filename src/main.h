/*
This file is part of Cuefinger 1

Cuefinger 1 gives you the possibility to remote control Universal Audio's
Console Application via Network (TCP).
Copyright © 2024 Frank Brempel

Cuefinger 1 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef _MAIN_
#define _MAIN_

#ifdef __linux__ 
#include "network_linux.h"
#include <limits.h>
#define MAX_PATH	PATH_MAX

#elif _WIN32
#include "network_win.h"
#include <Shellscalingapi.h>
#endif

#include "gfx2d_sdl.h"
#include "simdjson.h"
#include <map>
#include <queue>

#ifdef __ANDROID__
#include "cuefinger_jni.h"
#endif

using namespace simdjson;

const string APP_VERSION = "1.3.3";
const string APP_NAME = "Cuefinger";
const string WND_TITLE = APP_NAME + " " + APP_VERSION;
const string INFO_TEXT = APP_NAME + " " + APP_VERSION + "\n\
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n\
Idea & code\n\
Frank Brempel\n\
\n\
Graphics\n\
Michael Brempel\n\
\n\
Cuefinger uses SDL2 and simdjson\n\
www.libsdl.org | simdjson.org\n\
---\n\
Donations\n\
If you like Cuefinger, you can donate by\n\
buying my music on https://franqulator.de\n\
---\n\
This program comes with ABSOLUTELY NO WARRANTY.\n\
This is free software, and you are welcome to redistribute it\n\
under certain conditions; have a look at the COPYING file for details.\n\
\n\
Copyright © 2021 - 2024 Frank Brempel\n\
https://github.com/franqulator/cuefinger";
//#define SIMULATION 

#define SIMULATION_CHANNEL_COUNT	16
#define SIMULATION_SENDS_COUNT	6

#define WM_CREATE_SENDBUTTONS		WM_APP+10
#define WM_UPDATE_CONNECTBUTTONS	WM_APP+8
#define WM_CONNECTION_LOST			WM_APP+11

#define UA_SENDVALUE_RESOLUTION	0.002 // auflösung um performance zu verbessern und netzwerklast zu reduzieren => entspricht einer Rasterung auf 500 Stufen (1 / 0.002)
#define UA_METER_PRECISION	800.0

#define UA_MAX_SERVER_LIST	3 //könnte mehr sein, wenn mir eine GUI-Lösung einfällt
#define UA_TCP_PORT		"4710"

#define UA_MAX_SERVER_LIST_SETTING	7 

#define ID_BTN_CONNECT			50 // +connection index
#define ID_BTN_INFO				2
#define ID_BTN_PAGELEFT			3
#define ID_BTN_PAGERIGHT		4
#define ID_BTN_MIX				5
#define ID_BTN_SENDS			100 // + send-index
#define ID_BTN_SELECT_CHANNELS	6
#define ID_BTN_CHANNELWIDTH		7

#define BTN_COLOR_YELLOW	0
#define BTN_COLOR_RED		1
#define BTN_COLOR_GREEN		2
#define BTN_COLOR_BLUE		3

#define UNITY				1.0
#define METER_THRESHOLD		-76.0
#define METER_COLOR_BORDER	RGB(90, 90, 120)
#define METER_COLOR_BG		RGB(30, 30, 30)
#define METER_COLOR_GREEN	RGB(62, 175, 72)
#define METER_COLOR_YELLOW	RGB(200, 162, 42)
#define METER_COLOR_RED		RGB(250, 62, 42)

#define UA_SERVER_RESFRESH_TIME	20000 //in ms
#define IS_UA_SERVER_REFRESHING (GetTickCount64() - g_server_refresh_start_time < UA_SERVER_RESFRESH_TIME)

#define TOUCH_ACTION_NONE	0
#define TOUCH_ACTION_LEVEL	1
#define TOUCH_ACTION_PAN	2
#define TOUCH_ACTION_PAN2	3
#define TOUCH_ACTION_SOLO	4
#define TOUCH_ACTION_MUTE	5
#define TOUCH_ACTION_SELECT	6
#define TOUCH_ACTION_GROUP	7
#define TOUCH_ACTION_POST_FADER		8
#define TOUCH_ACTION_REORDER	9

#define INPUT	0
#define AUX		1
#define MASTER	2

#define SWITCH	-1
#define ON		1
#define OFF		0

#define NONE		0x00
#define PRESSED		0x01
#define RELEASED	0x02
#define CHECKED		0x10

#define BUTTON		0
#define CHECK		1
#define RADIO		2

#define ALL				0b000111111111
#define LEVEL			0b000000000001
#define PAN				0b000000000010
#define METER			0b000000000100
#define SOLO			0b000000001000
#define MUTE			0b000000010000
#define SEND_POST		0b000000100000
#define NAME			0b000001000000
#define STATE			0b000010000000
#define STEREO			0b000100000000
#define ALL_MIXES		0b001000000000

#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;

#define RED		RGB(140, 5, 5)
#define GREEN	RGB(0, 140, 20)
#define BLUE	RGB(0, 50, 180)
#define YELLOW	RGB(170, 170, 10)
#define ORANGE	RGB(190, 90, 20)
#define PURPLE	RGB(80, 10, 170)

#define LOG_INFO        0b0001
#define LOG_ERROR       0b0010
#define LOG_EXTENDED    0b0100

void writeLog(int type, const string &s);
void terminateAllPingThreads();

class Settings {
public:
	int x, y, w, h;
	bool maximized;
	bool fullscreen;
	int channel_width; // 1 narrow, 2 regular, 3 wide, 0 default (regular)
	bool lock_settings;
	string lock_to_mix;
	string serverlist[UA_MAX_SERVER_LIST];
	bool extended_logging;
	unsigned int reconnect_time;
	bool show_offline_devices;
	string label_aux1;
	string label_aux2;

	Settings() {
		x = 0;
		y = 0;
		w = 400;
		h = 320;
		maximized = false;
		fullscreen = false;
		channel_width = 0;
		lock_settings = false;
		extended_logging = false;
		reconnect_time = 10000;
		show_offline_devices = false;
		label_aux1 = "AUX";
		label_aux2 = "AUX";
	}
	bool load(const string& json = "");
	bool save();
    string toJSON();
};

class Button {
private:
	int id;
	std::string text;
	float x, y, w, h;
	int type;
	int state;
	bool enabled;
	bool visible;
	void (*onStateChanged)(Button *btn);
public:
	Button(int type=BUTTON, int id = 0, const string &text = "", float x = 0.0f, float y = 0.0f, float w = 0.0f, float h = 0.0f,
		bool checked = false, bool enabled = true, bool visible = true, void (*onStateChanged)(Button *btn) = NULL);
	bool onPress(SDL_Point *pt);
	bool onRelease();
	void setCheck(bool check);
	bool isChecked();
	int getState();
	void setVisible(bool visible);
	bool isVisible();
	void setEnable(bool enabled);
	bool isEnabled();
	void setText(const string &text);
	string getText();
	void setBounds(float x, float y, float w, float h);
	void setSize(float w, float h);
	float getX();
	float getY();
	float getWidth();
	float getHeight();
	int getId();
	bool isHighlighted();
	void draw(int color, const string &overrideName="");
};

class Touchpoint {
public:
	SDL_TouchID id;
	SDL_FingerID finger_id;
	float pt_start_x;
	float pt_start_y;
	float pt_end_x;
	float pt_end_y;
	int action;
	bool is_mouse;
	Touchpoint();
};

class UADevice {
public:
	string id;
	bool online;
	int channelsTotal;
	UADevice(const string &us_deviceId);
	~UADevice();
};

class Module {
public:
	string id;
	double level; // 0 - 4; 1 = unity, 2 = +6 dB, 4 = + 12dB
	double pan;
	double pan2;
	bool mute;
	double meter_level;
	double meter_level2;
	int subscriptions;
	bool clip;
	bool clip2;
	Module(const string &id);
	virtual ~Module();
	virtual void changeLevel(double level_change, bool absolute = false) {};
	virtual void changePan(double pan_change, bool absolute = false) {};
	virtual void changePan2(double pan_change, bool absolute = false) {};
	virtual void pressMute(int state = SWITCH) {};
};

class Channel;

class Send : public Module {
public:
	Channel *channel;

	Send(Channel* channel, const string &id);
	~Send();
	void init();
	void updateSubscription(bool subscribe, int flags);
	void changeLevel(double level_change, bool absolute = false) override; 
	void changePan(double pan_change, bool absolute = false) override;
	void pressMute(int state = SWITCH) override;
};

class Channel : public Module {
private:
	string name;
	string stereoname;
public:
	int type;
	UADevice* device;
	string properties;
	bool solo;
	bool post_fader;
	unordered_map<string, Send*> sendsByName;
	unordered_map<string, Send*> sendsById;
	bool stereo;
	bool hidden;
	bool enabledByUser;
	bool active;
	int label_gfx;
	float label_rotation;
	int fader_group;
	bool selected_to_show;

	Touchpoint touch_point;

	Channel(UADevice* device, const string &id, int type);
	~Channel();
	void init();
	void updateSubscription(bool subscribe, int flags); // also handles subscrption for sends
	bool isTouchOnFader(Vector2D *pos);
	bool isTouchOnPan(Vector2D *pos);
	bool isTouchOnPan2(Vector2D *pos);
	bool isTouchOnMute(Vector2D *pos);
	bool isTouchOnSolo(Vector2D *pos);
	bool isTouchOnPostFader(Vector2D* pos);
	bool isTouchOnGroup1(Vector2D *pos);
	bool isTouchOnGroup2(Vector2D *pos);
	void pressSolo(int state = SWITCH);
	void pressPostFader(int state = SWITCH);
	bool isOverriddenShow();
	bool isOverriddenHide();
	bool isVisible(bool only_selected);
	Send* getSendByName(const string &name);
	Send* getSendByUAId(const string &id);
	void updateProperties();
	string getName();
	void setName(const string &name);
	void setStereoname(const string &stereoname);
	void setStereo(bool stereo);
	void getColoredGfx(GFXSurface** gsLabel, GFXSurface** gsFader);
	void draw(float x, float y, float width, float height);
	Module* getModule(const string &mixBus);
	void changeLevel(double level_change, bool absolute = false) override;
	void changePan(double pan_change, bool absolute = false) override;
	void changePan2(double pan_change, bool absolute = false) override;
	void pressMute(int state = SWITCH) override;
};

void tcpClientSend(const string &msg);
bool connect(int);
void disconnect();
void draw();
bool loadAllGfx();
void releaseAllGfx();
bool loadServerSettings(const string &server_name, Button *btnSend);
bool loadServerSettings(const string &server_name);
bool saveServerSettings(const string &server_name);
Button *addSendButton(const string &name);
void updateConnectButtons();
void updateSubscriptions();
void initSettingsDialog();
void releaseSettingsDialog();
void cleanUp();
void browseToSelectedChannel(int index);
int getMiddleSelectedChannel();
void cleanUpUADevices();
void updateAllMuteBtnText();
void setRedrawWindow(bool redraw);
void updateChannelWidthButton();
void muteChannels(bool, bool);
int getActiveChannelsCount(bool onlyVisible);
void setLoadingState(bool loading);
bool isLoading();

inline double toDbFS(double linVal) {
	if (linVal <= 0.0)
		return -144.0;
	return 20.0 * log10(linVal);
}

inline double fromDbFS(double dbVal) {
	return pow(10.0, dbVal / 20.0);
}

inline double toMeterScale(double linVal) {
	return pow(linVal, 0.2) / pow(4.0, 0.2);
}

inline double fromMeterScale(double taperedVal) {
	return pow(taperedVal * pow(4.0, 0.2), 1.0 / 0.2);
}

#endif