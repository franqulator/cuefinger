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

using namespace simdjson;

const string APP_VERSION = "1.3.0";
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

#define UA_SENDVALUE_RESOLUTION	0.005 // auflösung um performance zu verbessern und netzwerklast zu reduzieren => entspricht einer Rasterung auf 200 Stufen (1 / 0.005)
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

#define DB_UNITY			1.0
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

#define UA_ALL_ENABLED_AND_ACTIVE	0
#define UA_VISIBLE					1

#define INPUT	0
#define AUX		1

#define SWITCH	-1
#define ON		1
#define OFF		0



#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;

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
	}
	bool load(string *json = NULL);
	bool save();
    string toJSON();
};

class Button {
public:
	int id;
	std::string text;
	SDL_Rect rc;
	bool checked;
	bool enabled;
	bool visible;
	
	Button(int _id=0, std::string _text="", int _x=0, int _y=0, int _w=0, int _h=0,
					bool _checked=false, bool _enabled=true, bool _visible=true);
	bool isClicked(SDL_Point *pt);
	void draw(int color);
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
	UADevice(string us_deviceId);
	~UADevice();
};

class Channel;

class Send {
public:
	string id;
	double gain;
	double pan;
	bool bypass;
	double meter_level;
	double meter_level2;
	Channel *channel;
	bool subscribed;
	bool clip;
	bool clip2;

	Send(Channel *channel, string id);
	~Send();
	void init();
	void updateSubscription(bool subscribe);
	void changePan(double pan_change, bool absolute = false);//relative
	void changeGain(double gain_change, bool absolute = false);//relative
	void pressBypass(int state=SWITCH);
};

class Channel {
public:
	string id;
	int type;
	string name;
	double level; // 0 - 4; 1 = unity, 2 = +6 dB, 4 = + 12dB
	double pan;
	bool solo;
	bool mute;
	double meter_level;
	double meter_level2;
	UADevice *device;
	unordered_map<string, Send*> sendsByName;
	unordered_map<string, Send*> sendsById;
	bool stereo;
	string stereoname;
	double pan2;
	bool hidden;
	bool enabledByUser;
	bool active;
	bool selected_to_show;
	char label_gfx;
	float label_rotation;
	int fader_group;
	bool subscribed;
	bool clip;
	bool clip2;

	Touchpoint touch_point;

	Channel(UADevice* device, string id, int type);
	~Channel();
	void init();
	void updateSubscription(bool subscribe);
	bool isTouchOnFader(Vector2D *pos);
	bool isTouchOnPan(Vector2D *pos);
	bool isTouchOnPan2(Vector2D *pos);
	bool isTouchOnMute(Vector2D *pos);
	bool isTouchOnSolo(Vector2D *pos);
	bool isTouchOnGroup1(Vector2D *pos);
	bool isTouchOnGroup2(Vector2D *pos);
	void changeLevel(double level_change, bool absolute = false); //relative value
	void changePan(double pan_change, bool absolute = false);//relative value
	void changePan2(double pan_change, bool absolute = false);//relative value
	void pressMute(int state=SWITCH);
	void pressSolo(int state=SWITCH);
	bool isOverriddenShow(bool ignoreStereoname = false);
	bool isOverriddenHide(bool ignoreStereoname = false);
	bool isVisible(bool only_selected);
	Send* getSendByName(string name);
	Send* getSendByUAId(string id);
};

void tcpClientSend(const char* msg);
bool connect(int);
void disconnect();
void draw();
bool loadAllGfx();
void releaseAllGfx();
bool loadServerSettings(string server_name, Button *btnSend);
bool loadServerSettings(string server_name, UADevice *dev);
bool saveServerSettings(string server_name);
Button *addSendButton(string name);
void updateConnectButtons();
int getAllChannelsCount(bool countWithHidden = true);
void updateSubscriptions();
void initSettingsDialog();
void releaseSettingsDialog();
void cleanUp();
void browseToSelectedChannel(int index);
int getMiddleSelectedChannel();
void cleanUpUADevices();
void updateAllMuteBtnText();


inline double toDbFS(double linVal) {
	if (linVal == 0.0)
		return -144.0;
	return 20.0 * log10(linVal);
}

inline double fromDbFS(double dbVal) {
	return pow(10.0, dbVal / 20.0);
}

inline double toMeterScale(double dbVal) {
	return pow(dbVal, 0.2) / pow(4.0, 0.2);
}

inline double fromMeterScale(double linVal) {
	return pow(linVal * pow(4.0, 0.2), 1.0 / 0.2);
}

#endif