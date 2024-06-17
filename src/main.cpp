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

#include "main.h"

int g_userEventBeginNum = 0;
#define USER_EVENT_COUNT				14
#define EVENT_CONNECT					1
#define EVENT_DISCONNECT				2
#define EVENT_DEVICES_INITIATE_RELOAD	3
#define EVENT_DEVICES_LOAD				4
#define EVENT_SENDS_LOAD				5
#define EVENT_OUTPUTS_LOAD				6
#define EVENT_INPUTS_LOAD				7
#define EVENT_AUXS_LOAD					8
#define EVENT_CHANNEL_STATE_CHANGED		9
#define EVENT_UPDATE_SUBSCRIPTIONS		10
#define EVENT_POST_FADER_METERING		11
#define EVENT_AFTER_UPDATE_PROPERTIES	12
#define EVENT_RESET_ORDER				13
#define EVENT_DEVICE_ONLINE				14

GFXEngine *gfx = NULL;

SDL_Window *g_window=NULL;
SDL_TimerID g_timer_network_serverlist = 0;
SDL_TimerID g_timer_network_reconnect = 0;
SDL_TimerID g_timer_network_timeout = 0;
SDL_TimerID g_timer_network_send_timeout_msg = 0;
SDL_TimerID g_timerFlipPage = 0;
SDL_TimerID g_timerResetOrder = 0;
SDL_TimerID g_timerUnmuteAll = 0;
SDL_TimerID g_timerSelectAllChannels = 0;
int g_dragPageFlip = 0;
int g_resetOrderCountdown = 0;
int g_unmuteAllCountdown = 0;
int g_selectAllChannelsCountdown = 0;

Settings g_settings;
map<string, string> serverSettingsJSON;

string g_msg = "";

vector<string> g_ua_server_list;
string g_ua_server_connected = "";
int g_ua_server_last_connection = -1;

string g_selectedMixBus = "MIX";

int g_channels_per_page = 0;
int g_browseToChannel = 0;

int g_activeChannelsCount = -1;
int g_visibleChannelsCount = -1;

bool g_redraw = true;
bool g_serverlist_defined = false;
bool g_reorder_dragging = false;

//Channel Layout (werden in UpdateLayout-Funktion berechnet)
const float MIN_BTN_WIDTH = 80.0f;
const float MAX_BTN_WIDTH = 160.0f;
const float SPACE_X = 2.0f;
const float SPACE_Y = 2.0f;
float g_channel_offset_x = 20.0f;
float g_channel_offset_y = 40.0f;
float g_fader_label_height;
float g_fadertracker_width;
float g_fadertracker_height;
float g_pantracker_width = 3.0f;
float g_pantracker_height = 28.0f;
float g_channel_height = 400.0f;
float g_channel_width = 100.0f;
float g_channel_pan_height = 30.0f;
float g_faderrail_width;
float g_faderrail_height;

float g_btn_width = 120.0f;
float g_btn_height = 60.0f;
float g_channel_btn_size;

float g_channel_label_fontsize;
float g_main_fontsize;
float g_info_fontsize;
float g_offline_fontsize;
float g_btnbold_fontsize;
float g_faderscale_fontsize;

vector<Button*> g_btnConnect;
vector<pair<string, Button*>> g_btnSends;
Button *g_btnSelectChannels=NULL;
Button* g_btnReorderChannels = NULL;
Button *g_btnChannelWidth=NULL;
Button *g_btnPageLeft=NULL;
Button *g_btnPageRight=NULL;
Button *g_btnMix=NULL;
Button *g_btnSettings=NULL;
Button* g_btnMuteAll = NULL;
Button* g_btnLockSettings = NULL;
Button* g_btnPostFaderMeter = NULL;
Button* g_btnShowOfflineDevices = NULL;
Button* g_btnLockToMixMIX = NULL;
Button* g_btnLockToMixCUE[4] = {NULL, NULL, NULL, NULL};
Button* g_btnLockToMixAUX[2] = { NULL, NULL };
Button* g_btnLabelAUX1[4] = { NULL, NULL, NULL, NULL };
Button* g_btnLabelAUX2[4] = { NULL, NULL, NULL, NULL };
Button* g_btnInfo = NULL;
Button* g_btnFullscreen = NULL;
Button* g_btnReconnect = NULL;
Button* g_btnServerlistScan = NULL;
vector<Button*> g_btnsServers;

TCPClient *g_tcpClient = NULL;
int g_page = 0;

GFXFont *g_fntMain = NULL;
GFXFont* g_fntInfo = NULL;
GFXFont *g_fntChannelBtn = NULL;
GFXFont *g_fntLabel = NULL;
GFXFont *g_fntFaderScale = NULL;
GFXFont *g_fntOffline = NULL;

GFXSurface *g_gsButtonYellow[2];
GFXSurface *g_gsButtonRed[2];
GFXSurface *g_gsButtonGreen[2];
GFXSurface *g_gsButtonBlue[2];
GFXSurface *g_gsChannelBg;
GFXSurface *g_gsBgRight;
GFXSurface *g_gsFaderrail;
GFXSurface *g_gsPan;
GFXSurface *g_gsPanPointer;
GFXSurface* g_gsFaderWhite;
GFXSurface* g_gsFaderBlack;
GFXSurface* g_gsFaderRed;
GFXSurface* g_gsFaderGreen;
GFXSurface* g_gsFaderBlue;
GFXSurface* g_gsFaderYellow;
GFXSurface* g_gsFaderOrange;
GFXSurface* g_gsFaderPurple;
GFXSurface *g_gsLabelWhite[4];
GFXSurface* g_gsLabelBlack[4];
GFXSurface* g_gsLabelRed[4];
GFXSurface* g_gsLabelGreen[4];
GFXSurface* g_gsLabelBlue[4];
GFXSurface* g_gsLabelYellow[4];
GFXSurface* g_gsLabelOrange[4];
GFXSurface* g_gsLabelPurple[4];

vector<SDL_Thread*> pingThreadsList;

unsigned long long g_server_refresh_start_time = -UA_SERVER_RESFRESH_TIME;
vector<UADevice*> g_ua_devices;
unordered_map<string, Channel*> g_channelsById;
map<int, Channel*> g_channelsInOrder;
set<Channel*> g_touchpointChannels;
set<string> g_channelsMutedBeforeAllMute;
string g_masterChannelId;

#define LOG_INFO        0b0001
#define LOG_ERROR       0b0010
#define LOG_EXTENDED    0b0100

void writeLog(int type, string s) {

	if (!(type & LOG_EXTENDED) || g_settings.extended_logging) {
		if (type & LOG_INFO) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", s.c_str());
		}
		else if (type & LOG_ERROR) {
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", s.c_str());
		}
	}
}

string json_workaround_secure_unicode_characters(string &input)
{
	size_t sz = 0;
	do
	{
		sz = input.find("\\u", sz);
		if (sz == string::npos)
			return input;

		input.replace(sz, 2,  "\\\\u");
		sz += 3;
	} while (true);

	return input;
}

inline string unescape_to_utf8(string &input)
{
	size_t sz = 0;
	do
	{
		sz = input.find("\\u", sz);
		if (sz == string::npos)
			return input;

		string num = input.substr(sz+2, 4);

		char c = (char)strtol(num.c_str(), NULL, 16);

		input.replace(sz, 6, 1, c);
		sz++;
	} while (true);

	return input;
}

void pushEvent(int eventId, void* data1=NULL, void* data2=NULL) {

	SDL_Event event;
	memset(&event, 0, sizeof(SDL_Event));
	event.type = SDL_USEREVENT;
	event.user.code = g_userEventBeginNum + eventId;
	event.user.data1 = data1;
	event.user.data2 = data2;
	SDL_PushEvent(&event);
}

int calculateMinChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 3.0f;
	int channels = (int)floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int calculateAverageChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 5.0f;
	int channels = (int)floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int calculateMaxChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = (float)h / 1.02f;

	float max_channel_width = mixer_height / 9.0f;
	int channels = (int)floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int calculateChannelsPerPage() {
	
	switch (g_settings.channel_width) {
	case 1:
		return calculateMaxChannelsPerPage();
	case 3:
		return calculateMinChannelsPerPage();
	}

	return calculateAverageChannelsPerPage();
}

int maxPages() {
	return (getActiveChannelsCount(true) - 1) / g_channels_per_page;
}

void updateMaxPages() {
	int max_pages = maxPages();
	if (g_page >= max_pages) {
		g_page = max_pages;
		g_browseToChannel = getMiddleSelectedChannel();
	}

	g_btnPageLeft->setEnable(g_page != 0);
	g_btnPageRight->setEnable(g_page != maxPages());
}

bool pageLeft() {
	if (g_page > 0) {
		g_page--;
		pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
		g_browseToChannel = getMiddleSelectedChannel();
		setRedrawWindow(true);

		g_btnPageLeft->setEnable(g_page != 0);
		g_btnPageRight->setEnable(g_page != maxPages());

		return true;
	}
	return false;
}

bool pageRight() {
	if (g_page < maxPages()) {
		g_page++;
		pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
		g_browseToChannel = getMiddleSelectedChannel();
		setRedrawWindow(true);

		g_btnPageLeft->setEnable(g_page != 0);
		g_btnPageRight->setEnable(g_page != maxPages());

		return true;
	}
	return false;
}

void updateLayout() {

	g_channels_per_page = calculateChannelsPerPage();
	
	int w,h;
	SDL_GetWindowSize(g_window, &w, &h);
	float win_width = (float)w;
	float win_height = (float)h;
	/*
	int smaller_btn_height = win_height /7;
	if (smaller_btn_height < g_btn_height)
		g_btn_height = smaller_btn_height;
	*/

	g_btn_width = (win_width / (2.0f + (float)g_channels_per_page)) / 1.2f;
	if (g_btn_width < MIN_BTN_WIDTH)
		g_btn_width = MIN_BTN_WIDTH;
	if (g_btn_width > MAX_BTN_WIDTH)
		g_btn_width = MAX_BTN_WIDTH;

	g_btn_height = win_height / 10.0f;

	if (g_btn_height > g_btn_width)
		g_btn_height = g_btn_width;

	g_channel_width = ((win_width - 2.0f * (g_btn_width * 1.2f + SPACE_X)) / (float)g_channels_per_page);
	g_channel_height = win_height / 1.02f;

	g_channel_offset_x = (win_width - (float)g_channels_per_page * g_channel_width) / 2.0f;
	g_channel_offset_y = (win_height - (float)g_channel_height) / 2.0f;

	g_channel_btn_size = min(g_btn_height * 0.8f, g_channel_width * 0.4f);
	if (g_channel_btn_size > g_channel_height / 10.0f)
		g_channel_btn_size = g_channel_height / 10.0f;

	g_fadertracker_height = g_channel_height / 7.0f;
	g_fadertracker_width = g_channel_width / 1.5f;
	if (g_fadertracker_width > g_fadertracker_height / 1.5f)
		g_fadertracker_width = g_fadertracker_height / 1.5f;

	g_faderrail_width = 11.0f;
	if (g_channel_width * 0.07f < g_faderrail_width)
		g_faderrail_width = g_channel_width * 0.07f;

	g_fader_label_height = max(28.0f, g_channel_height / 18);

	g_channel_pan_height = g_channel_width;
	if (g_channel_pan_height > g_channel_height / 8.0f)
		g_channel_pan_height = g_channel_height / 8.0f;

	g_pantracker_height = g_channel_pan_height * 1.1f;
	if (g_pantracker_height > g_channel_width)
		g_pantracker_height = g_channel_width;
	
	//reload labelfont if size changed
	if (g_channel_label_fontsize != g_fader_label_height * 0.5f)
	{
		g_channel_label_fontsize = g_fader_label_height * 0.5f;

		if (g_fntLabel) {
			gfx->DeleteFont(g_fntLabel);
			g_fntLabel = NULL;
		}

		g_fntLabel = gfx->CreateFontFromFile(getDataPath("chelseamarket.ttf"), (int)round(g_channel_label_fontsize * 96.0f / 100.0f));
	}

	float new_main_fontsize = min(g_btn_height / 3.0f, g_btn_width / 7.0f);
	//reload mainfont if size changed
	if (g_main_fontsize != new_main_fontsize) {
		g_main_fontsize = new_main_fontsize;

		if (g_fntMain) {
			gfx->DeleteFont(g_fntMain);
			g_fntMain = NULL;
		}

		g_fntMain = gfx->CreateFontFromFile(getDataPath("chakrapetch.ttf"), (int)round(g_main_fontsize * 96.0f / 100.0f), false);
	}

//	float new_info_fontsize = min(win_height / 33.0f, win_width / 33.0f);
	float new_info_fontsize = min(g_btn_height / 2.0f, g_btn_width / 5.5f);
	if (g_info_fontsize != new_info_fontsize) {
		g_info_fontsize = new_info_fontsize;
		if (g_fntInfo) {
			gfx->DeleteFont(g_fntInfo);
			g_fntInfo = NULL;
		}

		g_fntInfo = gfx->CreateFontFromFile(getDataPath("chakrapetch.ttf"), (int)round(g_info_fontsize * 96.0f / 100.0f));
	}

	float new_faderscale_fontsize = min(g_channel_btn_size / 4.0f, g_channel_label_fontsize);
	if (g_faderscale_fontsize != new_faderscale_fontsize) {
		g_faderscale_fontsize = new_faderscale_fontsize;

		if (g_fntFaderScale) {
			gfx->DeleteFont(g_fntFaderScale);
			g_fntFaderScale = NULL;
		}

		g_fntFaderScale = gfx->CreateFontFromFile(getDataPath("chakrapetch.ttf"), (int)round(g_faderscale_fontsize * 96.0f / 100.0f));
	}

	float new_btnbold_fontsize = g_channel_btn_size / 3;
	if (g_btnbold_fontsize != new_btnbold_fontsize) {
		g_btnbold_fontsize = new_btnbold_fontsize;

		if (g_fntChannelBtn) {
			gfx->DeleteFont(g_fntChannelBtn);
			g_fntChannelBtn = NULL;
		}

		g_fntChannelBtn = gfx->CreateFontFromFile(getDataPath("chakrapetch.ttf"), (int)round(g_btnbold_fontsize * 96.0f / 100.0f), false);
	}

	float new_offline_fontsize = min(win_width / 6.0f, win_height / 3.0f);

	//reload font if size changed
	if (g_offline_fontsize != new_offline_fontsize) {
		g_offline_fontsize = new_offline_fontsize;

		if (g_fntOffline) {
			gfx->DeleteFont(g_fntOffline);
			g_fntOffline = NULL;
		}

		g_fntOffline = gfx->CreateFontFromFile(getDataPath("changa.ttf"), (int)round(g_offline_fontsize * 96.0f / 100.0f));
	}

	//btnleiste links
	g_btnSettings->setBounds(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X, g_channel_offset_y, g_btn_width, g_btn_height);
	g_btnMuteAll->setBounds(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X, g_channel_offset_y + g_btn_height + SPACE_Y,
		g_btn_width, g_btn_height);

	float sends_offset_y = (win_height - g_channel_offset_y + g_btn_height - (g_btn_height + SPACE_Y) * (float)g_btnSends.size()) / 2.0f;
	
	int n = 0;
	for(vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		it->second->setBounds(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X, sends_offset_y + (g_btn_height + SPACE_Y) * (float)n,
			g_btn_width, g_btn_height);
		n++;
	}

	g_btnMix->setBounds(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X, win_height - g_channel_offset_y - g_btn_height,
		g_btn_width, g_btn_height);

	//btnleiste rechts
	for (int n = 0; n < (int)g_btnConnect.size(); n++)
	{
		g_btnConnect[n]->setBounds(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f, g_channel_offset_y + (float)n * (g_btn_height + SPACE_Y),
			g_btn_width, g_btn_height);
	}

	g_btnSelectChannels->setBounds(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f, win_height / 2.0f - (g_btn_height + SPACE_Y / 2.0f),
		g_btn_width, g_btn_height);

	g_btnReorderChannels->setBounds(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f, 
		win_height / 2.0f - (g_btn_height + SPACE_Y / 2.0f) + g_btn_height + SPACE_Y,
		g_btn_width, g_btn_height);

	g_btnChannelWidth->setBounds(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f,
		win_height - g_channel_offset_y - 3.0f * g_btn_height - g_main_fontsize - SPACE_Y * 3.0f,
		g_btn_width, g_btn_height);

	g_btnPageLeft->setBounds(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f,
		win_height - g_channel_offset_y - 2.0f * g_btn_height - SPACE_Y,
		g_btn_width, g_btn_height);

	g_btnPageRight->setBounds(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f,
		win_height - g_channel_offset_y - g_btn_height,
		g_btn_width, g_btn_height);

	//settings dialog
	float x = g_channel_offset_x + 20.0f;
	float y = 20.0f;
	float vspace = 20.0f;

	g_btnInfo->setBounds(x + g_main_fontsize * 10.0f + g_btn_width, y, g_btn_width, g_btn_height / 2.0f);
	g_btnFullscreen->setBounds(x + g_main_fontsize * 10.0f, y, g_btn_width, g_btn_height / 2.0f);
	g_btnLockSettings->setBounds(x, y, g_btn_width, g_btn_height / 2.0f);
	y += g_btn_height / 2.0f + 2.0f + vspace;

	g_btnLockToMixMIX->setBounds(x + g_main_fontsize * 10.0f, y, g_btn_width / 2.0f, g_btn_height / 2.0f);
	for (int n = 0; n < 2; n++) {
		g_btnLockToMixAUX[n]->setBounds(x + g_main_fontsize * 10.0f + (float)(1 + n) * g_btn_width / 2.0f,
			y, g_btn_width / 2.0f, g_btn_height / 2.0f);
	}
	y += g_btn_height / 2.0f + 2.0f;
	for (int n = 0; n < 4; n++) {
		g_btnLockToMixCUE[n]->setBounds(x + g_main_fontsize * 10.0f + (float)n * g_btn_width / 2.0f, y,
			g_btn_width / 2.0f, g_btn_height / 2.0f);
	}

	y += g_btn_height / 2.0f + 2.0f + vspace;

	for (int n = 0; n < 4; n++) {
		g_btnLabelAUX1[n]->setBounds(x + g_main_fontsize * 10.0f + (float)n * g_btn_width / 2.0f, y,
			g_btn_width / 2.0f, g_btn_height / 2.0f);
	}

	y += g_btn_height / 2.0f + 2.0f;

	for (int n = 0; n < 4; n++) {
		g_btnLabelAUX2[n]->setBounds(x + g_main_fontsize * 10.0f + (float)n * g_btn_width / 2.0f, y,
			g_btn_width / 2.0f, g_btn_height / 2.0f);
	}

	y += g_btn_height / 2.0f + 2.0f + vspace;

	g_btnPostFaderMeter->setBounds(x + g_main_fontsize * 10.0f, y, g_btn_width, g_btn_height / 2.0f);
	y += g_btn_height / 2.0f + 2.0f + vspace;

	g_btnShowOfflineDevices->setBounds(x + g_main_fontsize * 10.0f, y, g_btn_width, g_btn_height / 2.0f);
	y += g_btn_height / 2.0f + 2.0f + vspace;

	g_btnReconnect->setBounds(x + g_main_fontsize * 10.0f, y, g_btn_width, g_btn_height / 2.0f);
	y += g_btn_height / 2.0f + 2.0f + vspace;

	g_btnServerlistScan->setBounds(x + g_main_fontsize * 10.0f, y, g_btn_width, g_btn_height / 2.0f);

	for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
		(*it)->setBounds(x + g_main_fontsize * 10.0f + (g_btn_width + 2.0f), y, g_btn_width, g_btn_height / 2.0f);
		y += g_btn_height / 2.0f + 2.0f;
	}

	browseToSelectedChannel(g_browseToChannel);
	updateMaxPages();

	setRedrawWindow(true);
}

Button::Button(int type, int id, string text, float x, float y, float w, float h, bool checked, 
	bool enabled, bool visible, void (*onStateChanged)(Button*)) {
	this->type = type;
	this->id = id;
	this->text = text;
	this->x = x;
	this->y = y;
	this->w = w;
	this->h = h;
	this->state = checked ? CHECKED : NONE;
	this->enabled = enabled;
	this->visible = visible;
	this->onStateChanged = onStateChanged;
}

bool Button::onPress(SDL_Point *pt) {
	if(this->visible && this->enabled) {
		SDL_Rect rc = { (int)this->x, (int)this->y, (int)this->w, (int)this->h};
		if ((bool)SDL_PointInRect(pt, &rc)) {
			int oldState = this->state;
			this->state &= 0x10;
			this->state |= PRESSED;
			if (oldState != this->state) {
				if(onStateChanged)
					onStateChanged(this);
				setRedrawWindow(true);
			}
			return true;
		}
	}
	return false;
}


bool Button::onRelease() {
	if (this->state & PRESSED) {
		int oldState = this->state;
		if (this->type == BUTTON) {
			this->state = RELEASED;
		}
		else {
			this->state &= 0x10;
			if (this->type == CHECK) {
				this->state ^= 0x10;
			}
			else {
				this->state |= 0x10;
			}
			this->state |= RELEASED;
		}

		if (oldState != this->state) {
			if (onStateChanged)
				onStateChanged(this);
			setRedrawWindow(true);
		}

		return true;
	}
	return false;
}

void Button::setBounds(float x, float y, float w, float h) {
	this->x = x;
	this->y = y;
	this->setSize(w, h);
}

void Button::setSize(float w, float h) {
	this->w = w;
	this->h = h;
	setRedrawWindow(true);
}

float Button::getX() {
	return this->x;
}
float Button::getY() {
	return this->y;
}
float Button::getWidth() {
	return this->w;
}
float Button::getHeight() {
	return this->h;
}

bool Button::isHighlighted() {
	return this->state & CHECKED || this->state & PRESSED;
}

void Button::setCheck(bool check) {
	int oldState = this->state;
	if (check) {
		this->state = CHECKED;
	}
	else {
		this->state = NONE;
	}
	if (oldState != this->state) {
		if (onStateChanged)
			onStateChanged(this);
		setRedrawWindow(true);
	}
}

bool Button::isChecked() {
	return this->state & CHECKED;
}


int Button::getState() {
	return this->state;
}

void Button::setVisible(bool visible) {
	this->visible = visible;
	setRedrawWindow(true);
}

bool Button::isVisible() {
	return this->visible;
}

void Button::setEnable(bool enabled) {
	this->enabled = enabled;
	setRedrawWindow(true);
}

bool Button::isEnabled() {
	return this->enabled;
}

void Button::setText(string text) {
	this->text = text;
	setRedrawWindow(true);
}

string Button::getText() {
	return this->text;
}

int Button::getId() { return this->id; }

void Button::draw(int color, string overrideName)
{
	if (!this->visible)
		return;

	int btn_switch = 0;
	if (this->isHighlighted()) // pressed or checked
	{
		btn_switch = 1;
	}

//	DrawColor(x, y, width, height, bgClr);
	Vector2D stretch = Vector2D(this->w, this->h);

	GFXSurface* gs = g_gsButtonYellow[btn_switch];
	if (color == BTN_COLOR_RED) {
		gs = g_gsButtonRed[btn_switch];
	}
	else if (color == BTN_COLOR_GREEN) {
		gs = g_gsButtonGreen[btn_switch];
	}
	else if (color == BTN_COLOR_BLUE) {
		gs = g_gsButtonBlue[btn_switch];
	}

	COLORREF clr = RGB(0, 0, 0);
	if (!this->enabled && !(this->state & PRESSED)) {
		gfx->SetFilter_Saturation(gs, -0.1f);
		gfx->SetFilter_Brightness(gs, 0.9f);
		gfx->SetFilter_Contrast(gs, 0.7f);
		clr = RGB(70, 70, 70);
	}
	gfx->Draw(gs, this->x, this->y, NULL, GFX_NONE, 1.0f, 0, NULL, &stretch);

	gfx->SetFilter_Saturation(gs, 0.0f);
	gfx->SetFilter_Brightness(gs, 1.0f);
	gfx->SetFilter_Contrast(gs, 1.0f);

	string text = overrideName.length() == 0 ? this->text : overrideName;

	float max_width = this->w * 0.9f;
	Vector2D sz = gfx->GetTextBlockSize(g_fntMain, text, GFX_CENTER | GFX_AUTOBREAK, max_width);
	Vector2D szText(max_width, this->h);
	gfx->SetColor(g_fntMain, clr);
	gfx->Write(g_fntMain, this->x + (this->w - max_width) / 2.0f, this->y + this->h / 2.0f - sz.getY() / 2.0f, text, GFX_CENTER | GFX_AUTOBREAK, &szText, 0, 0.8f);
}

Touchpoint::Touchpoint() {
	memset(this, 0, sizeof(Touchpoint));
}

Module::Module(string id) {
	this->id = id;
	this->level = 0.0;
	this->mute = false;
	this->pan = 0.0;
	this->pan2 = 0.0;
	this->meter_level = fromDbFS(-144.0);
	this->meter_level2 = fromDbFS(-144.0);
	this->clip = false;
	this->clip2 = false;
	this->subscriptions = 0;
}

Module::~Module() {
}

Send::Send(Channel* channel, string id) : Module(id) {
	this->channel = channel;
}
Send::~Send() {
	this->updateSubscription(false, ALL);
}

void Send::init() {
	this->updateSubscription(true, MUTE);
}

void Send::changePan(double pan_change, bool absolute)
{
	if (this->channel->stereo)
		return;

	double _pan = this->pan + pan_change;
	if (absolute)
		_pan = pan_change;

	if (_pan > 1)
	{
		_pan = 1;
	}
	else if (_pan < -1)
	{
		_pan = -1;
	}

	if (this->pan != _pan)
	{
		this->pan = _pan;

		string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";

		string msg = "set /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Pan/value/ " + to_string(this->pan);
		tcpClientSend(msg.c_str());
	}
}

void Send::changeLevel(double level_change, bool absolute)
{
	double _level = 0.0;
	if (absolute) {
		_level = level_change;
	}
	else {
		_level = fromMeterScale(toMeterScale(this->level) + level_change);
	}

	_level = min(_level, 4.0);
	_level = max(_level, fromDbFS(-144.0));

	if (this->level != _level) {
		this->level = _level;

		string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";
		string msg = "set /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Gain/value/ " + to_string(toDbFS(this->level));
		tcpClientSend(msg.c_str());
	}
}

void Send::pressMute(int state)
{
	if (state == SWITCH)
		this->mute = !this->mute;
	else
		this->mute = (bool)state;

	string value = "true";
	if (!this->mute)
		value = "false";

	string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";
	string msg = "set /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Bypass/value/ " + value;
	tcpClientSend(msg.c_str());
}


void Send::updateSubscription(bool subscribe, int flags)
{
	string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";
	string root = "";
	if (!subscribe) {
		root += "un";
	}
	root += "subscribe /devices/" + this->channel->device->id;
	
	if (this->channel->type == INPUT) {
		root += "/inputs/" + this->channel->id + "/sends/" + this->id;
	}
	else if (this->channel->type == AUX) {
		root += "/auxs/" + this->channel->id + "/sends/" + this->id;
	}

	if ((flags & LEVEL) && ((bool)(this->subscriptions & LEVEL) != subscribe)) {
		string cmd = root + "/Gain/";
		tcpClientSend(cmd.c_str());
		this->subscriptions ^= LEVEL;
	}

	if ((flags & PAN) && ((bool)(this->subscriptions & PAN) != subscribe)) {
		if (this->channel->type == INPUT) {
			string cmd = root + "/Pan/";
			tcpClientSend(cmd.c_str());
			this->subscriptions ^= PAN;
		}
	}

	if ((flags & METER) && ((bool)(this->subscriptions & METER) != subscribe)) {
		string cmd = root + "/meters/0/MeterLevel/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/0/MeterClip/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/1/MeterLevel/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/1/MeterClip/";
		tcpClientSend(cmd.c_str());

		this->subscriptions ^= METER;
	}

	if ((flags & MUTE) && ((bool)(this->subscriptions & MUTE) != subscribe)) {
		string cmd = root + "/Bypass/";
		tcpClientSend(cmd.c_str());
		this->subscriptions ^= MUTE;
	}
}

Channel::Channel(UADevice* device, string id, int type) : Module(id) {
	this->label_gfx = rand() % 4;
	this->label_rotation = (float)(rand() % 100) / 2000.0f - 0.025f;
	this->device = device;
	this->type = type;
	this->solo = false;
	this->post_fader = false;
	if (this->type == AUX || this->type == MASTER) {
		this->hidden = false;
		this->enabledByUser = true;
		this->active = false;
		this->stereo = true;
	}
	else {
		this->hidden = true;
		this->enabledByUser = false;
		this->active = false;
		this->stereo = false;
	}
	this->selected_to_show = true;
	this->fader_group = 0;
}
Channel::~Channel() {
	this->updateSubscription(false, ALL | ALL_MIXES);

	for (unordered_map<string, Send*>::iterator it = this->sendsByName.begin(); it != this->sendsByName.end(); ++it) {
		SAFE_DELETE(it->second);
	}
	this->sendsById.clear();
	this->sendsByName.clear();
}

void Channel::init() {
	this->updateSubscription(true, STEREO | NAME | MUTE | STATE | ALL_MIXES);

	if (this->type != MASTER) {
		string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
		string cmd = "get /devices/" + this->device->id + type_str + this->id + "/sends";
		tcpClientSend(cmd.c_str());
	}
}

bool Channel::isVisible(bool only_selected) {

	if ((only_selected && !this->selected_to_show) || (!this->device->online && !g_btnShowOfflineDevices->isHighlighted())) {
		return false;
	}

	for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		if (it->second->isHighlighted()) {
			Send* send = this->getSendByName(it->second->getText());
			if (!send) {
				return false;
			}
		}
	}

	return (!this->hidden && (!this->stereo || (stoi(this->id, NULL, 10) % 2) == 0 || this->type == AUX || this->type == MASTER)
		&& this->enabledByUser && this->active);
}

bool Channel::isTouchOnFader(Vector2D *pos)
{
	float y = (g_fader_label_height + g_channel_pan_height + g_channel_btn_size);
	float height = g_channel_height - y - g_channel_btn_size;
	return (pos->getX() > 0 && pos->getX() < g_channel_width
		&& pos->getY() >y && pos->getY() < y + height);
}

bool Channel::isTouchOnPan(Vector2D *pos)
{
	float width = g_channel_width;

	if (this->stereo)
		width /= 2.0f;

	return (pos->getX() > 0 && pos->getX() < width
		&& pos->getY() > g_fader_label_height && pos->getY() < (g_fader_label_height + g_channel_pan_height));
}

bool Channel::isTouchOnPan2(Vector2D *pos)
{
	float width = g_channel_width / 2.0f;

	return (pos->getX() > width && pos->getX() < g_channel_width
		&& pos->getY() > g_fader_label_height && pos->getY() < (g_fader_label_height + g_channel_pan_height));
}

bool Channel::isTouchOnGroup1(Vector2D *pos)
{
	float x = (g_channel_width / 2.0f - g_channel_btn_size) / 2.0f;
	float y = g_channel_height - g_channel_btn_size;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnGroup2(Vector2D *pos)
{
	float x = g_channel_width / 2.0f + (g_channel_width / 2.0f - g_channel_btn_size) / 2.0f;
	float y = g_channel_height - g_channel_btn_size;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnMute(Vector2D *pos)
{
	float x = g_channel_width / 2.0f + (g_channel_width / 2.0f - g_channel_btn_size) / 2.0f;
	float y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnSolo(Vector2D *pos)
{
	if (this->type != INPUT)
		return false;

	float x = (g_channel_width / 2.0f - g_channel_btn_size) / 2.0f;
	float y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnPostFader(Vector2D* pos)
{
	if (this->type != AUX)
		return false;

	float x = (g_channel_width / 2.0f - g_channel_btn_size) / 2.0f;
	float y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

void Channel::changePan(double pan_change, bool absolute)
{
	double _pan = this->pan + pan_change;
	if (absolute)
		_pan = pan_change;

	if (_pan > 1.0)
	{
		_pan = 1.0;
	}
	else if (_pan < -1.0)
	{
		_pan = -1.0;
	}

	if (this->pan != _pan)
	{
		this->pan = _pan;
		string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
		string msg = "set /devices/" + this->device->id + type_str + this->id + "/Pan/value/ " + to_string(this->pan);
		tcpClientSend(msg.c_str());
	}
}

void Channel::changePan2(double pan_change, bool absolute)
{
	double _pan = this->pan2 + pan_change;
	if (absolute)
		_pan = pan_change;

	if (_pan > 1.0)
	{
		_pan = 1.0;
	}
	else if (_pan < -1.0)
	{
		_pan = -1.0;
	}

	if (this->pan2 != _pan)
	{
		this->pan2 = _pan;
		string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
		string msg = "set /devices/" + this->device->id + type_str + this->id + "/Pan2/value/ " + to_string(this->pan2);
		tcpClientSend(msg.c_str());
	}
}

void Channel::changeLevel(double level_change, bool absolute)
{
	double _level = 0;

	if (absolute) {
		_level = level_change;
	}
	else {
		_level = fromMeterScale(toMeterScale(this->level) + level_change);
	}
	
	if (this->type == MASTER) {
		_level = min(_level, 1.0);
	}
	else {
		_level = min(_level, 4.0);
	}
	_level = max(_level, fromDbFS(-144.0));

	if (this->level != _level)
	{
		this->level = _level;

		if (this->type == MASTER) {
			string msg = "set /devices/" + this->device->id + "/outputs/" + this->id + "/CRMonitorLevel/value/ " + to_string(toDbFS(_level));
			tcpClientSend(msg.c_str());
		}
		else {
			string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
			string msg = "set /devices/" + this->device->id + type_str + this->id + "/FaderLevel/value/ " + to_string(toDbFS(_level));
			tcpClientSend(msg.c_str());
		}
	}
}

void Channel::pressMute(int state)
{
	if (state == SWITCH)
		this->mute = !this->mute;
	else
		this->mute = (bool)state;


	string value = "true";
	if (!this->mute)
		value = "false";

	if (this->type == MASTER) {
		string msg = "set /devices/" + this->device->id + "/outputs/" + this->id + "/Mute/value/ " + value;
		tcpClientSend(msg.c_str());
	}
	else {
		string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
		string msg = "set /devices/" + this->device->id + type_str + this->id + "/Mute/value/ " + value;
		tcpClientSend(msg.c_str());
	}
}

void Channel::pressSolo(int state)
{
	if (state == SWITCH)
		this->solo = !this->solo;
	else
		this->solo = (bool)state;

	string value = "true";
	if (!this->solo)
		value = "false";

	string msg = "set /devices/" + this->device->id + "/inputs/" + this->id + "/Solo/value/ " + value;
	tcpClientSend(msg.c_str());
}

void Channel::pressPostFader(int state)
{
	if (state == SWITCH)
		this->post_fader = !this->post_fader;
	else
		this->post_fader = (bool)state;

	string value = "true";
	if (!this->post_fader)
		value = "false";

	string msg = "set /devices/" + this->device->id + "/auxs/" + this->id + "/SendPostFader/value/ " + value;
	tcpClientSend(msg.c_str());
}

void Channel::updateProperties() {
	string name = this->name;
	if (this->stereo && this->type == INPUT) {
		name = this->stereoname;
	}

	string newProperties;
	size_t pos = name.find_last_of("?");
	if (pos != string::npos) {
		newProperties = name.substr(pos + 1);
	}
	else {
		newProperties = "";
	}
	if (newProperties != this->properties) {
		this->properties = newProperties;
		if (this->isOverriddenShow()) {
			this->selected_to_show = true;
		}
		else if (this->isOverriddenHide()) {
			this->selected_to_show = false;
		}
		pushEvent(EVENT_AFTER_UPDATE_PROPERTIES);
	}
}

string Channel::getName() {
	string name = this->name;
	if (this->stereo && this->type == INPUT) {
		name = this->stereoname;
	}

	size_t pos = name.find_last_of("?");
	if (pos != string::npos) {
		name = name.substr(0, pos);
	}
	return name;
}

void Channel::setName(const string &name) {
	this->name = name;
	this->updateProperties();

	if (this->name.length() == 0) {
		int debug = 1;
	}
}

void Channel::setStereoname(const string &stereoname) {
	this->stereoname = stereoname;
	this->updateProperties();
}

void Channel::setStereo(bool stereo) {
	this->stereo = stereo;
	this->updateProperties();
}

void Channel::getColoredGfx(GFXSurface **gsLabel, GFXSurface** gsFader) {
	if (!gsLabel || !gsFader) {
		return;
	}

	if (this->type == AUX) {
		*gsLabel = g_gsLabelBlue[this->label_gfx];
		*gsFader = g_gsFaderBlue;
	}
	else if (this->type == MASTER) {
		*gsLabel = g_gsLabelRed[this->label_gfx];
		*gsFader = g_gsFaderRed;
	}
	else {
		if (properties.length() > 0) {
			if (properties.find("r") != string::npos || properties.find("R") != string::npos) {
				*gsLabel = g_gsLabelRed[this->label_gfx];
				*gsFader = g_gsFaderRed;
			}
			else if (properties.find("g") != string::npos || properties.find("G") != string::npos) {
				*gsLabel = g_gsLabelGreen[this->label_gfx];
				*gsFader = g_gsFaderGreen;
			}
			else if (properties.find("bl") != string::npos || properties.find("BL") != string::npos) {
				*gsLabel = g_gsLabelBlack[this->label_gfx];
				*gsFader = g_gsFaderBlack;
			}
			else if (properties.find("b") != string::npos || properties.find("B") != string::npos) {
				*gsLabel = g_gsLabelBlue[this->label_gfx];
				*gsFader = g_gsFaderBlue;
			}
			else if (properties.find("y") != string::npos || properties.find("Y") != string::npos) {
				*gsLabel = g_gsLabelYellow[this->label_gfx];
				*gsFader = g_gsFaderYellow;
			}
			else if (properties.find("p") != string::npos || properties.find("P") != string::npos) {
				*gsLabel = g_gsLabelPurple[this->label_gfx];
				*gsFader = g_gsFaderPurple;
			}
			else if (properties.find("o") != string::npos || properties.find("O") != string::npos) {
				*gsLabel = g_gsLabelOrange[this->label_gfx];
				*gsFader = g_gsFaderOrange;
			}
		}
		else {
			*gsLabel = g_gsLabelWhite[this->label_gfx];
			*gsFader = g_gsFaderWhite;
		}
	}
}

bool Channel::isOverriddenShow() {
	return (properties.length() > 0 && (properties.find("s") != string::npos || properties.find("S") != string::npos));
}

bool Channel::isOverriddenHide() {
	return (properties.length() > 0 && (properties.find("h") != string::npos || properties.find("H") != string::npos));
}

void Channel::updateSubscription(bool subscribe, int flags)
{
	if (g_btnMix->isHighlighted() || (flags & ALL_MIXES)) {

		string type_str;
		if (this->type == INPUT) {
			type_str = "/inputs/";
		}
		else if (this->type == AUX) {
			type_str = "/auxs/";
		}
		else if (this->type == MASTER) {
			type_str = "/outputs/";
		}

		string root = "";
		if (!subscribe) {
			root += "un";
		}
		root += "subscribe /devices/" + this->device->id + type_str + this->id;

		if ((flags & NAME) && ((bool)(this->subscriptions & NAME) != subscribe)) {
			string cmd = root + "/Name/";
			tcpClientSend(cmd.c_str());

			if (this->type == INPUT) {
				cmd = root + "/StereoName/";
				tcpClientSend(cmd.c_str());

				this->subscriptions ^= NAME;
			}
		}

		if ((flags & STEREO) && ((bool)(this->subscriptions & STEREO) != subscribe)) {
			if (this->type == INPUT) {
				string cmd = root + "/Stereo/";
				tcpClientSend(cmd.c_str());

				this->subscriptions ^= STEREO;
			}
		}

		if ((flags & MUTE) && ((bool)(this->subscriptions & MUTE) != subscribe)) {
			string cmd = root + "/Mute/";
			tcpClientSend(cmd.c_str());

			this->subscriptions ^= MUTE;
		}

		if ((flags & STATE) && ((bool)(this->subscriptions & STATE) != subscribe)) {
			if (this->type == INPUT) {
				string cmd = root + "/ChannelHidden/";
				tcpClientSend(cmd.c_str());

				cmd = root + "/EnabledByUser/";
				tcpClientSend(cmd.c_str());

				cmd = root + "/Active/";
				tcpClientSend(cmd.c_str());

				this->subscriptions ^= STATE;
			}
		}

		if ((flags & LEVEL) && ((bool)(this->subscriptions & LEVEL) != subscribe)) {
			if (this->type == MASTER) {
				string cmd = root + "/CRMonitorLevel/";
				tcpClientSend(cmd.c_str());
			}
			else {
				string cmd = root + "/FaderLevel/";
				tcpClientSend(cmd.c_str());
			}
			this->subscriptions ^= LEVEL;
		}

		if ((flags & SOLO) && ((bool)(this->subscriptions & SOLO) != subscribe)) {
			if (this->type == INPUT) {
				string cmd = root + "/Solo/";
				tcpClientSend(cmd.c_str());

				this->subscriptions ^= SOLO;
			}
		}

		if ((flags & SEND_POST) && ((bool)(this->subscriptions & SEND_POST) != subscribe)) {
			if (this->type == AUX) {
				string cmd = root + "/SendPostFader/";
				tcpClientSend(cmd.c_str());

				this->subscriptions ^= SEND_POST;
			}
		}

		if ((flags & PAN) && ((bool)(this->subscriptions & PAN) != subscribe)) {
			if (this->type == INPUT) {
				string cmd = root + "/Pan/";
				tcpClientSend(cmd.c_str());

				cmd = root + "/Pan2/";
				tcpClientSend(cmd.c_str());

				this->subscriptions ^= PAN;
			}
		}

		if ((flags & METER) && ((bool)(this->subscriptions & METER) != subscribe)) {
			string cmd = root + "/meters/0/MeterLevel/";
			tcpClientSend(cmd.c_str());

			cmd = root + "/meters/0/MeterClip/";
			tcpClientSend(cmd.c_str());

			cmd = root + "/meters/1/MeterLevel/";
			tcpClientSend(cmd.c_str());

			cmd = root + "/meters/1/MeterClip/";
			tcpClientSend(cmd.c_str());

			this->subscriptions ^= METER;
		}
	}

	if (this->type != MASTER) {
		for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
			if (itB->second->isHighlighted() || (flags & ALL_MIXES)) {
				unordered_map<string, Send*>::iterator itS = this->sendsByName.find((*itB).first);
				if (itS != this->sendsByName.end()) {
					bool subscribeSend = subscribe && (*itB).second->isHighlighted();
					itS->second->updateSubscription(subscribeSend, flags);
				}
			}
		}
	}
}

UADevice::UADevice(string us_deviceId) {
	this->online = false;
	this->id = us_deviceId;
	this->channelsTotal = 0;

	string msg = "subscribe /devices/" + this->id + "/DeviceOnline/";
	tcpClientSend(msg.c_str());
}
UADevice::~UADevice() {
}

int getActiveChannelsCount(bool onlyVisible)
{
	if (g_channelsInOrder.empty()) {
		return 0;
	}

	if ((g_activeChannelsCount == -1 && !onlyVisible) || (g_visibleChannelsCount == -1 && onlyVisible)) {
		g_activeChannelsCount = 0;
		g_visibleChannelsCount = 0;

		bool btn_select = g_btnSelectChannels->isHighlighted();

		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
			if (!it->second->enabledByUser || !it->second->active || it->second->hidden)
				continue;
			if (it->second->isVisible(!btn_select))
				g_visibleChannelsCount++;
			g_activeChannelsCount++;
		}
	}

	if (onlyVisible) {
		return g_visibleChannelsCount;
	}

	return g_activeChannelsCount;
}

void clearChannels() {

	for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
		it->second->updateSubscription(false, ALL|ALL_MIXES);
		SAFE_DELETE(it->second);
	}
	g_channelsById.clear();
	g_channelsInOrder.clear();
	g_touchpointChannels.clear();
	g_masterChannelId = "";
}

void updateSubscriptions() {

	if (!isLoading()) {
		int count = 0;
		bool btn_select = g_btnSelectChannels->isHighlighted();

		for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
			bool subscribe = false;
			if (it->second->isVisible(!btn_select)) {
				if (count >= g_page * g_channels_per_page && count < (g_page + 1) * g_channels_per_page) {
					subscribe = true;
				}
				count++;
			}
			it->second->updateSubscription(subscribe, LEVEL | METER | PAN | SOLO | SEND_POST | (subscribe ? 0 : ALL_MIXES));
		}

		// dragged channel on reorder pageflip
		for (set<Channel*>::iterator it = g_touchpointChannels.begin(); it != g_touchpointChannels.end(); ++it) {
			if (!(*it)->isVisible(!btn_select))
				continue;

			if ((*it)->touch_point.action == TOUCH_ACTION_REORDER) {
				(*it)->updateSubscription(true, LEVEL | METER | PAN | SOLO);
			}
		}
	}
}

UADevice *getDeviceByUAId(string ua_device_id)
{
	if (ua_device_id.length() == 0)
		return NULL;

	for (size_t n = 0; n < g_ua_devices.size(); n++)
	{
		if (g_ua_devices[n]->id == ua_device_id)
		{
			return g_ua_devices[n];
		}
	}
	return NULL;
}

Send *Channel::getSendByName(string name)
{
	unordered_map<string, Send*>::iterator res = this->sendsByName.find(name);

	if (res != this->sendsByName.end()) {
		return res->second;
	}

	return NULL;
}

Send* Channel::getSendByUAId(string id)
{
	unordered_map<string, Send*>::iterator res = this->sendsById.find(id);

	if (res != this->sendsById.end()) {
		return res->second;
	}

	return NULL;
}

Channel *getChannelByUAIds(string ua_device_id, string ua_channel_id, int type)
{
	string type_str;
	if (type == INPUT) {
		type_str = ".input.";
	}
	else if (type == AUX) {
		type_str = ".aux.";
	}
	else if (type == MASTER) {
		type_str = ".master.";
	}

	unordered_map<string, Channel*>::iterator res = g_channelsById.find(ua_device_id + type_str + ua_channel_id);
	
	if (res != g_channelsById.end()) {
		return res->second;
	}

	return NULL;
}

Channel* getChannelByTouchpointId(bool _is_mouse, SDL_TouchFingerEvent *touch_input)
{
	if (!touch_input && !_is_mouse)
		return NULL;

	bool btn_select = g_btnSelectChannels->isHighlighted();

	int index = 0;
	for (set<Channel*>::iterator it = g_touchpointChannels.begin(); it != g_touchpointChannels.end(); ++it) {
		if (!(*it)->isVisible(!btn_select))
			continue;

		if ((touch_input && (*it)->touch_point.id == touch_input->touchId
			&& (*it)->touch_point.finger_id == touch_input->fingerId)
			|| (_is_mouse && (*it)->touch_point.is_mouse))
		{
			return (*it);
		}
		index++;
	}

	return NULL;
}

Channel* getChannelByPosition(Vector2D pt) // client-position; e.g. touch / mouseclick
{
	//offset
	pt.subtractX(g_channel_offset_x);
	pt.subtractY(g_channel_offset_y);

	if (pt.getY() >= 0 && pt.getY() < g_channel_height)
	{
		int channelIndex = (int)floor(pt.getX() / g_channel_width);
		bool btn_select = g_btnSelectChannels->isHighlighted();

		if (channelIndex >= 0 && channelIndex < g_channels_per_page)
		{
			channelIndex += g_page * g_channels_per_page;
			Channel* channel = NULL;
			int visibleChannelsCount = getActiveChannelsCount(true);
			if (channelIndex < visibleChannelsCount)
			{
				int n = 0;
				for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {

					channel = it->second;
					if (!it->second->isVisible(!btn_select))
					{
						channelIndex++;
					}
					if (n >= channelIndex || channelIndex >= (int)g_channelsInOrder.size()) {
						break;
					}
					n++;
				}
			}
			return channel;
		}
	}

	return NULL;
}

static int pingServer(void *param)
{
	if (!param)
		return 0;

	string ip ((char*)param);

	try
	{
		TCPClient* tcpClient = new TCPClient(ip, UA_TCP_PORT, NULL);

		string computername = GetComputerNameByIP(ip);

		bool exists = false;
		for (size_t n = 0; n < g_ua_server_list.size(); n++)
		{
			if (g_ua_server_list[n] == computername)
			{
				exists = true;
				break;
			}
		}

		if (!exists)
		{
			g_ua_server_list.push_back(computername);

			updateConnectButtons();

			writeLog(LOG_INFO, "UA-Server found on " + g_ua_server_list.back());
		}

		SAFE_DELETE(tcpClient);
	}
	catch (string error) {
		writeLog(LOG_INFO| LOG_EXTENDED, "No UA-Server found on " + ip);
	}
	free(param);

	return 0;
}

void getServerListCallback(string ip) {
	if (ip.substr(0, 4) == "127.") {
		//check localhost
		char *scanIpBuffer = (char*)malloc(sizeof(char) * 16);
		if (scanIpBuffer) {
			ip.copy(scanIpBuffer, ip.length());
			scanIpBuffer[ip.length()] = '\0';
			string threadName = "PingUAServer on " + ip;
			SDL_Thread *thread = SDL_CreateThread(pingServer, threadName.c_str(), (void*)scanIpBuffer);
			if (!thread) {
				writeLog(LOG_ERROR, "Error in UA_GetServerListCallback at SDL_CreateThread " + ip);
			}
			else {
				pingThreadsList.push_back(thread);
			}
		}
		else {
			writeLog(LOG_ERROR, "Error in UA_GetServerListCallback at SDL_CreateThread " + ip + "(out of memory)");
		}
	}
	else {
		size_t cut = ip.find_last_of('.');
		string ipFragment = ip.substr(0, cut + 1);

		for (int n = 0; n < 256; n++) {
			ip = ipFragment + to_string(n);
			char* scanIpBuffer = (char*)malloc(sizeof(char) * 16);
			if (scanIpBuffer) {
				ip.copy(scanIpBuffer, ip.length());
				scanIpBuffer[ip.length()] = '\0';
				string threadName = "PingUAServer on " + ip;
				SDL_Thread *thread = SDL_CreateThread(pingServer, threadName.c_str(), scanIpBuffer);
				if (!thread) {
					writeLog(LOG_ERROR, "Error in UA_GetServerListCallback at SDL_CreateThread " + ip);
				}
				else {
					pingThreadsList.push_back(thread);
				}
			}
			else {
				writeLog(LOG_ERROR, "Error in UA_GetServerListCallback at SDL_CreateThread " + ip + "(out of memory)");
			}
		}
	}
}

void terminateAllPingThreads() {
	for (vector<SDL_Thread*>::iterator it = pingThreadsList.begin(); it != pingThreadsList.end(); ++it) {
		SDL_DetachThread(*it);
	}
	pingThreadsList.clear();
}


void cleanUpConnectionButtons()
{
	for (size_t n = 0; n < g_btnConnect.size(); n++) {
		SAFE_DELETE(g_btnConnect[n]);
	}
	g_btnConnect.clear();
}

void cleanUpSendButtons() {
	for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		SAFE_DELETE(it->second);
	}
	g_btnSends.clear();
}

void setRedrawWindow(bool redraw)
{
	g_redraw = redraw;
}

bool getRedrawWindow()
{
	return g_redraw;
}

Uint32 timerCallbackSelectAllChannels(Uint32 interval, void* param) {
	if (g_selectAllChannelsCountdown == 5) { // select all
		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
			it->second->selected_to_show = true;
		}
	}
	else if (g_selectAllChannelsCountdown == 1) { // select none
		g_selectAllChannelsCountdown = 0;
		SDL_RemoveTimer(g_timerSelectAllChannels);
		g_timerSelectAllChannels = 0;

		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
			it->second->selected_to_show = false;
		}

		setRedrawWindow(true);
		return 0;
	}
	g_selectAllChannelsCountdown--;
	setRedrawWindow(true);
	return interval;
}

Uint32 timerCallbackUnmuteAll(Uint32 interval, void* param) {
	if (g_unmuteAllCountdown == 1) {
		g_unmuteAllCountdown = 0;
		SDL_RemoveTimer(g_timerUnmuteAll);
		g_timerUnmuteAll = 0;

		muteChannels(false, true);
		
		setRedrawWindow(true);

		return 0;
	}
	g_unmuteAllCountdown--;
	setRedrawWindow(true);
	return interval;
}

Uint32 timerCallbackResetOrder(Uint32 interval, void* param) {
	if (g_resetOrderCountdown == 1) {
		g_resetOrderCountdown = 0;
		SDL_RemoveTimer(g_timerResetOrder);
		g_timerResetOrder = 0;
		pushEvent(EVENT_RESET_ORDER);
		return 0;
	}
	g_resetOrderCountdown--;
	setRedrawWindow(true);
	return interval;
}

Uint32 timerCallbackFlipPage(Uint32 interval, void* param) {

	Channel* channel = (Channel*)param;
	if (channel->touch_point.pt_end_x < g_channel_offset_x) {
		if (pageLeft()) {
			g_dragPageFlip--;
		}
		return 1200;
	}
	else if (channel->touch_point.pt_end_x > g_channel_offset_x + (float)g_channels_per_page * g_channel_width) {
		if (pageRight()) {
			g_dragPageFlip++;
		}
		return 1200;
	}
	SDL_RemoveTimer(g_timerFlipPage);
	g_timerFlipPage = 0;
	return 0;
}

Uint32 timerCallbackRefreshServerList(Uint32 interval, void *param) //g_timer_network_serverlist
{
	if (!IS_UA_SERVER_REFRESHING)
	{
		setRedrawWindow(true);
		SDL_RemoveTimer(g_timer_network_serverlist);
		g_timer_network_serverlist = 0;
		return 0;
	}

	setRedrawWindow(true);
	return interval;
}

Uint32 timerCallbackReconnect(Uint32 interval, void* param) //g_timer_network_reconnect
{
	if (g_ua_server_connected.length() == 0 && g_ua_server_last_connection != -1) {
		pushEvent(EVENT_CONNECT);
	}
	else {
		SDL_RemoveTimer(g_timer_network_reconnect);
		g_timer_network_reconnect = 0;
		return 0;
	}
	
	return interval;
}

Uint32 timerCallbackNetworkTimeout(Uint32 interval, void* param) //g_timer_network_timeout
{
	g_msg = "Network timeout";
	writeLog(LOG_INFO, g_msg);
	
	pushEvent(EVENT_DISCONNECT);

	setRedrawWindow(true);
	SDL_RemoveTimer(g_timer_network_timeout);
	g_timer_network_timeout = 0;

	// try to reconnect
	if (g_settings.reconnect_time && g_timer_network_reconnect == 0) {
		g_timer_network_reconnect = SDL_AddTimer(g_settings.reconnect_time, timerCallbackReconnect, NULL);
	}

	return 0;
}

Uint32 timerCallbackNetworkSendTimeoutMsg(Uint32 interval, void* param) //g_timer_network_timeout
{
	tcpClientSend("get /devices/0/Name/"); // just ask for sth to check if the connection is alive
	SDL_RemoveTimer(g_timer_network_send_timeout_msg);
	g_timer_network_send_timeout_msg = 0;

	return 0;
}

void setNetworkTimeout() {
#ifndef SIMULATION
	if (g_timer_network_send_timeout_msg != 0) {
		SDL_RemoveTimer(g_timer_network_send_timeout_msg);
	}
	g_timer_network_send_timeout_msg = SDL_AddTimer(4000, timerCallbackNetworkSendTimeoutMsg, NULL);

	if (g_timer_network_timeout != 0) {
		SDL_RemoveTimer(g_timer_network_timeout);
	}
	g_timer_network_timeout = SDL_AddTimer(10000, timerCallbackNetworkTimeout, NULL);
#endif
}

#ifdef SIMULATION
bool getServerList()
{
	g_ua_server_last_connection = -1;

	cleanUpConnectionButtons();

	setRedrawWindow(true);

	g_ua_server_list.clear();
	g_ua_server_list.push_back("Simulation");

	updateConnectButtons();

	return true;
}
#endif

#ifndef SIMULATION
bool getServerList()
{
	if (g_serverlist_defined)
		return false;

	if (IS_UA_SERVER_REFRESHING)
		return false;

	writeLog(LOG_INFO, "begin updating serverlist");

	terminateAllPingThreads();

	g_ua_server_last_connection = -1;
	g_ua_server_list.clear();

	updateConnectButtons();

	g_server_refresh_start_time = GetTickCount64(); //10sec

	if(g_timer_network_serverlist == 0)
		g_timer_network_serverlist = SDL_AddTimer(1000, timerCallbackRefreshServerList, NULL);

	setRedrawWindow(true);

	if (!GetClientIPs(getServerListCallback))
		return false;

	return true;
}
#endif

void tcpClientProc(int msg, const string &data)
{
	if (!g_tcpClient)
		return;

	switch (msg)
	{
	case MSG_CLIENT_CONNECTED:
		break;
	case MSG_CLIENT_DISCONNECTED:
		break;
	case MSG_CLIENT_CONNECTION_LOST:
	{
		// try to reconnect
		if (g_settings.reconnect_time && g_timer_network_reconnect == 0) {
			g_timer_network_reconnect = SDL_AddTimer(g_settings.reconnect_time, timerCallbackReconnect, NULL);
		}

		//send message disconnect
		pushEvent(EVENT_DISCONNECT);
		break;
	}
	case MSG_TEXT:
	{
		setNetworkTimeout();

		if (g_settings.extended_logging) {
			string tcp_msg = data;
			writeLog(LOG_INFO | LOG_EXTENDED, "UA <- " + json_workaround_secure_unicode_characters(tcp_msg));
		}
		
		string path_parameter[12];

		try
		{
			dom::parser parser;
			padded_string json = padded_string(data);

			//path
			dom::element element = parser.parse(json); // parse a string
			string_view sv = element["path"];
			string path{ sv };

			//path aufsplitten
			size_t i = 0;
			size_t lpos = 1;
			do
			{
				size_t rpos = path.find_first_of('/', lpos);
				if (rpos != string::npos)
				{
					path_parameter[i] = path.substr(lpos, rpos - lpos);
					i++;
				}
				else
				{
					path_parameter[i] = path.substr(lpos);
					break;
				}
				lpos = rpos + 1;
			} while (lpos != string::npos && i < 12);

			//daten anhand path_parameter verarbeiten
			if (path_parameter[0] == "Session" || path_parameter[0] == "IOMapPreset") {
				pushEvent(EVENT_DEVICES_INITIATE_RELOAD);
			}
			else if (path_parameter[0] == "PostFaderMetering") {
				pushEvent(EVENT_POST_FADER_METERING, (void*)(bool)element["data"]);
			}
			else if (path_parameter[0] == "devices") // devices laden
			{
				if (path_parameter[1].length() == 0)
				{
					const dom::object obj = element["data"]["children"];
					vector<string>* strDevices = new vector<string>();
					for (dom::object::iterator it = obj.begin(); it != obj.end(); ++it) {
						string id{ it.key() };
						strDevices->push_back(id);
					}
					pushEvent(EVENT_DEVICES_LOAD, (void*)strDevices);
				}
				else if (path_parameter[2] == "DeviceOnline") {
					pushEvent(EVENT_DEVICE_ONLINE, (void*)new string(path_parameter[1]), (void*)(bool)element["data"]);
				}
				else if (path_parameter[2] == "CueBusCount") {
					int *cueBusCount = new int;
					*cueBusCount = (int)((int64_t)element["data"]);
					pushEvent(EVENT_SENDS_LOAD, (void*)cueBusCount);
				}
				else if (path_parameter[2] == "inputs" || path_parameter[2] == "auxs" || path_parameter[2] == "outputs")
				{
					if (path_parameter[3].length() == 0) {

						const dom::object obj = element["data"]["children"];

						string* pType = new string(path_parameter[2]);
						vector<string>* pIds = new vector<string>();
						for (dom::object::iterator it = obj.begin(); it != obj.end(); ++it) {
							string id{ it.key() };
							pIds->push_back(id);
						}

						string* devStr = new string(path_parameter[1]);
						if (path_parameter[2] == "inputs") {
							pushEvent(EVENT_INPUTS_LOAD, (void*)pIds, (void*)devStr);
						}
						else if (path_parameter[2] == "auxs") {
							pushEvent(EVENT_AUXS_LOAD, (void*)pIds, (void*)devStr);
						}
						else if (path_parameter[2] == "outputs") {
							pushEvent(EVENT_OUTPUTS_LOAD, (void*)pIds, (void*)devStr);
						}
					}
					else {

						int type = 0;
						if (path_parameter[2] == "inputs") {
							type = INPUT;
						}
						else if (path_parameter[2] == "auxs") {
							type = AUX;
						}
						else if (path_parameter[2] == "outputs") {
							type = MASTER;
						}
						Channel* channel = getChannelByUAIds(path_parameter[1], path_parameter[3], type);
						if (channel)
						{
							if (path_parameter[4] == "sends")//input sends
							{
								if (path_parameter[5].length() != 0)
								{
									Send* send = channel->getSendByUAId(path_parameter[5]);
									if (send)
									{
										if (path_parameter[6] == "meters")
										{
											if (path_parameter[7] == "0" && path_parameter[8] == "MeterLevel" && path_parameter[9] == "value")
											{
												double db = element["data"];
												double prev_meter_level = send->meter_level;
												send->meter_level = min(1.0, fromDbFS(db));
												if (db >= METER_THRESHOLD &&
													((int)(toMeterScale(prev_meter_level) * UA_METER_PRECISION)) != ((int)(toMeterScale(send->meter_level) * UA_METER_PRECISION))) {
													setRedrawWindow(true);
												}
											}

											if (path_parameter[7] == "0" && path_parameter[8] == "MeterClip" && path_parameter[9] == "value")
											{
												send->clip = element["data"];
												setRedrawWindow(true);
											}

											if (path_parameter[7] == "1" && path_parameter[8] == "MeterLevel" && path_parameter[9] == "value")
											{
												double db = element["data"];
												double prev_meter_level2 = send->meter_level2;
												send->meter_level2 = min(1.0, fromDbFS(db));
												if (db >= METER_THRESHOLD &&
													((int)(toMeterScale(prev_meter_level2) * UA_METER_PRECISION)) != ((int)(toMeterScale(send->meter_level2) * UA_METER_PRECISION))) {
													setRedrawWindow(true);
												}
											}

											if (path_parameter[7] == "1" && path_parameter[8] == "MeterClip" && path_parameter[9] == "value")
											{
												send->clip2 = element["data"];
												setRedrawWindow(true);
											}
										}
										else if (path_parameter[6] == "Gain" && channel->touch_point.action != TOUCH_ACTION_LEVEL)//sends
										{
											if (path_parameter[7] == "value")
											{
												send->level = fromDbFS(element["data"]);
												setRedrawWindow(true);
											}
										}
										else if (path_parameter[6] == "Pan" && channel->touch_point.action != TOUCH_ACTION_PAN)//sends
										{
											if (path_parameter[7] == "value")
											{
												send->pan = element["data"];
												setRedrawWindow(true);
											}
										}
										else if (path_parameter[6] == "Bypass")//sends
										{
											if (path_parameter[7] == "value")
											{
												send->mute = element["data"];
												setRedrawWindow(true);
											}
										}
									}
								}
							}
							else if (path_parameter[4] == "meters")
							{
								if (path_parameter[5] == "0" && path_parameter[6] == "MeterLevel" && path_parameter[7] == "value")
								{
									double db = element["data"];
									double prev_meter_level = channel->meter_level;
									channel->meter_level = min(1.0, fromDbFS(db));
									if (db >= METER_THRESHOLD &&
										((int)(toMeterScale(prev_meter_level) * UA_METER_PRECISION)) != ((int)(toMeterScale(channel->meter_level) * UA_METER_PRECISION))) {
										setRedrawWindow(true);
									}
								}
								if (path_parameter[5] == "0" && path_parameter[6] == "MeterClip" && path_parameter[7] == "value")
								{
									channel->clip = element["data"];
									setRedrawWindow(true);
								}
								if (path_parameter[5] == "1" && path_parameter[6] == "MeterLevel" && path_parameter[7] == "value")
								{
									double db = element["data"];
									double prev_meter_level2 = channel->meter_level2;
									channel->meter_level2 = min(1.0, fromDbFS(db));
									if (db >= METER_THRESHOLD &&
										((int)(toMeterScale(prev_meter_level2) * UA_METER_PRECISION)) != ((int)(toMeterScale(channel->meter_level2) * UA_METER_PRECISION))) {
										setRedrawWindow(true);
									}
								}
								if (path_parameter[5] == "1" && path_parameter[6] == "MeterClip" && path_parameter[7] == "value")
								{
									channel->clip2 = element["data"];
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "FaderLevel" && channel->touch_point.action != TOUCH_ACTION_LEVEL)
							{
								if (path_parameter[5] == "value")
								{
									channel->level = fromDbFS((double)element["data"]);
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "CRMonitorLevel" && channel->touch_point.action != TOUCH_ACTION_LEVEL)
							{
								if (path_parameter[5] == "value")
								{
									double dBlevel = (double)element["data"];
									if (dBlevel > -96.0) {
										channel->level = fromDbFS(dBlevel);
									}
									else {
										channel->level = fromDbFS(-144.0);
									}
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Name") {
								if (path_parameter[5] == "value") {

									writeLog(LOG_INFO, "setname " + path_parameter[3]);
									string_view sv = element["data"];
									string value{ sv };
									channel->setName(unescape_to_utf8(value));
									writeLog(LOG_INFO, "->ok\n");
								}
							}
							else if (path_parameter[4] == "Pan" && channel->touch_point.action != TOUCH_ACTION_PAN)
							{
								if (path_parameter[5] == "value")
								{
									channel->pan = element["data"];
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Pan2" && channel->touch_point.action != TOUCH_ACTION_PAN2)
							{
								if (path_parameter[5] == "value")
								{
									channel->pan2 = element["data"];
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Solo")
							{
								if (path_parameter[5] == "value")
								{
									channel->solo = element["data"];
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "SendPostFader")
							{
								if (path_parameter[5] == "value")
								{
									channel->post_fader = element["data"];
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Mute")
							{
								if (path_parameter[5] == "value")
								{
									channel->mute = element["data"];
									if (channel->type == AUX || channel->type == MASTER) {
										if (channel->type == MASTER) {
											setLoadingState(false);
										}
										channel->active = true;
										pushEvent(EVENT_CHANNEL_STATE_CHANGED);
									}
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Stereo") {
								if (path_parameter[5] == "value") {
									g_activeChannelsCount = -1;
									g_visibleChannelsCount = -1;
									channel->setStereo(element["data"]);
								}
							}
							else if (path_parameter[4] == "StereoName") {
								if (path_parameter[5] == "value") {
									string_view sv = element["data"];
									string value{ sv };
									channel->setStereoname(unescape_to_utf8(value));
								}
							}
							else if (path_parameter[4] == "ChannelHidden")
							{
								if (path_parameter[5] == "value")
								{
									channel->hidden = element["data"];
									pushEvent(EVENT_CHANNEL_STATE_CHANGED);
								}
							}
							else if (path_parameter[4] == "EnabledByUser")
							{
								if (path_parameter[5] == "value")
								{
									channel->enabledByUser = element["data"];
									pushEvent(EVENT_CHANNEL_STATE_CHANGED);
								}
							}
							else if (path_parameter[4] == "Active")
							{
								if (path_parameter[5] == "value")
								{
									channel->active = element["data"];
									pushEvent(EVENT_CHANNEL_STATE_CHANGED);
								}
							}
						}
						else {
							writeLog(LOG_ERROR | LOG_EXTENDED, "Channel was NULL");
						}
					}
				}
			}

			try {
				//ua-error
				const char *c;
				if (element["error"].get_c_str().get(c) == SUCCESS) {
					string s(c);
					writeLog(LOG_ERROR| LOG_EXTENDED, "UA-Error " + s);
				}
			}
			catch (simdjson_error&) {}
		}
		catch (simdjson_error &e) {
			writeLog(LOG_ERROR| LOG_EXTENDED, "JSON-Error: " + string(e.what()));
		}
		break;
	}
	}
}

void tcpClientSend(const string &msg) {
	if (g_tcpClient) {
		writeLog(LOG_INFO| LOG_EXTENDED, "UA -> " + msg);
		g_tcpClient->Send(msg);
	}
}

void drawScaleMark(float x, float y, int scale_value, double total_scale) {
	Vector2D sz = gfx->GetTextBlockSize(g_fntFaderScale, "-1234567890");
	double v = fromDbFS((double)scale_value);
	y = y - (float)(toMeterScale(v) * total_scale) + (float)total_scale;

	gfx->DrawShape(GFX_RECTANGLE, RGB(170, 170, 170), x + g_channel_width * 0.25f, y, g_channel_width * 0.1f, 1.0f, GFX_NONE, 0.8f);
	gfx->Write(g_fntFaderScale, x + g_channel_width * 0.24f, y - sz.getY() * 0.5f, to_string(scale_value), GFX_RIGHT, NULL, GFX_NONE, 0.8f);
}

Module* Channel::getModule(const string &mixBus) {
	if (mixBus == "MIX") {
		return (Module*)this;
	}
	else {
		return this->getSendByName(mixBus);
	}
	return NULL;
}

void Channel::draw(float _x, float _y, float _width, float _height) {

	float x = _x;
	float y = _y;
	float width = _width;
	float height = _height;

	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	Vector2D sz;
	Vector2D stretch;

	Module* mod = this->getModule(g_selectedMixBus);
	string name = this->getName();

	if (this->type == AUX) {
		if (name == "AUX 1") {
			name = g_settings.label_aux1;
			if (g_settings.label_aux1 == g_settings.label_aux2) {
				name += " 1";
			}
		}
		else if (name == "AUX 2") {
			name = g_settings.label_aux2;
			if (g_settings.label_aux1 == g_settings.label_aux2) {
				name += " 2";
			}
		}
	}

	bool gray = false;
	bool highlight = false;

	bool btn_select = g_btnSelectChannels->isHighlighted();

	//SELECT CHANNELS
	if (btn_select) {
		if (!this->selected_to_show) {
			gray = true;
		}
		else {
			highlight = true;
		}
	}

	if(highlight) {
		unsigned int clr = RGB(200, 200, 50);

		Vector2D pt = {_x - 1, _y};
		Channel *prev_channel = getChannelByPosition(pt);
		if (!prev_channel || !prev_channel->selected_to_show) {
			gfx->DrawShape(GFX_RECTANGLE, clr, _x - SPACE_X, 0, SPACE_X, _height * 1.02f, GFX_NONE, 0.5f); // links
		}

		gfx->DrawShape(GFX_RECTANGLE, clr, _x, 0, _width - SPACE_X, SPACE_Y, GFX_NONE, 0.5f); // oben
		gfx->DrawShape(GFX_RECTANGLE, clr, _x, _height * 1.02f - SPACE_Y, _width - SPACE_X, SPACE_Y, GFX_NONE, 0.5f); // unten
		gfx->DrawShape(GFX_RECTANGLE, clr, _x + _width - SPACE_X, 0, SPACE_X, _height * 1.02f, GFX_NONE, 0.5f); // rechts
	}

	GFXSurface *gsLabel = NULL, *gsFader = NULL;
	this->getColoredGfx(&gsLabel, &gsFader);

	if (mod) {
		//LABEL überspringen und am Ende zeichenen (wg. asugrauen)
		y += g_fader_label_height;
		height -= g_fader_label_height;

		if (this->type == INPUT) {
			//PAN
			//tracker
			if (g_btnMix->isHighlighted() || !this->stereo) {
				float pan_width = width;
				if (this->stereo) {
					pan_width /= 2;
					float local_PAN_TRACKER_HEIGHT = pan_width;
					if (local_PAN_TRACKER_HEIGHT > g_pantracker_height)
						local_PAN_TRACKER_HEIGHT = g_pantracker_height;

					stretch = Vector2D(local_PAN_TRACKER_HEIGHT, local_PAN_TRACKER_HEIGHT);
					gfx->Draw(g_gsPan, x + pan_width + (pan_width - local_PAN_TRACKER_HEIGHT) / 2.0f, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2.0f, NULL,
						GFX_NONE, 1.0f, 0, NULL, &stretch);

					gfx->Draw(g_gsPanPointer, x + pan_width + (pan_width - local_PAN_TRACKER_HEIGHT) / 2.0f, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2.0f, NULL,
						GFX_NONE, 1.0f, DEG2RAD((float)mod->pan2 * 140.0f), NULL, &stretch);

					gfx->Draw(g_gsPan, x + (pan_width - local_PAN_TRACKER_HEIGHT) / 2.0f, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2.0f, NULL,
						GFX_NONE, 1.0f, 0, NULL, &stretch);

					gfx->Draw(g_gsPanPointer, x + (pan_width - local_PAN_TRACKER_HEIGHT) / 2.0f, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2.0f, NULL,
						GFX_NONE, 1.0f, DEG2RAD((float)mod->pan * 140.0f), NULL, &stretch);
				}
				else {
					stretch = Vector2D(g_pantracker_height, g_pantracker_height);
					gfx->Draw(g_gsPan, x + (pan_width - g_pantracker_height) / 2.0f, y + (g_channel_pan_height - g_pantracker_height) / 2.0f, NULL,
						GFX_NONE, 1.0f, 0, NULL, &stretch);

					gfx->Draw(g_gsPanPointer, x + (pan_width - g_pantracker_height) / 2.0f, y + (g_channel_pan_height - g_pantracker_height) / 2.0f, NULL,
						GFX_NONE, 1.0f, DEG2RAD((float)mod->pan * 140.0f), NULL, &stretch);
				}

			}
		}

		gfx->SetColor(g_fntMain, RGB(20, 20, 20));
		gfx->SetColor(g_fntFaderScale, RGB(170, 170, 170));
		gfx->SetColor(g_fntChannelBtn, RGB(80, 80, 80));

		y += g_channel_pan_height;
		height -= g_channel_pan_height;

		float x_offset = (width / 2.0f - g_channel_btn_size) / 2.0f;
		//SOLO
		if (g_btnMix->isHighlighted()) {
			int btn_switch = 0;
			if ((this->type == AUX && !this->post_fader) || (this->type != AUX && this->solo)) {
				btn_switch = 1;
			}

			GFXSurface* gsBtn = g_gsButtonRed[btn_switch];
			string txt = "S";
			if (this->type == AUX) {
				gsBtn = g_gsButtonBlue[btn_switch];
				txt = "PRE";
			}

			if (this->type != MASTER) {
				stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
				gfx->Draw(gsBtn, x + x_offset, y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

				sz = gfx->GetTextBlockSize(g_fntChannelBtn, txt, GFX_CENTER);
				gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2, y + (g_channel_btn_size - sz.getY()) / 2, txt, GFX_CENTER);
			}
		}

		//MUTE
		int btn_switch = 0;
		if (mod->mute) {
			btn_switch = 1;
		}

		stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
		gfx->Draw(g_gsButtonYellow[btn_switch], x + x_offset + width / 2, y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

		sz = gfx->GetTextBlockSize(g_fntChannelBtn, "M", GFX_CENTER);
		gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2 + width / 2, y + (g_channel_btn_size - sz.getY()) / 2, "M", GFX_CENTER);

		y += g_channel_btn_size;
		height -= g_channel_btn_size;

		if (this->type != MASTER) {
			//GROUPLABEL
			float group_y = _height - g_channel_btn_size;
			sz = gfx->GetTextBlockSize(g_fntFaderScale, "Group");
			gfx->Write(g_fntFaderScale, x + g_channel_width / 2.0f, group_y - sz.getY() - SPACE_Y * 2.0f, "Group", GFX_CENTER, NULL, GFX_NONE, 0.9f);

			//GROUP1
			btn_switch = 0;
			if (this->fader_group == 1) {
				btn_switch = 1;
			}

			stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
			gfx->Draw(g_gsButtonGreen[btn_switch], x + x_offset, group_y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

			sz = gfx->GetTextBlockSize(g_fntChannelBtn, "1", GFX_CENTER);
			gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2, group_y + (g_channel_btn_size - sz.getY()) / 2, "1", GFX_CENTER);

			//GROUP2
			btn_switch = 0;
			if (this->fader_group == 2) {
				btn_switch = 1;
			}

			stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
			gfx->Draw(g_gsButtonBlue[btn_switch], x + x_offset + width / 2, group_y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

			sz = gfx->GetTextBlockSize(g_fntChannelBtn, "2", GFX_CENTER);
			gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2 + width / 2, group_y + (g_channel_btn_size - sz.getY()) / 2, "2", GFX_CENTER);
		}

		//FADER
		//rail
		height -= g_channel_btn_size + sz.getY() + SPACE_Y; //Fader group unterhalb
		g_faderrail_height = height - g_fadertracker_height;
		if (this->type == MASTER) {
			float shorten = (float)g_faderrail_height - (float)(toMeterScale(fromDbFS(0)) * g_faderrail_height);
			stretch = Vector2D(g_faderrail_width, g_faderrail_height - shorten);
			gfx->Draw(g_gsFaderrail, x + (width - g_faderrail_width) / 2.0f, y + g_fadertracker_height / 2.0f + shorten,
				NULL, GFX_NONE, 1.0f, 0, NULL, &stretch);
		}
		else {
			stretch = Vector2D(g_faderrail_width, g_faderrail_height);
			gfx->Draw(g_gsFaderrail, x + (width - g_faderrail_width) / 2.0f, y + g_fadertracker_height / 2.0f,
				NULL, GFX_NONE, 1.0f, 0, NULL, &stretch);
		}

		float o = g_fadertracker_height / 2.0f;

		if (this->type != MASTER) {
			drawScaleMark(x, y + o, 12, g_faderrail_height);
			drawScaleMark(x, y + o, 6, g_faderrail_height);
		}
		drawScaleMark(x, y + o, 0, g_faderrail_height);
		drawScaleMark(x, y + o, -6, g_faderrail_height);
		drawScaleMark(x, y + o, -12, g_faderrail_height);
		drawScaleMark(x, y + o, -20, g_faderrail_height);
		drawScaleMark(x, y + o, -32, g_faderrail_height);
		drawScaleMark(x, y + o, -52, g_faderrail_height);
		drawScaleMark(x, y + o, -84, g_faderrail_height);

		//tracker
		stretch = Vector2D(g_fadertracker_width, g_fadertracker_height);

		gfx->Draw(gsFader, x + (width - g_fadertracker_width) / 2.0f,
			y + height - (float)toMeterScale(mod->level) * (height - g_fadertracker_height) - g_fadertracker_height, NULL,
			GFX_NONE, 1.0f, 0, NULL, &stretch);

		//Clip LED
		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f - 1.0f,
			y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 10.0f,
			7.0f, 9.0f);

		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BG, x + g_channel_width * 0.75f,
			y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 8.0f,
			5.0f, 7.0f);

		if (mod->clip) {
			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_RED, x + g_channel_width * 0.75f + 1,
				y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 7.0f,
				3.0f, 5.0f);
		}

		if (this->stereo) {
			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f + 5.0f,
				y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 10.0f,
				7.0f, 9.0f);

			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BG, x + g_channel_width * 0.75f + 6.0f,
				y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 8.0f,
				5.0f, 7.0f);

			if (mod->clip2) {
				gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_RED, x + g_channel_width * 0.75f + 7.0f,
					y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 7.0f,
					3.0f, 5.0f);
			}
		}

		//LEVELMETER
		float threshold = g_faderrail_height * (float)toMeterScale(fromDbFS(METER_THRESHOLD));

		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f - 1.0f,
			y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 1.0f,
			7.0f, g_faderrail_height * (float)toMeterScale(UNITY) + 2.0f - threshold);

		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BG, x + g_channel_width * 0.75f,
			y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height,
			5.0f, g_faderrail_height * (float)toMeterScale(UNITY) - threshold);

		if (mod->meter_level > fromDbFS(METER_THRESHOLD)) {
			float amplitude = round(g_faderrail_height * (float)toMeterScale(mod->meter_level));

			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_GREEN, x + g_channel_width * 0.75f + 1.0f,
				y + o + g_faderrail_height - amplitude,
				3.0f, amplitude - threshold);

			if (mod->meter_level > fromDbFS(-9.0)) {
				gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_YELLOW, x + g_channel_width * 0.75f + 1.0f,
					y + o + g_faderrail_height - amplitude,
					3.0f, amplitude - g_faderrail_height * (float)toMeterScale(fromDbFS(-9.0)));
			}
		}

		if (this->stereo) {

			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f + 5.0f,
				y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height - 1.0f,
				7.0f, g_faderrail_height* (float)toMeterScale(UNITY) + 2.0f - threshold);

			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BG, x + g_channel_width * 0.75f + 6.0f,
				y + o - (float)toMeterScale(UNITY) * g_faderrail_height + g_faderrail_height,
				5.0f, g_faderrail_height* (float)toMeterScale(UNITY) - threshold);

			if (mod->meter_level > fromDbFS(METER_THRESHOLD)) {
				float amplitude = round(g_faderrail_height * (float)toMeterScale(mod->meter_level2));

				gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_GREEN, x + g_channel_width * 0.75f + 7.0f,
					y + o + g_faderrail_height - amplitude,
					3.0f, amplitude - threshold);

				if (mod->meter_level2 > fromDbFS(-9.0)) {
					gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_YELLOW, x + g_channel_width * 0.75f + 7.0f,
						y + o + g_faderrail_height - amplitude,
						3.0f, amplitude - g_faderrail_height * (float)toMeterScale(fromDbFS(-9.0)));
				}
			}
		}

		if (gray) {
			gfx->DrawShape(GFX_RECTANGLE, RGB(20, 20, 20), _x, 0, _width, _height * 1.02f, GFX_NONE, 0.7f);
		}

		//selektion gesperrt (serverseitig überschrieben)
		if (btn_select && (this->isOverriddenHide() || this->isOverriddenShow())) {
			gfx->DrawShape(GFX_RECTANGLE, RGB(255, 0, 0), _x, 0, _width, _height * 1.02f, GFX_NONE, 0.1f);

			gfx->SetColor(g_fntMain, WHITE);
			gfx->SetShadow(g_fntMain, BLACK, 1);
			sz = gfx->GetTextBlockSize(g_fntMain, "locked", GFX_CENTER);
			gfx->Write(g_fntMain, _x + _width / 2.0f, 0 + (_height - sz.getY()) / 2.0f, "locked", GFX_CENTER);
			gfx->SetShadow(g_fntMain, 0, 0);
		}
	}

	//LABEL
	stretch = Vector2D(_width * 1.1f, g_fader_label_height);
	gfx->Draw(gsLabel, _x - _width * 0.05f, _y, NULL, GFX_NONE, 1.0f, this->label_rotation, NULL, &stretch);

	Vector2D max_size = Vector2D(_width , g_fader_label_height);

	// remove spaces if name too long
	size_t n = name.length() - 1;
	do {
		sz = gfx->GetTextBlockSize(g_fntLabel, name, GFX_CENTER);
		if (sz.getX() > _width && (name[n] == ' ' || name[n] == '\t')) {
			name = name.substr(0, n) + name.substr(n + 1, name.length() - n + 1);
		}
		n--;
	} while (sz.getX() > _width && n > 0);

	// remove vowels if name too long
	n = name.length() - 1;
	do {
		sz = gfx->GetTextBlockSize(g_fntLabel, name, GFX_CENTER);
		if (sz.getX() > _width && 
			(name[n] == 'a' || name[n] == 'e' || name[n] == 'i' || name[n] == 'o' || name[n] == 'u'
			|| name[n] == 'A' || name[n] == 'E' || name[n] == 'I' || name[n] == 'O' || name[n] == 'U')) {
			name = name.substr(0, n) + name.substr(n + 1, name.length() - n + 1);
		}
		n--;
	} while (sz.getX() > _width && n > 0);

	// if name still too long remove chars from the end
	do {
		sz = gfx->GetTextBlockSize(g_fntLabel, name, GFX_CENTER);
		if (sz.getX() > _width) {
			name = name.substr(0, name.length() - 1);
		}
	} while (sz.getX() > _width);

	if (gsLabel == g_gsLabelBlack[this->label_gfx]) {
		gfx->SetColor(g_fntLabel, WHITE);
	}
	else {
		gfx->SetColor(g_fntLabel, BLACK);
	}

	gfx->Write(g_fntLabel, _x, _y + (g_fader_label_height - sz.getY()) / 2, name, GFX_CENTER, &max_size);
}

int getMiddleSelectedChannel() {

	int index = 0;
	int counter = 0;

	for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
		if (!it->second->isVisible(!g_btnSelectChannels->isHighlighted())) {
			continue;
		}

		if (counter >= g_page * g_channels_per_page + g_channels_per_page / 2) {
			break;
		}
		counter++;
		if (it->second->isVisible(true)) {
			index++;
		}
	}
	return index;
}

void browseToSelectedChannel(int index) {

	if (g_btnSelectChannels->isHighlighted()) {
		int i = index;
		for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
			if (i < 0) {
				break;
			}
			if (it->second->isVisible(true)) {
				i--;
			}
			else if (it->second->isVisible(false)) {
				index++;
			}
		}
	}
	if (index == getActiveChannelsCount(false)) {
		g_page = 0;
	}
	else if (g_channels_per_page) {
		g_page = index / g_channels_per_page;
	}
}

void draw() {
	
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float win_width = (float)w;
	float win_height = (float)h;
	Vector2D sz;
	Vector2D stretch;

	//background
	gfx->DrawShape(GFX_RECTANGLE, BLACK, 0, 0, win_width, win_height);

	//bg right
	stretch = Vector2D(g_channel_offset_x, win_height);
	gfx->Draw(g_gsBgRight, g_channel_offset_x + g_channel_width * (float)g_channels_per_page, 0, NULL,
		GFX_NONE, 1.0, 0, NULL, &stretch);

	//bg left
	stretch = Vector2D(g_channel_offset_x, win_height);
	gfx->Draw(g_gsBgRight, 0, 0, NULL, GFX_HFLIP, 1.0, 0, NULL, &stretch);

	//draw buttons
	g_btnSettings->draw(BTN_COLOR_YELLOW);
	g_btnMuteAll->draw(BTN_COLOR_RED);
	for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		if (it->second->getText() == "AUX 1") {
			string text = g_settings.label_aux1;
			if (g_settings.label_aux1 == g_settings.label_aux2) {
				text += " 1";
			}
			text += "\nSend";
			it->second->draw(BTN_COLOR_BLUE, text);
		}
		else if (it->second->getText() == "AUX 2") {
			string text = g_settings.label_aux2;
			if (g_settings.label_aux1 == g_settings.label_aux2) {
				text += " 2";
			}
			text += "\nSend";
			it->second->draw(BTN_COLOR_BLUE, text);
		}
		else {
			it->second->draw(BTN_COLOR_BLUE);
		}
	}
	g_btnMix->draw(BTN_COLOR_BLUE);

	for (size_t n = 0; n < g_btnConnect.size(); n++) {
		g_btnConnect[n]->draw(BTN_COLOR_GREEN);
	}
	g_btnChannelWidth->draw(BTN_COLOR_YELLOW);
	g_btnPageLeft->draw(BTN_COLOR_YELLOW);
	g_btnPageRight->draw(BTN_COLOR_YELLOW);
	g_btnSelectChannels->draw(BTN_COLOR_YELLOW);
	g_btnReorderChannels->draw(BTN_COLOR_YELLOW);


	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	string str_page = "Page: " + to_string(g_page +1);

	gfx->SetColor(g_fntMain, RGB(200, 200, 200));
	gfx->Write(g_fntMain, g_btnPageLeft->getX() + g_btnPageLeft->getWidth() / 2,
		g_btnPageLeft->getY() - SPACE_Y * 2.0f - g_main_fontsize, str_page, GFX_CENTER);

	if (g_ua_server_connected.length() == 0) { //offline
		sz = gfx->GetTextBlockSize(g_fntOffline, "Offline");
		gfx->SetColor(g_fntOffline, RGB(100, 100, 100));
		gfx->Write(g_fntOffline, win_width/2, (win_height - sz.getY())/2, "Offline", GFX_CENTER);

		string refresh_txt = ".refreshing serverlist.";
		unsigned long long time = GetTickCount64() / 1000;

		for (int n = 0; n < (int)(time % 4); n++)
			refresh_txt = "." + refresh_txt + ".";

		gfx->SetColor(g_fntMain, RGB(180, 180, 180));
		if (!g_serverlist_defined) {
			if (IS_UA_SERVER_REFRESHING)
				gfx->Write(g_fntMain, win_width / 2, (win_height + sz.getY()) / 2, refresh_txt, GFX_CENTER);
			else {
				string text = (g_btnConnect.empty() ? "No servers found\n" : "");
				text += "Click to refresh the serverlist";
				gfx->Write(g_fntMain, win_width / 2, (win_height + sz.getY()) / 2, text, GFX_CENTER);
			}
		}
	}
	else {
		int count = 0;
		int offset = 0;
		bool btn_select = g_btnSelectChannels->isHighlighted();
		
		//BG
		stretch = Vector2D(g_channel_width * (float)g_channels_per_page, win_height);
		gfx->Draw(g_gsChannelBg, g_channel_offset_x, 0, NULL, 
			GFX_NONE, 1.0, 0, NULL, &stretch);

		Channel* reorderChannel = NULL;
		for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
			if (!it->second->isVisible(!btn_select))
				continue;


			if (it->second->touch_point.action == TOUCH_ACTION_REORDER) {
				reorderChannel = it->second;
			}
			if (count >= g_page * g_channels_per_page && count < (g_page + 1) * g_channels_per_page) {
				float x = g_channel_offset_x + (float)offset * g_channel_width;

				//channel seperator
				gfx->DrawShape(GFX_RECTANGLE, BLACK, x + g_channel_width - SPACE_X / 2.0f, 0, SPACE_X, win_height, GFX_NONE, 0.7f);

				if (it->second->touch_point.action != TOUCH_ACTION_REORDER) {
					it->second->draw(x, g_channel_offset_y, g_channel_width, g_channel_height);
				}

				offset++;
			}
			count++;
		}

		if (reorderChannel) {
			float offset = reorderChannel->touch_point.pt_start_x - g_channel_offset_x;
			offset = -fmodf(offset, g_channel_width);
			float x = reorderChannel->touch_point.pt_end_x + offset;

			float reorderInsertX = ceil((x - g_channel_offset_x) / g_channel_width) * g_channel_width + g_channel_offset_x;
			reorderInsertX = max(reorderInsertX, g_channel_offset_x);
			reorderInsertX = min(reorderInsertX, g_channel_offset_x + g_channel_width * (float)g_channels_per_page);
			if (g_page == maxPages()) {
				int channelsOnLastPage = getActiveChannelsCount(true) % g_channels_per_page;
				if (channelsOnLastPage) {
					reorderInsertX = min(reorderInsertX, g_channel_offset_x + g_channel_width * (float)channelsOnLastPage);
				}
			}
			gfx->DrawShape(GFX_RECTANGLE, RGB(200, 200, 50), reorderInsertX - 4.0f, 0, 8.0f, win_height, GFX_NONE, 0.5f);

			gfx->DrawShape(GFX_RECTANGLE, BLACK, x, 0, g_channel_width, win_height, GFX_NONE, 0.5f);
			reorderChannel->draw(x, g_channel_offset_y, g_channel_width, g_channel_height);
		}
	}

	if (g_msg.length()) {
		sz = gfx->GetTextBlockSize(g_fntInfo, g_msg);
		gfx->SetColor(g_fntInfo, WHITE);
		gfx->SetShadow(g_fntInfo, BLACK, 1.0);

		gfx->DrawShape(GFX_RECTANGLE, WHITE, (win_width - sz.getX() - 20.0f) / 2.0f, (win_height - sz.getY() - 20.0f) / 2.0f, 
			sz.getX() + 20.0f, sz.getY() + 20.0f, 0, 0.2f);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, (win_width - sz.getX() - 16.0f) / 2.0f, (win_height - sz.getY() - 16.0f) / 2.0f, 
			sz.getX() + 16.0f, sz.getY() + 16.0f, 0, 0.7f);

		gfx->Write(g_fntInfo, win_width / 2, (win_height - sz.getY()) / 2, g_msg, GFX_CENTER);
		gfx->SetShadow(g_fntInfo, BLACK, 0);
		gfx->SetColor(g_fntInfo, BLACK);
	}

	if (g_btnSettings->isHighlighted()) {

		float box_width = g_btnInfo->getX() + g_btn_width + 20.0f - g_channel_offset_x;
		float box_height = g_btnServerlistScan->getY() + g_btn_height / 2.0f + 20.0f;
		if (g_btnsServers.size() > 1) {
			box_height = g_btnsServers.back()->getY() + g_btn_height / 2.0f + 20.0f;
		}
		gfx->DrawShape(GFX_RECTANGLE, WHITE, g_channel_offset_x, 0, box_width, box_height, 0, 0.3f);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, g_channel_offset_x + 2.0f, 2.0f, box_width - 4.0f, box_height - 4.0f, 0, 0.9f);

		g_btnInfo->draw(BTN_COLOR_YELLOW);
		g_btnFullscreen->draw(BTN_COLOR_GREEN);
		g_btnLockSettings->draw(BTN_COLOR_GREEN);

		float x = g_channel_offset_x + 20.0f;

		string s = "Lock to mix";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, g_btnLockToMixMIX->getY() + g_btnLockToMixMIX->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		g_btnLockToMixMIX->draw(BTN_COLOR_GREEN);
		for (int n = 0; n < 2; n++) {
			g_btnLockToMixAUX[n]->draw(BTN_COLOR_GREEN);
		}
		for (int n = 0; n < 4; n++) {
			g_btnLockToMixCUE[n]->draw(BTN_COLOR_GREEN);
		}

		s = "Label AUX 1";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, g_btnLabelAUX1[0]->getY() + g_btnLabelAUX1[0]->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		for (int n = 0; n < 4; n++) {
			g_btnLabelAUX1[n]->draw(BTN_COLOR_GREEN);
		}

		s = "Label AUX 2";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, g_btnLabelAUX2[0]->getY() + g_btnLabelAUX2[0]->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		for (int n = 0; n < 4; n++) {
			g_btnLabelAUX2[n]->draw(BTN_COLOR_GREEN);
		}

		s = "MIX Meter";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, g_btnPostFaderMeter->getY() + g_btnPostFaderMeter->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		g_btnPostFaderMeter->draw(BTN_COLOR_GREEN);

		s = "Show offline devices";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, g_btnShowOfflineDevices->getY() + g_btnShowOfflineDevices->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		g_btnShowOfflineDevices->draw(BTN_COLOR_GREEN);

		s = "Try to reconnect\nautomatically";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, g_btnReconnect->getY() + g_btnReconnect->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		g_btnReconnect->draw(BTN_COLOR_GREEN);

		s = "Server list";
		gfx->SetColor(g_fntMain, WHITE);
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->Write(g_fntMain, x, g_btnServerlistScan->getY() + g_btnServerlistScan->getHeight() / 2 - sz.getY() / 2.0f, s, GFX_LEFT);
		g_btnServerlistScan->draw(BTN_COLOR_GREEN);
		for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
			(*it)->draw(BTN_COLOR_GREEN);
		}

		gfx->SetColor(g_fntMain, BLACK);
	}

	if (g_btnInfo->isHighlighted()) {
		sz = gfx->GetTextBlockSize(g_fntInfo, INFO_TEXT);
		gfx->SetColor(g_fntInfo, RGB(220,220,220));
		gfx->SetShadow(g_fntInfo, BLACK, 1.0f);

		gfx->DrawShape(GFX_RECTANGLE, WHITE, (win_width - sz.getX() - 30.0f) / 2.0f, (win_height - sz.getY() - 30.0f) / 2.0f, 
			sz.getX() + 30.0f, sz.getY() + 30.0f, 0, 0.3f);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, (win_width - sz.getX() - 26.0f) / 2.0f, (win_height - sz.getY() - 26.0f) / 2.0f, 
			sz.getX() + 26.0f, sz.getY() + 26.0f, 0, 0.9f);

		gfx->Write(g_fntInfo, win_width / 2.0f, (win_height - sz.getY()) / 2.0f, INFO_TEXT, GFX_CENTER);
		gfx->SetShadow(g_fntInfo, BLACK, 0);
		gfx->SetColor(g_fntInfo, BLACK);
	}

	if (g_resetOrderCountdown || g_unmuteAllCountdown || g_selectAllChannelsCountdown) {
		string msg = "Hold for ";
		
		if (g_resetOrderCountdown) {
			msg += to_string(g_resetOrderCountdown) + "s to reset channel order";
		}
		else if(g_unmuteAllCountdown) {
			msg += to_string(g_unmuteAllCountdown) + "s to unmute all channels";
			if (g_settings.lock_to_mix.length()) {
				msg += " of " + g_settings.lock_to_mix;
			}
		}
		else if (g_selectAllChannelsCountdown) {
			if (g_selectAllChannelsCountdown > 4) {
				msg += to_string(g_selectAllChannelsCountdown - 4) + "s to select all channels\n or ";
			}
			msg += to_string(g_selectAllChannelsCountdown) + "s to unselect all channels";
		}

		sz = gfx->GetTextBlockSize(g_fntInfo, msg);
		gfx->SetColor(g_fntInfo, WHITE);
		gfx->SetShadow(g_fntInfo, BLACK, 1.0);

		gfx->DrawShape(GFX_RECTANGLE, WHITE, (win_width - sz.getX() - 20.0f) / 2.0f, (win_height - sz.getY() - 20.0f) / 2.0f,
			sz.getX() + 20.0f, sz.getY() + 20.0f, 0, 0.2f);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, (win_width - sz.getX() - 16.0f) / 2.0f, (win_height - sz.getY() - 16.0f) / 2.0f,
			sz.getX() + 16.0f, sz.getY() + 16.0f, 0, 0.7f);

		gfx->Write(g_fntInfo, win_width / 2, (win_height - sz.getY()) / 2, msg, GFX_CENTER);
		gfx->SetShadow(g_fntInfo, BLACK, 0);
		gfx->SetColor(g_fntInfo, BLACK);
	}
	
	gfx->Update();
}

#ifdef SIMULATION
bool connect(int connection_index)
{
	if (connection_index != g_ua_server_last_connection) {
		g_page = 0;
	}

	g_ua_server_connected = g_ua_server_list[connection_index];

	g_ua_devices[0].id = "0";
	g_ua_devices[0].AllocChannels(UA_INPUT, SIMULATION_CHANNEL_COUNT);
	for (int n = 0; n < SIMULATION_CHANNEL_COUNT; n++)
	{
		g_ua_devices[0].LoadChannels(UA_INPUT, n, to_string(n));
		g_ua_devices[0].inputs[n].name = "Analog " + to_string(n + 1);
		g_ua_devices[0].inputs[n].AllocSends(SIMULATION_SENDS_COUNT);
		for (int i = 0; i < SIMULATION_SENDS_COUNT; i++)
		{
			g_ua_devices[0].inputs[n].LoadSends(i, to_string(i));
		}
	}

	g_btnSelectChannels->setEnable(true);
	g_btnReorderChannels->setEnable(true);
	g_btnChannelWidth->setEnable(true);
	g_btnPageLeft->setEnable(true);
	g_btnPageRight->setEnable(true);
	g_btnMix->setEnable(true);
	g_btnMuteAll->setEnable(true);

	g_ua_server_last_connection = connection_index;

	setRedrawWindow(true);

	return true;
}
#endif

bool g_loading = false;
void setLoadingState(bool loading) {
	g_loading = loading;
}

bool isLoading() {
	return g_loading;
}

#ifndef SIMULATION
bool connect(int connection_index)
{
	if (g_ua_server_connected.length()) {
		disconnect();
	}

	if (connection_index != g_ua_server_last_connection) {
		g_page = 0;
		g_btnMuteAll->setCheck(false);
		g_channelsMutedBeforeAllMute.clear();
	}

	if (g_ua_server_list[connection_index].length() == 0)
		return false;

	//connect to UA
	try
	{
		g_msg = "Connecting to " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT;
		writeLog(LOG_INFO, g_msg);
		draw(); // force draw

		g_tcpClient = new TCPClient(g_ua_server_list[connection_index], UA_TCP_PORT, &tcpClientProc);
		g_ua_server_connected = g_ua_server_list[connection_index];
		writeLog(LOG_INFO, "UA:  Connected on " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT);

		setLoadingState(true);
		tcpClientSend("subscribe /Session");
		tcpClientSend("subscribe /IOMapPreset");
		tcpClientSend("subscribe /PostFaderMetering");
		tcpClientSend("get /devices");

		g_btnSelectChannels->setEnable(true);
		g_btnReorderChannels->setEnable(true);
		g_btnChannelWidth->setEnable(true);
		g_btnMix->setEnable(true);
		g_btnMuteAll->setEnable(true);

		for (size_t n = 0; n < g_btnConnect.size(); n++) {
			if (g_btnConnect[n]->getId() - ID_BTN_CONNECT == connection_index) {
				g_btnConnect[n]->setCheck(true);
				break;
			}
		}

		g_ua_server_last_connection = connection_index;

		g_msg = "";

		setNetworkTimeout();

		setRedrawWindow(true);
	}
	catch (string error) {
		g_msg = "Connection failed on " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT + ": " + string(error);
		writeLog(LOG_ERROR, "UA:  " + g_msg);
		disconnect();
		return false;
	}
	return true;
}
#endif

void disconnect()
{
	if (g_timer_network_timeout != 0) {
		SDL_RemoveTimer(g_timer_network_timeout);
		g_timer_network_timeout = 0;
	}
	if (g_timerFlipPage != 0) {
		SDL_RemoveTimer(g_timerFlipPage);
		g_timerFlipPage = 0;
	}

	g_btnSelectChannels->setEnable(false);
	g_btnReorderChannels->setEnable(false);
	g_btnChannelWidth->setEnable(false);
	g_btnPageLeft->setEnable(false);
	g_btnPageRight->setEnable(false);
	g_btnMix->setEnable(false);
	g_btnPostFaderMeter->setEnable(false);
	g_btnMuteAll->setEnable(false);

	saveServerSettings(g_ua_server_connected);

	if (g_tcpClient) {
		writeLog(LOG_INFO, "UA:  Disconnect from " + g_ua_server_connected + ":" + UA_TCP_PORT);
		delete g_tcpClient;
		g_tcpClient = NULL;
	}

	g_ua_server_connected = "";
	cleanUpSendButtons();
	cleanUpUADevices();

	for (size_t n = 0; n < g_btnConnect.size(); n++) {
		g_btnConnect[n]->setCheck(false);
	}

	g_btnMix->setCheck(true);
	setRedrawWindow(true);
}

string getCheckedConnectionButton() {
	for (size_t n = 0; n < g_btnConnect.size(); n++) {
		if (g_btnConnect[n]->isHighlighted()) {
			return g_btnConnect[n]->getText();
		}
	}
	return "";
}

void onTouchDown(Vector2D *mouse_pt, SDL_TouchFingerEvent *touchinput)
{
	Vector2D pos_pt(0, 0);

	if (touchinput)
	{
		int w,h;
		SDL_GetWindowSize(g_window, &w, &h);
		pos_pt.setX(touchinput->x * (float)w); //touchpoint ist normalisiert
		pos_pt.setY(touchinput->y * (float)h);
	}
	else if (mouse_pt)
	{
		pos_pt = *mouse_pt;
	}
	else {
		return;
	}

	Channel *channel = getChannelByPosition(pos_pt);
	if (channel && channel->touch_point.id == 0 && !channel->touch_point.is_mouse)
	{
		Vector2D relative_pos_pt = pos_pt;
		relative_pos_pt.subtractX(g_channel_offset_x);
		relative_pos_pt.setX(fmodf(relative_pos_pt.getX(), g_channel_width));
		relative_pos_pt.subtractY(g_channel_offset_y);

		Module* module = channel->getModule(g_selectedMixBus);

		if (g_btnSelectChannels->isHighlighted()) {
			if (touchinput) {
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}
			else {
				channel->touch_point.is_mouse = true;
			}

			channel->touch_point.action = TOUCH_ACTION_SELECT;
			channel->selected_to_show = !channel->selected_to_show;

			if (channel->isOverriddenShow())
				channel->selected_to_show = true;
			else if (channel->isOverriddenHide())
				channel->selected_to_show = false;

			g_touchpointChannels.insert(channel);

			setRedrawWindow(true);
		}
		else if (g_btnReorderChannels->isHighlighted())
		{
			if (!g_reorder_dragging) {
				g_reorder_dragging = true;
				g_dragPageFlip = 0;
				if (touchinput)
				{
					channel->touch_point.id = touchinput->touchId;
					channel->touch_point.finger_id = touchinput->fingerId;
					channel->touch_point.pt_start_x = pos_pt.getX();
					channel->touch_point.pt_start_y = pos_pt.getY();
				}
				else if (mouse_pt)
				{
					channel->touch_point.is_mouse = true;
					channel->touch_point.pt_start_x = mouse_pt->getX();
					channel->touch_point.pt_start_y = mouse_pt->getY();
				}
				channel->touch_point.pt_end_x = channel->touch_point.pt_start_x;
				channel->touch_point.pt_end_y = channel->touch_point.pt_start_y;
				channel->touch_point.action = TOUCH_ACTION_REORDER;

				g_touchpointChannels.insert(channel);
			}
		}
		else if (channel->isTouchOnPan(&relative_pos_pt) && channel->type == INPUT)
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
				channel->touch_point.pt_start_x = pos_pt.getX();
				channel->touch_point.pt_start_y = pos_pt.getY();
			}
			else if(mouse_pt)
			{
				channel->touch_point.is_mouse = true;
				channel->touch_point.pt_start_x = mouse_pt->getX();
				channel->touch_point.pt_start_y = mouse_pt->getY();
			}
			channel->touch_point.pt_end_x = channel->touch_point.pt_start_x;
			channel->touch_point.pt_end_y = channel->touch_point.pt_start_y;
			channel->touch_point.action = TOUCH_ACTION_PAN;

			g_touchpointChannels.insert(channel);

			channel->updateSubscription(false, PAN);
		}
		else if (channel->isTouchOnPan2(&relative_pos_pt) && channel->type == INPUT)
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
				channel->touch_point.pt_start_x = pos_pt.getX();
				channel->touch_point.pt_start_y = pos_pt.getY();
			}
			else if (mouse_pt)
			{
				channel->touch_point.is_mouse = true;
				channel->touch_point.pt_start_x = mouse_pt->getX();
				channel->touch_point.pt_start_y = mouse_pt->getY();
			}
			channel->touch_point.pt_end_x = channel->touch_point.pt_start_x;
			channel->touch_point.pt_end_y = channel->touch_point.pt_start_y;
			channel->touch_point.action = TOUCH_ACTION_PAN2;

			g_touchpointChannels.insert(channel);

			channel->updateSubscription(false, PAN);
		}
		else if (channel->isTouchOnMute(&relative_pos_pt))
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}
			else
				channel->touch_point.is_mouse = true;
			channel->touch_point.action = TOUCH_ACTION_MUTE;

			if (channel->fader_group) {
				int state = (int)!module->mute;
				for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
					if (it->second->isVisible(false)) {
						if (it->second->fader_group == channel->fader_group) {
							Module* moduleGroup = it->second->getModule(g_selectedMixBus);
							moduleGroup->pressMute(state);
						}
					}
				}
			}
			else {
				module->pressMute();
			}
			setRedrawWindow(true);
			g_touchpointChannels.insert(channel);
		}
		else if (channel->isTouchOnSolo(&relative_pos_pt) && channel->type == INPUT)
		{
			if (g_selectedMixBus == "MIX") {
				if(touchinput)
				{
					channel->touch_point.id = touchinput->touchId;
					channel->touch_point.finger_id = touchinput->fingerId;
				}
				else
					channel->touch_point.is_mouse = true;
				channel->touch_point.action = TOUCH_ACTION_SOLO;
			
				if (channel->fader_group) {
					int state = (int)!channel->solo;
					for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
						if (it->second->isVisible(false)) {
							if (it->second->fader_group == channel->fader_group) {
								it->second->pressSolo(state);
							}
						}
					}
				}
				else {
					channel->pressSolo();
				}
				setRedrawWindow(true);
				g_touchpointChannels.insert(channel);
			}
		}
		else if (channel->isTouchOnPostFader(&relative_pos_pt) && channel->type == AUX)
		{
			if (g_selectedMixBus == "MIX") {
				if (touchinput)
				{
					channel->touch_point.id = touchinput->touchId;
					channel->touch_point.finger_id = touchinput->fingerId;
				}
				else
					channel->touch_point.is_mouse = true;
				channel->touch_point.action = TOUCH_ACTION_POST_FADER;

				if (channel->fader_group) {
					int state = (int)!channel->post_fader;
					for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
						if (it->second->isVisible(false)) {
							if (it->second->fader_group == channel->fader_group) {
								it->second->pressPostFader(state);
							}
						}
					}
				}
				else {
					channel->pressPostFader();
				}
				setRedrawWindow(true);
				g_touchpointChannels.insert(channel);
			}
		}
		else if (channel->isTouchOnGroup1(&relative_pos_pt) && channel->type != MASTER)
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}	
			else
				channel->touch_point.is_mouse = true;
			channel->touch_point.action = TOUCH_ACTION_GROUP;

			if (channel->fader_group == 1)
				channel->fader_group = 0;
			else
				channel->fader_group = 1;
			g_touchpointChannels.insert(channel);
			setRedrawWindow(true);
		}
		else if (channel->isTouchOnGroup2(&relative_pos_pt) && channel->type != MASTER)
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}
			else
				channel->touch_point.is_mouse = true;
			channel->touch_point.action = TOUCH_ACTION_GROUP;

			if (channel->fader_group == 2)
				channel->fader_group = 0;
			else
				channel->fader_group = 2;
			g_touchpointChannels.insert(channel);
			setRedrawWindow(true);
		}
		else if (channel->isTouchOnFader(&relative_pos_pt)) {
			if (channel->fader_group) {
				for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
					if (it->second->isVisible(true)) {
						if (it->second->fader_group == channel->fader_group) {
							it->second->updateSubscription(false, LEVEL);
							it->second->touch_point.action = TOUCH_ACTION_LEVEL;
						}
					}
				}
			}
			else {
				channel->updateSubscription(false, LEVEL);
			}

			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
				channel->touch_point.pt_start_x = pos_pt.getX();
				channel->touch_point.pt_start_y = pos_pt.getY();
			}
			else if (mouse_pt)
			{
				channel->touch_point.is_mouse = true;
				channel->touch_point.pt_start_x = mouse_pt->getX();
				channel->touch_point.pt_start_y = mouse_pt->getY();
			}
			channel->touch_point.pt_end_x = channel->touch_point.pt_start_x;
			channel->touch_point.pt_end_y = channel->touch_point.pt_start_y;
			channel->touch_point.action = TOUCH_ACTION_LEVEL;
			g_touchpointChannels.insert(channel);
		}
	}
}

void onTouchDrag(Vector2D *mouse_pt, SDL_TouchFingerEvent *touchinput)
{
	Channel* channel = NULL;

	if (touchinput)
		channel = getChannelByTouchpointId(false, touchinput);
	else
		channel = getChannelByTouchpointId((bool)mouse_pt, NULL);

	if (channel && ( (mouse_pt && channel->touch_point.is_mouse) || (touchinput  && channel->touch_point.id) ))
	{
		Vector2D pos_pt(0, 0);
		Module* module = channel->getModule(g_selectedMixBus);
		if (module) {

			if (touchinput) {
				int w, h;
				SDL_GetWindowSize(g_window, &w, &h);
				pos_pt.setX(touchinput->x * (float)w); //touchpoint ist normalisiert
				pos_pt.setY(touchinput->y * (float)h);
			}
			else if (mouse_pt) {
				pos_pt = *mouse_pt;
			}
			else {
				return;
			}

			channel->touch_point.pt_end_x = pos_pt.getX();
			channel->touch_point.pt_end_y = pos_pt.getY();

			if (channel->touch_point.action == TOUCH_ACTION_SELECT) {
				Channel* channelHover = getChannelByPosition(pos_pt);
				if (channelHover && channelHover->touch_point.id == 0 && !channelHover->touch_point.is_mouse)
				{
					channelHover->selected_to_show = channel->selected_to_show;
					//override als selected, wenn name mit ! beginnt
					if (channelHover->isOverriddenShow())
						channelHover->selected_to_show = true;
					//override als nicht selected, wenn name mit # endet
					else if (channelHover->isOverriddenHide())
						channelHover->selected_to_show = false;
					setRedrawWindow(true);
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_REORDER)
			{
				if (g_timerFlipPage == 0 &&
					(channel->touch_point.pt_end_x < g_channel_offset_x ||
						channel->touch_point.pt_end_x > g_channel_offset_x + (float)g_channels_per_page * g_channel_width)) {
					g_timerFlipPage = SDL_AddTimer(800, timerCallbackFlipPage, channel);
				}
				setRedrawWindow(true);
			}
			else if (channel->touch_point.action == TOUCH_ACTION_MUTE)
			{
				Channel* channelHover = getChannelByPosition(pos_pt);
				if (channelHover && channelHover->touch_point.id == 0 && !channelHover->touch_point.is_mouse) {
					Vector2D relative_pos_pt = pos_pt;
					relative_pos_pt.subtractX(g_channel_offset_x);
					relative_pos_pt.setX(fmodf(relative_pos_pt.getX(), g_channel_width));
					relative_pos_pt.subtractY(g_channel_offset_y);

					if (channelHover->isTouchOnMute(&relative_pos_pt)) {
						Module* moduleHover = channelHover->getModule(g_selectedMixBus);
						if (moduleHover) {
							if (channelHover->fader_group) {
								int state = (int)module->mute;
								for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
									if (it->second->isVisible(false)) {
										if (it->second->fader_group == channelHover->fader_group) {
											Module* moduleGroup = it->second->getModule(g_selectedMixBus);
											if (moduleGroup && moduleGroup->mute != (bool)state) {
												moduleGroup->pressMute(state);
											}
										}
									}
								}
							}
							else {
								if (moduleHover->mute != module->mute) {
									moduleHover->pressMute(module->mute);
								}
							}
							setRedrawWindow(true);
						}
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_SOLO) {
				Channel* channelHover = getChannelByPosition(pos_pt);
				if (channelHover && channelHover->touch_point.id == 0 && !channelHover->touch_point.is_mouse && channelHover->type == INPUT) {
					Vector2D relative_pos_pt = pos_pt;
					relative_pos_pt.subtractX(g_channel_offset_x);
					relative_pos_pt.setX(fmodf(relative_pos_pt.getX(), g_channel_width));
					relative_pos_pt.subtractY(g_channel_offset_y);

					if (channelHover->isTouchOnSolo(&relative_pos_pt)) {
						if (channelHover->fader_group) {
							int state = (int)channel->solo;
							for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
								if (it->second->isVisible(false)) {
									if (it->second->fader_group == channelHover->fader_group) {
										if (it->second->solo != (bool)state) {
											it->second->pressSolo(state);
										}
									}
								}
							}
						}
						else {
							if (channelHover->solo != channel->solo) {
								channelHover->pressSolo(channel->solo);
							}
						}
						setRedrawWindow(true);
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_POST_FADER) {
				Channel* channelHover = getChannelByPosition(pos_pt);
				if (channelHover && channelHover->touch_point.id == 0 && !channelHover->touch_point.is_mouse && channelHover->type == AUX) {
					Vector2D relative_pos_pt = pos_pt;
					relative_pos_pt.subtractX(g_channel_offset_x);
					relative_pos_pt.setX(fmodf(relative_pos_pt.getX(), g_channel_width));
					relative_pos_pt.subtractY(g_channel_offset_y);

					if (channelHover->isTouchOnPostFader(&relative_pos_pt)) {
						if (channelHover->fader_group) {
							int state = (int)channel->post_fader;
							for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
								if (it->second->isVisible(false)) {
									if (it->second->fader_group == channelHover->fader_group) {
										if (it->second->post_fader != (bool)state) {
											it->second->pressPostFader(state);
										}
									}
								}
							}
						}
						else {
							if (channelHover->post_fader != channel->post_fader) {
								channelHover->pressPostFader(channel->post_fader);
							}
						}
						setRedrawWindow(true);
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_GROUP) {
				Channel* channelHover = getChannelByPosition(pos_pt);

				if (channelHover && channelHover->touch_point.id == 0 && !channelHover->touch_point.is_mouse && channelHover->type != MASTER) {
					Vector2D relative_pos_pt = pos_pt;
					relative_pos_pt.subtractX(g_channel_offset_x);
					relative_pos_pt.setX(fmodf(relative_pos_pt.getX(), g_channel_width));
					relative_pos_pt.subtractY(g_channel_offset_y);

					if (channelHover->isTouchOnGroup1(&relative_pos_pt)
						|| channelHover->isTouchOnGroup2(&relative_pos_pt)) {
						if (channelHover->fader_group != channel->fader_group)
						{
							channelHover->fader_group = channel->fader_group;
							setRedrawWindow(true);
						}
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_PAN) {
				double pan_move = (double)((channel->touch_point.pt_end_x - channel->touch_point.pt_start_x) / g_channel_width);

				if (abs(pan_move) > UA_SENDVALUE_RESOLUTION) {
					channel->touch_point.pt_start_x = channel->touch_point.pt_end_x;
					channel->touch_point.pt_start_y = channel->touch_point.pt_end_y;
					module->changePan(pan_move);
					setRedrawWindow(true);
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_PAN2) {
				double pan_move = (double)((channel->touch_point.pt_end_x - channel->touch_point.pt_start_x) / g_channel_width);

				if (abs(pan_move) > UA_SENDVALUE_RESOLUTION) {
					channel->touch_point.pt_start_x = channel->touch_point.pt_end_x;
					channel->touch_point.pt_start_y = channel->touch_point.pt_end_y;
					module->changePan2(pan_move);
					setRedrawWindow(true);
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_LEVEL) {
				double fader_move = -(double)((channel->touch_point.pt_end_y - channel->touch_point.pt_start_y) / g_faderrail_height);

				if (abs(fader_move) > UA_SENDVALUE_RESOLUTION) {
					channel->touch_point.pt_start_x = channel->touch_point.pt_end_x;
					channel->touch_point.pt_start_y = channel->touch_point.pt_end_y;

					if (channel->fader_group) {
						double dbBefore = toDbFS(module->level);
						double dbAfter = min(12.0, toDbFS(fromMeterScale(toMeterScale(module->level) + fader_move)));
						dbAfter = max(-144.0, dbAfter);
						double dBDif = dbAfter - dbBefore;

						for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
							if (it->second->isVisible(false)) {
								if (it->second->fader_group == channel->fader_group) {
									Module* moduleGroup = it->second->getModule(g_selectedMixBus);
									if (moduleGroup) {
										double level = fromDbFS(toDbFS(moduleGroup->level) + dBDif);
										moduleGroup->changeLevel(level, true);
									}
								}
							}
						}
					}
					else {
						module->changeLevel(fader_move);
					}
					setRedrawWindow(true);
				}
			}
		}
	}
}

void onTouchUp(bool is_mouse, SDL_TouchFingerEvent *touchinput)
{
	Channel* channel = NULL;
		
	if (touchinput)
		channel = getChannelByTouchpointId(false, touchinput);
	else
		channel = getChannelByTouchpointId(is_mouse, 0);

	if (channel)
	{
		if (channel->touch_point.action == TOUCH_ACTION_REORDER) {

			if (g_timerFlipPage != 0){
				SDL_RemoveTimer(g_timerFlipPage);
				g_timerFlipPage = 0;
			}

			float tpX = min(channel->touch_point.pt_end_x, g_channel_offset_x + g_channel_width * (float)g_channels_per_page);
			tpX = max(tpX, g_channel_offset_x);
			float fmove = (tpX - channel->touch_point.pt_start_x) / g_channel_width + (float)(g_dragPageFlip * g_channels_per_page);
			int move = (int)(fmove > 0.0f ? floor(fmove) : ceil(fmove));

			map<int, Channel*> channelsInOrderCopy;

			int channelIndex = -1;
			int invisibleChannels = 0;
			if (move > 0) { // move right
				int n = 0;
				for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
					if (it->second == channel) {
						channelIndex = n;
					}
					else {
						if (channelIndex != -1) {
							if (!it->second->isVisible(true)) {
								invisibleChannels++;
							}
							else if (n == channelIndex + move + invisibleChannels) {
								channelsInOrderCopy.insert({ n, channel });
								channelIndex = -1;
								n++;
							}
						}
						channelsInOrderCopy.insert({ n, it->second });
						n++;
					}
				}
				if (channelIndex != -1) {
					channelsInOrderCopy.insert({ channelsInOrderCopy.size(), channel });
				}
				g_channelsInOrder.swap(channelsInOrderCopy);
			}
			else if (move < 0) { // move left
				int n = (int)g_channelsInOrder.size() - 1;
				for (map<int, Channel*>::reverse_iterator it = g_channelsInOrder.rbegin(); it != g_channelsInOrder.rend(); ++it) {
					if (it->second == channel) {
						channelIndex = n;
					}
					else {
						if (channelIndex != -1) {
							if (!it->second->isVisible(true)) {
								invisibleChannels++;
							}
							else if (n == channelIndex + move - invisibleChannels) {
								channelsInOrderCopy.insert({ n, channel });
								channelIndex = -1;
								n--;
							}
						}
						channelsInOrderCopy.insert({ n, it->second });
						n--;
					}
				}
				if (channelIndex != -1) {
					channelsInOrderCopy.insert({ 0, channel });
				}
				g_channelsInOrder.swap(channelsInOrderCopy);
			}

			setRedrawWindow(true);

			g_reorder_dragging = false;
		}
		else if (channel->touch_point.action == TOUCH_ACTION_LEVEL) {
			if (channel->fader_group) {
				for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
					if (it->second->isVisible(true)) {
						if (it->second->fader_group == channel->fader_group) {
							it->second->updateSubscription(true, LEVEL);
							it->second->touch_point.action = TOUCH_ACTION_NONE;
						}
					}
				}
			}
			else {
				channel->updateSubscription(true, LEVEL);
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_PAN || channel->touch_point.action == TOUCH_ACTION_PAN2) {
			channel->updateSubscription(true, PAN);
		}

		if (touchinput) {
			channel->touch_point.id = 0;
			channel->touch_point.finger_id = 0;
		}
		if (is_mouse) {
			channel->touch_point.is_mouse = false;
		}

		channel->touch_point.action = TOUCH_ACTION_NONE;

		g_touchpointChannels.erase(channel);
	}
}

void onStateChanged_btnSettings(Button *btn) {
	if (btn->getState() == PRESSED) {
		initSettingsDialog();
	}
	else if (btn->getState() == RELEASED) {
		releaseSettingsDialog();
	}
}

void muteChannels(bool mute, bool all) { // for unmute: all or previously muted
	if (mute) {
		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
			if (it->second->mute) {
				g_channelsMutedBeforeAllMute.insert("MIX." + it->first);
			}
			if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == "MIX") {
				it->second->pressMute(ON);
			}

			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				Send* send = it->second->getSendByName(itB->first);
				if (send) {

					if (send->mute) {
						g_channelsMutedBeforeAllMute.insert(itB->first + "." + it->first);
					}
					if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == itB->second->getText()) {
						send->pressMute(ON);
					}
				}
			}
		}
	}
	else {
		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
			if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == "MIX") {
				if (all || g_channelsMutedBeforeAllMute.find("MIX." + it->first) == g_channelsMutedBeforeAllMute.end()) {
					it->second->pressMute(OFF);
				}
			}

			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == itB->first) {
					if (all || g_channelsMutedBeforeAllMute.find(itB->first + "." + it->first) == g_channelsMutedBeforeAllMute.end()) {
						Send* send = it->second->getSendByName(itB->first);
						if (send) {
							send->pressMute(OFF);
						}
					}
				}
			}
		}
		g_channelsMutedBeforeAllMute.clear();
	}
}

void onStateChanged_btnMuteAll(Button *btn) {
	if (btn->getState() & PRESSED) {
		if (!(btn->getState() & CHECKED)) {
			muteChannels(true, true);
		}
		else {
			g_unmuteAllCountdown = 4;
			if (!g_timerUnmuteAll) {
				g_timerUnmuteAll = SDL_AddTimer(1000, timerCallbackUnmuteAll, NULL);
			}
		}
	}
	else if (btn->getState() == RELEASED) {
		if (g_timerUnmuteAll) {
			SDL_RemoveTimer(g_timerUnmuteAll);
			g_timerUnmuteAll = 0;
			g_unmuteAllCountdown = 0;
			muteChannels(false, false);
		}
	}
}

void onStateChanged_btnSelectChannels(Button *btn) {
	if (btn->getState() & PRESSED) {
		if (!(btn->getState() & CHECKED)) {
			browseToSelectedChannel(g_browseToChannel);
			g_browseToChannel = getMiddleSelectedChannel();
			g_btnReorderChannels->setCheck(false);
			updateMaxPages();
			pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
		}
		g_selectAllChannelsCountdown = 8;
		if (!g_timerSelectAllChannels) {
			g_timerSelectAllChannels = SDL_AddTimer(1000, timerCallbackSelectAllChannels, NULL);
		}
	}
	else if (btn->getState() & RELEASED) {

		if (!(btn->getState() & CHECKED)) {
			if (g_selectAllChannelsCountdown < 5) { // dont switch when allselect/unselect happened
				btn->setCheck(true);
			}
			else {
				browseToSelectedChannel(g_browseToChannel);
				updateMaxPages();
				g_browseToChannel = getMiddleSelectedChannel();
				pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
			}
		}

		SDL_RemoveTimer(g_timerSelectAllChannels);
		g_timerSelectAllChannels = 0;
		g_selectAllChannelsCountdown = 0;
	}
	g_visibleChannelsCount = -1;
}

void onStateChanged_btnReorderChannels(Button *btn) {
	if (btn->getState() & PRESSED) {
		g_btnSelectChannels->setCheck(false);
		g_resetOrderCountdown = 4;
		if (!g_timerResetOrder) {
			g_timerResetOrder = SDL_AddTimer(1000, timerCallbackResetOrder, NULL);
		}
	}
	else if (btn->getState() & RELEASED) {
		pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
		SDL_RemoveTimer(g_timerResetOrder);
		g_timerResetOrder = 0;
		g_resetOrderCountdown = 0;
	}
}

void onStateChanged_btnChannelWidth(Button* btn) {
	if (btn->getState() == PRESSED) {

		if (g_settings.channel_width == 0)
			g_settings.channel_width = 2;

		g_settings.channel_width++;
		if (g_settings.channel_width > 3)
			g_settings.channel_width = 1;

		updateLayout();
		updateChannelWidthButton();
		updateMaxPages();
		browseToSelectedChannel(g_browseToChannel);
		pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
	}
}

void onStateChanged_btnPageLeft(Button* btn) {
	if (btn->getState() == PRESSED) {
		pageLeft();
	}
}

void onStateChanged_btnPageRight(Button* btn) {
	if (btn->getState() == PRESSED) {
		pageRight();
	}
}

void onStateChanged_btnsSelectMix(Button* btn) {
	if (btn->getState() == PRESSED) {
		for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
			if (btn != itB->second) {
				itB->second->setCheck(false);
			}
			else {
				g_selectedMixBus = itB->first;
			}
		}
		if (btn != g_btnMix) {
			g_btnMix->setCheck(false);
		}
		else {
			g_selectedMixBus = "MIX";
		}
		g_visibleChannelsCount = -1;
		updateMaxPages();
		updateSubscriptions();
	}
}

void onStateChanged_btnLockSettings(Button* btn) {

	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		g_settings.lock_settings = (btn->isHighlighted() == PRESSED);

		if (g_settings.lock_settings)
			g_settings.save();

		g_btnPostFaderMeter->setEnable(!g_settings.lock_settings && !g_ua_devices.empty());
		g_btnShowOfflineDevices->setEnable(!g_settings.lock_settings);
		g_btnLockToMixMIX->setEnable(!g_settings.lock_settings);
		for (int n = 0; n < 2; n++) {
			g_btnLockToMixAUX[n]->setEnable(!g_settings.lock_settings);
		}
		for (int n = 0; n < 4; n++) {
			g_btnLockToMixCUE[n]->setEnable(!g_settings.lock_settings);
			g_btnLabelAUX1[n]->setEnable(!g_settings.lock_settings);
			g_btnLabelAUX2[n]->setEnable(!g_settings.lock_settings);
		}

		g_btnReconnect->setEnable(!g_settings.lock_settings);

		g_btnServerlistScan->setEnable(!g_settings.lock_settings);
		for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
			(*it)->setEnable(!g_settings.lock_settings);
		}
	}
}

void onStateChanged_btnPostFaderMeter(Button* btn) {
	if (btn->getState() == PRESSED) {
		tcpClientSend("set /PostFaderMetering/value true");
	}
	else if (btn->getState() == RELEASED) {
		tcpClientSend("set /PostFaderMetering/value false");
	}
}

void onStateChanged_btnShowOfflineDevices(Button* btn) {
	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		g_settings.show_offline_devices = g_btnShowOfflineDevices->getState();
		g_visibleChannelsCount = -1;
		updateMaxPages();
		updateSubscriptions();

	}
}

void onStateChanged_btnReconnect(Button* btn) {
	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		g_settings.reconnect_time = g_btnReconnect->isHighlighted() ? 10000 : 0;
	}
}

void onStateChanged_btnsLockToMix(Button* btn) {
	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		if (btn != g_btnLockToMixMIX)
			g_btnLockToMixMIX->setCheck(false);
		for (int n = 0; n < 2; n++) {
			if (btn != g_btnLockToMixAUX[n])
				g_btnLockToMixAUX[n]->setCheck(false);
		}
		for (int n = 0; n < 4; n++) {
			if (btn != g_btnLockToMixCUE[n])
				g_btnLockToMixCUE[n]->setCheck(false);
		}

		if (btn->isHighlighted()) {
			g_settings.lock_to_mix = btn->getText();

			if (g_settings.lock_to_mix == "MIX") {
				g_btnMix->setEnable(true);
				g_btnMix->setCheck(true);
			}
			else {
				g_btnMix->setEnable(false);
				g_btnMix->setCheck(false);
			}
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				itB->second->setEnable(itB->second->getText() == g_settings.lock_to_mix);
				itB->second->setCheck(itB->second->isEnabled());
			}
		}
		else {
			g_settings.lock_to_mix = "";
			g_btnMix->setEnable(true);
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				itB->second->setEnable(true);
			}
		}
		updateAllMuteBtnText();
		pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
	}
}

void onStateChanged_btnsLabelAUX1(Button* btn) {
	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		for (int n = 0; n < 4; n++) {
			if (g_btnLabelAUX1[n] != btn) {
				g_btnLabelAUX1[n]->setCheck(false);
			}
		}
		g_settings.label_aux1 = btn->getText();
	}
}

void onStateChanged_btnsLabelAUX2(Button* btn) {
	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		for (int n = 0; n < 4; n++) {
			if (g_btnLabelAUX2[n] != btn) {
				g_btnLabelAUX2[n]->setCheck(false);
			}
		}
		g_settings.label_aux2 = btn->getText();
	}
}

void onStateChanged_btnsConnect(Button* btn) {
	if (btn->getState() == PRESSED) {
		connect(btn->getId() - ID_BTN_CONNECT);
	}
	else if(btn->getState() == RELEASED) {
		disconnect();
	}
}

void onStateChanged_btnsServerlist(Button* btn) {
	bool updateList = false;
	if (btn->getState() == PRESSED) {
		if (btn == g_btnServerlistScan) {
			for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
				(*it)->setCheck(false);
			}
			disconnect();
			g_serverlist_defined = false;
			updateList = true;
		}
		else {
			if (g_btnServerlistScan->isHighlighted()) {
				terminateAllPingThreads();
				g_btnServerlistScan->setCheck(false);
			}
			int count = 0;
			for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
				if ((*it)->isHighlighted()) {
					count++;
				}
			}
			if (count >= UA_MAX_SERVER_LIST) {
				btn->setCheck(false);
			}
			g_serverlist_defined = true;
			updateList = true;
		}
	}
	else if (btn->getState() == RELEASED) {
		if (btn != g_btnServerlistScan) {
			int count = 0;
			for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
				if ((*it)->isHighlighted()) {
					count++;
				}
			}
			if (count < 1) {
				btn->setCheck(true);
			}
			else {
				if (g_ua_server_connected == btn->getText()) {
					disconnect();
					g_ua_server_last_connection = -1;
				}
			}
			g_serverlist_defined = true;
			updateList = true;
		}
	}
	if (updateList) {
		for (int i = 0; i < UA_MAX_SERVER_LIST; i++) {
			g_settings.serverlist[i] = "";
		}

		if (!g_serverlist_defined) {
			getServerList();
		}
		else {
			int i = 0;
			for (vector<Button*>::iterator it2 = g_btnsServers.begin(); it2 != g_btnsServers.end(); ++it2) {
				if ((*it2)->isHighlighted()) {
					g_settings.serverlist[i] = (*it2)->getText();
					i++;
				}
			}
		}
		updateConnectButtons();
	}
}

void toggleFullscreen() {
	int x, y;
	SDL_GetGlobalMouseState(&x, &y);
	if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) {
		SDL_SetWindowFullscreen(g_window, 0);

		int x, y, w, h;
		SDL_GetWindowPosition(g_window, &x, &y);
		SDL_GetWindowSize(g_window, &w, &h);

		int t, l, b, r;
		SDL_GetWindowBordersSize(g_window, &t, &l, &b, &r);

		SDL_Rect rc;
		if (SDL_GetDisplayUsableBounds(0, &rc) == 0) {
			if (x < rc.x + l)
				x = rc.x + l;
			if (y < rc.y + t)
				y = rc.y + t;
			if (w > rc.w - x - r)
				w = rc.w - x - r;
			if (h > rc.h - y - b)
				h = rc.h - y - b;
		}
		SDL_SetWindowPosition(g_window, x, y);
		SDL_SetWindowSize(g_window, w, h);

		g_btnFullscreen->setCheck(false);
	}
	else {
		SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		g_btnFullscreen->setCheck(true);
	}
	SDL_WarpMouseGlobal(x, y);
}

void onStateChanged_btnFullscreen(Button* btn) {
	if (btn->getState() == PRESSED || btn->getState() == RELEASED) {
		toggleFullscreen();
	}
}

void createStaticButtons()
{
	g_btnSettings = new Button(CHECK, ID_BTN_INFO, "Settings",0,0,g_btn_width, g_btn_height,false, true, true, &onStateChanged_btnSettings);
	g_btnMuteAll = new Button(CHECK, ID_BTN_INFO, "Mute all", 0, 0, g_btn_width, g_btn_height, false, false, true, &onStateChanged_btnMuteAll);
	updateAllMuteBtnText();
	g_btnSelectChannels = new Button(CHECK, ID_BTN_SELECT_CHANNELS, "Select\nChannels",0,0,g_btn_width, g_btn_height, 
		false, false, true, &onStateChanged_btnSelectChannels);
	g_btnReorderChannels = new Button(CHECK, ID_BTN_SELECT_CHANNELS, "Reorder\nChannels", 0, 0, g_btn_width, g_btn_height, 
		false, false, true, &onStateChanged_btnReorderChannels);
	g_btnChannelWidth = new Button(BUTTON, ID_BTN_CHANNELWIDTH, "View:\nRegular",0,0,g_btn_width, g_btn_height, 
		false, false, true, &onStateChanged_btnChannelWidth);
	g_btnPageLeft = new Button(BUTTON, ID_BTN_PAGELEFT, "Page Left",0,0,g_btn_width, g_btn_height,false, false, true, &onStateChanged_btnPageLeft);
	g_btnPageRight = new Button(BUTTON, ID_BTN_PAGELEFT, "Page Right",0,0,g_btn_width, g_btn_height,false, false, true, &onStateChanged_btnPageRight);
	g_btnMix = new Button(RADIO, ID_BTN_MIX, "MIX", 0, 0, g_btn_width, g_btn_height, true,false, true, &onStateChanged_btnsSelectMix);

	g_btnLockSettings = new Button(CHECK, 0, "Lock settings", 0, 0, g_btn_width, g_btn_height / 2.0f, g_settings.lock_settings, 
		true, true, &onStateChanged_btnLockSettings);
	g_btnPostFaderMeter = new Button(CHECK, 0, "Post Fader", 0, 0, g_btn_width, g_btn_height / 2.0f, false, false, true, &onStateChanged_btnPostFaderMeter);
	g_btnShowOfflineDevices = new Button(CHECK, 0, "On", 0, 0, g_btn_width, g_btn_height / 2.0f, g_settings.show_offline_devices, !g_settings.lock_settings,
		true, &onStateChanged_btnShowOfflineDevices);
	g_btnLockToMixMIX = new Button(CHECK, 0, "MIX", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.lock_to_mix == "MIX", !g_settings.lock_settings,
		true, &onStateChanged_btnsLockToMix);

	for (int n = 0; n < 2; n++) {
		g_btnLockToMixAUX[n] = new Button(CHECK, 0, "AUX " + to_string(n + 1), 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f,
			g_settings.lock_to_mix == "AUX " + to_string(n + 1), !g_settings.lock_settings, true, &onStateChanged_btnsLockToMix);
	}
	for (int n = 0; n < 4; n++) {
		g_btnLockToMixCUE[n] = new Button(CHECK, 0, "CUE " + to_string(n + 1), 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f,
			g_settings.lock_to_mix == "CUE " + to_string(n + 1), !g_settings.lock_settings, true, &onStateChanged_btnsLockToMix);
	}

	g_btnLabelAUX1[0] = new Button(RADIO, 0, "AUX", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "AUX", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX1);
	g_btnLabelAUX1[1] = new Button(RADIO, 0, "FX", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "FX", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX1);
	g_btnLabelAUX1[2] = new Button(RADIO, 0, "Reverb", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "Reverb", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX1);
	g_btnLabelAUX1[3] = new Button(RADIO, 0, "Delay", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "Delay", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX1);

	g_btnLabelAUX2[0] = new Button(RADIO, 0, "AUX", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "AUX", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX2);
	g_btnLabelAUX2[1] = new Button(RADIO, 0, "FX", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "FX", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX2);
	g_btnLabelAUX2[2] = new Button(RADIO, 0, "Reverb", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "Reverb", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX2);
	g_btnLabelAUX2[3] = new Button(RADIO, 0, "Delay", 0.0f, 0.0f, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "Delay", 
		!g_settings.lock_settings, true, &onStateChanged_btnsLabelAUX2);

	g_btnReconnect = new Button(CHECK, 0, "On", 0.0f, 0.0f, g_btn_width, g_btn_height / 2.0f, g_settings.reconnect_time != 0, !g_settings.lock_settings,
		true, &onStateChanged_btnReconnect);

	g_btnServerlistScan = new Button(RADIO, 0, "Scan network", 0.0f, 0.0f, g_btn_width, g_btn_height / 2.0f, true, !g_settings.lock_settings,
		true, &onStateChanged_btnsServerlist);

	g_btnInfo = new Button(CHECK, 0, "Info", 0.0f, 0.0f, g_btn_width, g_btn_height / 2.0f, false, true);
	g_btnFullscreen = new Button(CHECK, 0, "Fullscreen", 0.0f, 0.0f, g_btn_width, g_btn_height / 2.0f, 
		g_settings.fullscreen, true, true, &onStateChanged_btnFullscreen);
}

void cleanUpStaticButtons()
{
	SAFE_DELETE(g_btnSelectChannels);
	SAFE_DELETE(g_btnReorderChannels);
	SAFE_DELETE(g_btnChannelWidth);
	SAFE_DELETE(g_btnPageLeft);
	SAFE_DELETE(g_btnPageRight);
	SAFE_DELETE(g_btnSettings);
	SAFE_DELETE(g_btnMuteAll);
	SAFE_DELETE(g_btnMix);

	SAFE_DELETE(g_btnLockSettings);
	SAFE_DELETE(g_btnPostFaderMeter);
	SAFE_DELETE(g_btnShowOfflineDevices);
	SAFE_DELETE(g_btnLockToMixMIX);
	for (int n = 0; n < 2; n++) {
		SAFE_DELETE(g_btnLockToMixAUX[n]);
	}
	for (int n = 0; n < 2; n++) {
		SAFE_DELETE(g_btnLockToMixCUE[n]);
	}
	SAFE_DELETE(g_btnReconnect);
	SAFE_DELETE(g_btnServerlistScan);
	SAFE_DELETE(g_btnInfo);
	SAFE_DELETE(g_btnFullscreen);
}

Button *addSendButton(string name)
{
	Button* btn = new Button(RADIO, ID_BTN_SENDS + (int)g_btnSends.size(), name, 0, 0, 0, 0, false, true, true, &onStateChanged_btnsSelectMix);
	g_btnSends.push_back({ name, btn });

	updateLayout();
	updateSubscriptions();

	return btn;
}

void updateChannelWidthButton() {
	switch (g_settings.channel_width) {
	case 1:
		g_btnChannelWidth->setText("View:\nNarrow");
		break;
	case 3:
		g_btnChannelWidth->setText("View:\nWide");
		break;
	default:
		g_btnChannelWidth->setText("View:\nRegular");
	}
}

void updateConnectButtons()
{
	int w,h;
	SDL_GetWindowSize(g_window,&w,&h);

	string connected_btn_text = getCheckedConnectionButton();

	cleanUpConnectionButtons();

	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	for(int i = 0; i < (int)g_ua_server_list.size(); i++)
	{
		bool useit = !g_serverlist_defined;

		if (g_serverlist_defined) {
			for (int n = 0; n < UA_MAX_SERVER_LIST; n++) {
				if (g_settings.serverlist[n].length() && g_ua_server_list[i] == g_settings.serverlist[n]) {
					useit = true;
					break;
				}
			}
		}

		if (useit) {
			string btn_text = "Connect to\n" + g_ua_server_list[i];
			g_btnConnect.push_back(new Button(CHECK, ID_BTN_CONNECT + i, btn_text,
				(float)w - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f,
				g_channel_offset_y + (float)g_btnConnect.size() * (g_btn_height + SPACE_Y),
				g_btn_width, g_btn_height, (btn_text == connected_btn_text), true, true, &onStateChanged_btnsConnect
			));

			if (g_btnConnect.size() >= UA_MAX_SERVER_LIST)
				break;
		}
	}

	if (g_btnSettings->isHighlighted() && !g_serverlist_defined) {
		releaseSettingsDialog();
		initSettingsDialog();
	}

	setRedrawWindow(true);

	//wenn keine verbindung, verbinde automatisch mit dem ersten server
	if (g_btnConnect.size() && !g_ua_server_connected.length()){
		pushEvent(EVENT_CONNECT);
	}
}

void updateAllMuteBtnText() {
	if (g_btnMuteAll) {
		if (g_settings.lock_to_mix.length()) {
			g_btnMuteAll->setText("Mute " + g_settings.lock_to_mix);
		}
		else {
			g_btnMuteAll->setText("Mute All");
		}
	}
}

GFXSurface* loadGfx(string filename, bool keepRAM = false) {
	string path = getDataPath(filename);
	GFXSurface *gs = gfx->LoadGfx(path);
	if (!gs) {
		writeLog(LOG_ERROR, "loadGfx " + path + " failed");
		return NULL;
	}

	// usually GFXEngine keeps a copy of the image in RAM but here it's not needed
	if (!keepRAM) {
		gfx->FreeRAM(gs);
	}

	return gs;
}

bool loadAllGfx() {

	if (!(g_gsButtonYellow[0] = loadGfx("btn_yellow_off.gfx", true)))
		return false;
	if (!(g_gsButtonYellow[1] = loadGfx("btn_yellow_on.gfx", true)))
		return false;

	if (!(g_gsButtonRed[0] = loadGfx("btn_red_off.gfx", true)))
		return false;
	if (!(g_gsButtonRed[1] = loadGfx("btn_red_on.gfx", true)))
		return false;

	if (!(g_gsButtonGreen[0] = loadGfx("btn_green_off.gfx", true)))
		return false;
	if (!(g_gsButtonGreen[1] = loadGfx("btn_green_on.gfx", true)))
		return false;

	if (!(g_gsButtonBlue[0] = loadGfx("btn_blue_off.gfx", true)))
		return false;
	if (!(g_gsButtonBlue[1] = loadGfx("btn_blue_on.gfx", true)))
		return false;

	if (!(g_gsChannelBg = loadGfx("channelbg.gfx")))
		return false;

	if (!(g_gsBgRight = loadGfx("bg_right.gfx")))
		return false;

	if (!(g_gsFaderrail = loadGfx("faderrail.gfx")))
		return false;

	if (!(g_gsFaderWhite = loadGfx("fader.gfx", true)))
		return false;
	//black
	if (!(g_gsFaderBlack = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderBlack, 0.5f, BLACK);
	gfx->SetFilter_Saturation(g_gsFaderBlack, -1.0f);
	gfx->SetFilter_Gamma(g_gsFaderBlack, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderBlack, 1.3f);
	gfx->ApplyFilter(g_gsFaderBlack);
	gfx->FreeRAM(g_gsFaderBlack);
	//red
	if (!(g_gsFaderRed = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderRed, 0.3f, RED);
	gfx->SetFilter_Gamma(g_gsFaderRed, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderRed, 1.3f);
	gfx->ApplyFilter(g_gsFaderRed);
	gfx->FreeRAM(g_gsFaderRed);
	//green
	if (!(g_gsFaderGreen = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderGreen, 0.3f, GREEN);
	gfx->SetFilter_Gamma(g_gsFaderGreen, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderGreen, 1.3f);
	gfx->ApplyFilter(g_gsFaderGreen);
	gfx->FreeRAM(g_gsFaderGreen);
	//blue
	if (!(g_gsFaderBlue = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderBlue, 0.3f, BLUE);
	gfx->SetFilter_Gamma(g_gsFaderBlue, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderBlue, 1.3f);
	gfx->ApplyFilter(g_gsFaderBlue);
	gfx->FreeRAM(g_gsFaderBlue);
	//yellow
	if (!(g_gsFaderYellow = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderYellow, 0.3f, YELLOW);
	gfx->SetFilter_Gamma(g_gsFaderYellow, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderYellow, 1.3f);
	gfx->ApplyFilter(g_gsFaderYellow);
	gfx->FreeRAM(g_gsFaderYellow);
	//orange
	if (!(g_gsFaderOrange = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderOrange, 0.3f, ORANGE);
	gfx->SetFilter_Gamma(g_gsFaderOrange, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderOrange, 1.3f);
	gfx->ApplyFilter(g_gsFaderOrange);
	gfx->FreeRAM(g_gsFaderOrange);
	//purple
	if (!(g_gsFaderPurple = gfx->CopySurface(g_gsFaderWhite)))
		return false;
	gfx->SetFilter_ColorShift(g_gsFaderPurple, 0.3f, PURPLE);
	gfx->SetFilter_Gamma(g_gsFaderPurple, 0.8f);
	gfx->SetFilter_Contrast(g_gsFaderPurple, 1.3f);
	gfx->ApplyFilter(g_gsFaderPurple);
	gfx->FreeRAM(g_gsFaderPurple);
	gfx->FreeRAM(g_gsFaderWhite);

	if (!(g_gsPan = loadGfx("poti.gfx")))
		return false;

	if (!(g_gsPanPointer = loadGfx("poti_pointer.gfx")))
		return false;

	for (int n = 0; n < 4; n++) {
		if (!(g_gsLabelWhite[n] = loadGfx("label" + to_string(n + 1) + ".gfx", true))) {
			return false;
		}

		//black
		if (!(g_gsLabelBlack[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelBlack[n], 0.7f, BLACK);
		gfx->SetFilter_Saturation(g_gsLabelBlack[n], -1.0f);
		gfx->SetFilter_Gamma(g_gsLabelBlack[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelBlack[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelBlack[n]);
		gfx->FreeRAM(g_gsLabelBlack[n]);

		//red
		if (!(g_gsLabelRed[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelRed[n], 0.4f, RED);
		gfx->SetFilter_Gamma(g_gsLabelRed[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelRed[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelRed[n]);
		gfx->FreeRAM(g_gsLabelRed[n]);

		//green
		if (!(g_gsLabelGreen[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelGreen[n], 0.4f, GREEN);
		gfx->SetFilter_Gamma(g_gsLabelGreen[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelGreen[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelGreen[n]);
		gfx->FreeRAM(g_gsLabelGreen[n]);

		//blue
		if (!(g_gsLabelBlue[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelBlue[n], 0.4f, BLUE);
		gfx->SetFilter_Gamma(g_gsLabelBlue[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelBlue[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelBlue[n]);
		gfx->FreeRAM(g_gsLabelBlue[n]);

		//yellow
		if (!(g_gsLabelYellow[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelYellow[n], 0.4f, YELLOW);
		gfx->SetFilter_Gamma(g_gsLabelYellow[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelYellow[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelYellow[n]);
		gfx->FreeRAM(g_gsLabelYellow[n]);

		//orange
		if (!(g_gsLabelOrange[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelOrange[n], 0.4f, ORANGE);
		gfx->SetFilter_Gamma(g_gsLabelOrange[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelOrange[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelOrange[n]);
		gfx->FreeRAM(g_gsLabelOrange[n]);

		//purple
		if (!(g_gsLabelPurple[n] = gfx->CopySurface(g_gsLabelWhite[n]))) {
			return false;
		}
		gfx->SetFilter_ColorShift(g_gsLabelPurple[n], 0.4f, PURPLE);
		gfx->SetFilter_Gamma(g_gsLabelPurple[n], 0.8f);
		gfx->SetFilter_Contrast(g_gsLabelPurple[n], 1.3f);
		gfx->ApplyFilter(g_gsLabelPurple[n]);
		gfx->FreeRAM(g_gsLabelPurple[n]);

		gfx->FreeRAM(g_gsLabelWhite[n]);
	}

	return true;
}

void releaseAllGfx()
{
	for (int n = 0; n < 2; n++) {
		gfx->DeleteSurface(g_gsButtonYellow[n]);
		gfx->DeleteSurface(g_gsButtonRed[n]);
		gfx->DeleteSurface(g_gsButtonGreen[n]);
		gfx->DeleteSurface(g_gsButtonBlue[n]);
	}
	gfx->DeleteSurface(g_gsChannelBg);
	gfx->DeleteSurface(g_gsBgRight);
	gfx->DeleteSurface(g_gsFaderrail);
	gfx->DeleteSurface(g_gsFaderWhite);
	gfx->DeleteSurface(g_gsFaderBlack);
	gfx->DeleteSurface(g_gsFaderRed);
	gfx->DeleteSurface(g_gsFaderGreen);
	gfx->DeleteSurface(g_gsFaderBlue);
	gfx->DeleteSurface(g_gsFaderYellow);
	gfx->DeleteSurface(g_gsFaderOrange);
	gfx->DeleteSurface(g_gsFaderPurple);
	gfx->DeleteSurface(g_gsPan);
	gfx->DeleteSurface(g_gsPanPointer);

	for (int n = 0; n < 4; n++) {
		gfx->DeleteSurface(g_gsLabelWhite[n]);
		gfx->DeleteSurface(g_gsLabelBlack[n]);
		gfx->DeleteSurface(g_gsLabelRed[n]);
		gfx->DeleteSurface(g_gsLabelGreen[n]);
		gfx->DeleteSurface(g_gsLabelBlue[n]);
		gfx->DeleteSurface(g_gsLabelYellow[n]);
		gfx->DeleteSurface(g_gsLabelOrange[n]);
		gfx->DeleteSurface(g_gsLabelPurple[n]);
	}
}

void initSettingsDialog() {
	
	g_btnServerlistScan->setCheck(true);

	for (vector<string>::iterator it = g_ua_server_list.begin(); it != g_ua_server_list.end(); ++it) {
		bool listed = false;
		for (int n = 0; n < UA_MAX_SERVER_LIST; n++) {
			if (*it == g_settings.serverlist[n]) {
				g_btnServerlistScan->setCheck(false);
				listed = true;
				break;
			}
		}
		g_btnsServers.push_back(new Button(CHECK, 0, *it, 0, 0, g_btn_width, g_btn_height / 2.0f, listed, !g_settings.lock_settings,
			true, &onStateChanged_btnsServerlist));

		if (g_btnsServers.size() >= UA_MAX_SERVER_LIST_SETTING)
			break;
	}
	updateLayout();
}

void cleanUpUADevices() {
	clearChannels();
	for (vector<UADevice*>::iterator it = g_ua_devices.begin(); it != g_ua_devices.end(); ++it) {
		SAFE_DELETE(*it);
	}
	g_ua_devices.clear();
}

void releaseSettingsDialog() {
	for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
		SAFE_DELETE(*it);
	}
	g_btnsServers.clear();
}

bool loadServerSettings(string server_name, Button *btnSend)
{
	if (server_name.length() == 0 || !btnSend)
		return false;

	bool result = true;

	//JSON
	try
	{
		dom::parser parser;
		dom::element element;

#ifdef __ANDROID__
		element = parser.parse(serverSettingsJSON[server_name]);
#else
		string filename = server_name + ".json";
		element = parser.load(getPrefPath(filename));
#endif

		string_view selected_send = element["ua_server"]["selected_mix"];

		if (!g_settings.lock_to_mix.length() && btnSend->getText() == selected_send)
		{
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				if (itB->second->getId() == btnSend->getId())
					continue;
				itB->second->setCheck(false);
				g_selectedMixBus = itB->first;
			}
			g_btnMix->setCheck(false);

			btnSend->setCheck(true);
		}
	}
	catch (simdjson_error&)
	{
		result = false;
	}
	
	// disable mix-button if locked to different mix
	if (g_settings.lock_to_mix.length()) {
		if (g_settings.lock_to_mix == "MIX") {
			g_btnMix->setEnable(true);
			g_btnMix->setCheck(true);
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				itB->second->setEnable(false);
				itB->second->setCheck(false);
			}
		}
		else {
			g_btnMix->setEnable(false);
			g_btnMix->setCheck(false);
			btnSend->setEnable(btnSend->getText() == g_settings.lock_to_mix);
			btnSend->setCheck(btnSend->getText() == g_settings.lock_to_mix);
		}
	}

	return result;
}

bool loadServerSettings(string server_name) // läd channel-select & group-setting
{
	if (server_name.length() == 0)
		return false;

	string filename = server_name + ".json";

	//JSON
	try
	{
		dom::parser parser;
		dom::element element;

#ifdef __ANDROID__
		element = parser.parse(serverSettingsJSON[server_name]);
#else
		string filename = server_name + ".json";
		element = parser.load(getPrefPath(filename));
#endif
		map<int, Channel*> channelsInOrder;
		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {

			string type_str = it->second->type == INPUT ? ".input." : ".aux.";

			try
			{
				int order = (int)((int64_t)element["ua_server"][it->second->device->id + type_str + it->second->id + " order"]);
				channelsInOrder.insert({ order, it->second });

			}
			catch (simdjson_error&) {}
			try
			{
				it->second->selected_to_show = element["ua_server"][it->second->device->id + type_str + it->second->id + " selected"];

				if (it->second->isOverriddenShow())
					it->second->selected_to_show = true;
				else if (it->second->isOverriddenHide())
					it->second->selected_to_show = false;
			}
			catch (simdjson_error&) {}
			try
			{
				int64_t value = element["ua_server"][it->second->device->id + type_str + it->second->id + " group"];
				it->second->fader_group = (int)value;
			}
			catch (simdjson_error&) {}
		}

		//validate if all channels ordered
		if (channelsInOrder.size() == g_channelsInOrder.size()) {
			g_channelsInOrder.swap(channelsInOrder);
		}

		g_browseToChannel = (int)((int64_t)element["ua_server"]["first_visible_channel"]);
	}
	catch (simdjson_error&) {
		return false;
	}
	return true;
}

bool saveServerSettings(string server_name)
{
	if (server_name.length() == 0)
		return false;

	string json = "{\n";

	if (g_ua_server_connected.length())
	{
		json += "\"ua_server\": {\n";
		json += "\"name\": \"" + g_ua_server_connected  + "\",\n";

		json += "\"selected_mix\": ";
		json += "\"" + g_selectedMixBus + "\",\n";

		string ua_dev;
		int channel = getMiddleSelectedChannel();
		json += "\"first_visible_channel\": " + to_string(channel) + ",\n";


		bool set_komma = false;
		for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {

			if (set_komma) {
				json += ",\n";
			}			

			string type_str = it->second->type == INPUT ? ".input." : ".aux.";
			json += "\"" + it->second->device->id + type_str + it->second->id + " order\": " + to_string(it->first) + "\n,";
			json += "\"" + it->second->device->id + type_str + it->second->id + " group\": " + to_string(it->second->fader_group) + "\n,";
			json += "\"" + it->second->device->id + type_str + it->second->id + " selected\": ";
			
			if (it->second->selected_to_show) {
				json += "true";
			}
			else {
				json += "false";
			}
			set_komma = true;
		}

		json += "}\n";
	}

	json += "}";

#ifdef __ANDROID__
	serverSettingsJSON.insert_or_assign(server_name, json);
	// store to map and save map @ onPause in Cuefinger-class (java)
#else
	string filename = server_name + ".json";
	string path = getPrefPath(filename);

	FILE* fh = NULL;
	if (fopen_s(&fh, path.c_str(), "w") == 0) {
		fputs(json.c_str(), fh);
		fclose(fh);
	}
	else {
		return false;
	}
#endif

	return true;
}

bool Settings::load(string *json) {
	//JSON
	try {
        dom::parser parser;
        dom::element element;
        if (!json) {
            string filename = "settings.json";
            element = parser.load(getPrefPath(filename));
        }
        else {
            element = parser.parse(*json);
        }

		try {
			this->lock_settings = element["general"]["lock_settings"];
		}
		catch (simdjson_error&) {}

		try {
			this->show_offline_devices = element["general"]["show_offline_devices"];
		}
		catch (simdjson_error&) {}

		try {
			string_view sv = element["general"]["lock_to_mix"];
			this->lock_to_mix = string{sv};
		}
		catch (simdjson_error&) {}

		try {
			string_view sv = element["general"]["label_aux1"];
			this->label_aux1 = string{ sv };
		}
		catch (simdjson_error&) {}

		try {
			string_view sv = element["general"]["label_aux2"];
			this->label_aux2 = string{ sv };
		}
		catch (simdjson_error&) {}

		updateAllMuteBtnText();

		try {
			int64_t reconnect_time = element["general"]["reconnect_time"];
			this->reconnect_time = (unsigned int)reconnect_time;
		}
		catch (simdjson_error&) {}

		try {
			this->extended_logging = element["general"]["extended_logging"];
		}
		catch (simdjson_error&) {}

		try {
			this->maximized = element["window"]["maximized"];
			this->fullscreen = element["window"]["fullscreen"];
		}
		catch (simdjson_error&) {}

		int64_t x, y, w, h;
		x = element["window"]["x"];
		y = element["window"]["y"];
		w = element["window"]["width"];
		h = element["window"]["height"];

		this->x = (int)x;
		this->y = (int)y;
		this->w = (int)w;
		this->h = (int)h;

		try {
			int64_t channel_width = element["view"]["channel_width"];
			this->channel_width = (int)channel_width;
		}
		catch (simdjson_error&) {}

		for (int n = 0; n < UA_MAX_SERVER_LIST; n++) {
			try {
				string_view sv = element["serverlist"][to_string(n+1)];
				this->serverlist[n] = string{sv};
			}
			catch (simdjson_error&) {}
		}
	}
	catch (simdjson_error&) {
		return false;
	}

	return true;
}

string Settings::toJSON() {
    string json;

    json = "{\n";

    //STARTUP
    json+= "\"general\": {\n";
	if (this->lock_settings) {
		json += "\"lock_settings\": true,\n";
	}
	else {
		json += "\"lock_settings\": false,\n";
	}
	if (this->show_offline_devices) {
		json += "\"show_offline_devices\": true,\n";
	}
	else {
		json += "\"show_offline_devices\": false,\n";
	}
    json+="\"lock_to_mix\": \"" + this->lock_to_mix + "\",\n";

	json += "\"label_aux1\": \"" + this->label_aux1 + "\",\n";
	json += "\"label_aux2\": \"" + this->label_aux2 + "\",\n";

    json+="\"reconnect_time\": " + to_string(this->reconnect_time) + ",\n";
    if(this->extended_logging)
        json+="\"extended_logging\": true\n";
    else
        json+="\"extended_logging\": false\n";
    json+="},\n";

    //WINDOW
    int x, y, w, h;
    SDL_GetWindowSize(g_window, &w, &h);
    SDL_GetWindowPosition(g_window, &x, &y);

    Uint32 flags = SDL_GetWindowFlags(g_window);

    json+="\"window\": {\n";
    if(flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
        json+="\"fullscreen\": true,\n";
    else
        json+="\"fullscreen\": false,\n";
    if(flags & SDL_WINDOW_MAXIMIZED)
        json+="\"maximized\": true,\n";
    else
        json+="\"maximized\": false,\n";
    json+="\"x\": " + to_string(x) + ",\n";
    json+="\"y\": "+ to_string(y) + ",\n";
    json+="\"width\": " + to_string(w) + ",\n";
    json+="\"height\": " + to_string(h) + "\n";
    json+="},\n";

    //VIEW
    json+="\"view\": {\n";
    json+="\"channel_width\": " + to_string(this->channel_width) + "\n";
    json+="},\n";

    //SERVERLIST
    json+="\"serverlist\": {\n";
    for (int n = 0; n < UA_MAX_SERVER_LIST; n++)
    {
        json+="\"" + to_string(n + 1) + "\": \"" + this->serverlist[n] + "\"";
        if(n == UA_MAX_SERVER_LIST - 1)
            json+="\n";
        else
            json+=",\n";
    }
    json+="}\n";

    json+="}";

    return json;
}

bool Settings::save()
{
	string filename = "settings.json";
	string path = getPrefPath(filename);

	FILE* fh = NULL;
	if (fopen_s(&fh, path.c_str(), "w") == 0)
	{
        fputs(this->toJSON().c_str(), fh);

		fclose(fh);
	}
	else
		return false;

	return true;
}

#ifdef __ANDROID__
extern "C" void Java_franqulator_cuefinger_Cuefinger_load(JNIEnv *env, jobject obj, jobject assetManager){

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);
    if (mgr == NULL) {
		writeLog(LOG_ERROR, "error loading asset manager");
    }
    else {
        android_fopen_set_asset_manager(mgr);
    }
}


extern "C" void Java_franqulator_cuefinger_Cuefinger_loadSettings(JNIEnv *env, jobject obj, jstring json){
    string njson((const char*)env->GetStringUTFChars(json, NULL));
    g_settings.load(&njson);
    g_settings.fullscreen = true; // auf android immer im fullscreen starten
}

extern "C" jstring Java_franqulator_cuefinger_Cuefinger_getSettingsJSON(JNIEnv *env, jobject obj){
    return env->NewStringUTF(g_settings.toJSON().c_str());
}

extern "C" void Java_franqulator_cuefinger_Cuefinger_loadServerSettings(JNIEnv* env, jobject obj, jstring json) {
	string njson((const char*)env->GetStringUTFChars(json, NULL));

	size_t posSvr = njson.find("svr=", 0);
	while (posSvr != string::npos) {
		posSvr += 4;

		size_t posSet = njson.find("set=", posSvr);
		if (posSet == string::npos) {
			break;
		}
		string server = njson.substr(posSvr, posSet - posSvr);
		
		posSet += 4;
		posSvr = njson.find("svr=", posSet);
        if(posSvr == string::npos) {
            posSvr = njson.length();
        }
		string settings = njson.substr(posSet, posSvr - posSet);

		serverSettingsJSON.insert_or_assign(server, settings);
	}
}

extern "C" jstring Java_franqulator_cuefinger_Cuefinger_getServerSettingsJSON(JNIEnv* env, jobject obj) {
	string njson;

    if(g_ua_server_connected.length() != 0) {
        saveServerSettings(g_ua_server_connected);
    }

	for (map<string, string>::iterator it = serverSettingsJSON.begin(); it != serverSettingsJSON.end(); ++it) {
		njson += "svr=" + it->first + "set=" +  it->second;
	}

	return env->NewStringUTF(njson.c_str());
}

extern "C" void Java_franqulator_cuefinger_Cuefinger_terminateAllPingThreads(JNIEnv* env, jobject obj) {
	terminateAllPingThreads(); // also stores serverdata at disconnect()
}

extern "C" void Java_franqulator_cuefinger_Cuefinger_cleanUp(JNIEnv *env, jobject obj){
    cleanUp(); // also stores serverdata at disconnect()
}

#endif

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *lpCmdLine, int nCmdShow) {
	int argc = __argc;
	char** argv = __argv;
#else
int main(int argc, char* argv[]) {
#endif

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
	g_userEventBeginNum = SDL_RegisterEvents(USER_EVENT_COUNT);
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);

#ifdef __ANDROID__
    initDataPath(""); 
	initPrefPath("");
#else
	char* cs = SDL_GetBasePath();
	string path = string(cs);
	SDL_free(cs);
	size_t l = path.find_last_of("\\/", path.length() - 2);
	path = path.substr(0, l + 1);
	initDataPath(path + "data\\");

	cs = SDL_GetPrefPath("franqulator", "cuefinger");
	path = string(cs);
	SDL_free(cs);
	initPrefPath(path);
#endif	  

	writeLog(LOG_INFO, APP_NAME + " " + APP_VERSION);

	unsigned int startup_delay = 0; // in ms

	for (int n = 0; n < argc; n++) {
		string arg = argv[n];
		size_t split = arg.find_first_of('=', 0);

		if (split != string::npos) {
			string key = arg.substr(0, split);
			string value = arg.substr(split + 1);

			if (key == "delay") {
				startup_delay = (unsigned int)stoul(value);
			}
		}
	}

#ifdef _WIN32
	SetProcessDPIAware();
#endif

#ifndef __ANDROID__
	writeLog(LOG_INFO, "load settings");
	if (!g_settings.load())
	{
		writeLog(LOG_ERROR, "settings.load failed");

		SDL_Rect rc;
		if (SDL_GetDisplayBounds(0, &rc) != 0) {
			writeLog(LOG_ERROR, "SDL_GetDisplayBounds failed: " + string(SDL_GetError()));
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","SDL_GetDisplayBounds failed", NULL);
			return 1;       
		}
		
		g_settings.x = rc.w / 4;
		g_settings.y = rc.h / 4;
		g_settings.w=rc.w / 2;
		g_settings.h=rc.h / 2;						  
	}
#else
    SDL_Rect rc;
    if (SDL_GetDisplayBounds(0, &rc) != 0) {
		writeLog(LOG_ERROR, "SDL_GetDisplayBounds failed: " + string(SDL_GetError()));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","SDL_GetDisplayBounds failed", NULL);
        return 1;
    }

    g_settings.x = 0;
    g_settings.y = 0;
    g_settings.w=rc.w;
    g_settings.h=rc.h;
#endif
	g_settings.extended_logging = true;

	writeLog(LOG_INFO, "create gui objects");
	createStaticButtons();

#ifndef __ANDROID__
	writeLog(LOG_INFO, "Extended logging is deactivated.\nYou can activate it in settings.json but keep in mind that it will slow down performance.");
#endif

	SDL_Delay(startup_delay);

	updateChannelWidthButton();

	int flag = GFX_HARDWARE;
	if(g_settings.maximized)
	{
		flag |= GFX_MAXIMIZED;
	}
	if(g_settings.fullscreen)
	{
		flag |= GFX_FULLSCREEN_DESKTOP;
	}


	writeLog(LOG_INFO, "init gfx engine");
	try {
		gfx = new GFXEngine(WND_TITLE, g_settings.x, g_settings.y, g_settings.w, g_settings.h, flag, &g_window);
	}
	catch (invalid_argument&) {
		writeLog(LOG_ERROR, "InitGfx failed : " + string(SDL_GetError()));
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","InitGfx failed", NULL);
		return 1;
	}
	SDL_SetWindowMinimumSize(g_window, 320, 240);


	writeLog(LOG_INFO, "load gfx");
	if (loadAllGfx()) {

		writeLog(LOG_INFO, "update layout and load fonts");
        updateLayout();

#ifdef _WIN32
        InitNetwork();
#endif
        for (int n = 0; n < UA_MAX_SERVER_LIST; n++) {
            if (g_settings.serverlist[n].length() > 0) {
                g_ua_server_list.push_back(g_settings.serverlist[n]);
            }
        }
        if (g_ua_server_list.size()) // serverliste wurde definiert, automatische Serversuche wird nicht ausgeführt
        {
            g_serverlist_defined = true;
            updateConnectButtons();
        } else // Serverliste wurde nicht definiert, automatische Suche wird ausgeführt
        {
            g_serverlist_defined = false;
            getServerList();
        }
        bool running = true;
        SDL_Event e;

		writeLog(LOG_INFO, "start message loop");
        while (running) {
			if (SDL_WaitEventTimeout(&e, 33) > 0)
				//	while (SDL_PollEvent(&e) > 0)
			{
				switch (e.type) {
				case SDL_FINGERDOWN: {
					if (!g_btnSettings->isHighlighted()) {
						onTouchDown(NULL, &e.tfinger);
					}
					break;
				}
				case SDL_FINGERUP: {
					if (!g_btnSettings->isHighlighted()) {
						onTouchUp(false, &e.tfinger);
					}
					break;
				}
				case SDL_FINGERMOTION: {
					if (!g_btnSettings->isHighlighted()) {
						//only use last fingermotion event
						SDL_Event events[20];
						events[0] = e;
						SDL_PumpEvents();
						int ev_num = SDL_PeepEvents(&events[1], 19, SDL_GETEVENT, SDL_FINGERMOTION, SDL_FINGERMOTION) + 1;

						for (int n = ev_num - 1; n >= 0; n--) {
							bool drop_event = false;

							for (int i = n + 1; i < ev_num; i++) {
								if (events[n].tfinger.fingerId == events[i].tfinger.fingerId) {
									drop_event = true;
									break;
								}
							}

							if (!drop_event) {
								onTouchDrag(NULL, &events[n].tfinger);
							}
						}
					}
					break;
				}
				case SDL_MOUSEBUTTONDOWN: {
					if (e.button.button == SDL_BUTTON_LEFT) {
						bool handled = false;

						SDL_Point pt = { e.button.x, e.button.y };

						if (g_btnInfo->isHighlighted()) {
							g_btnInfo->setCheck(false);
							setRedrawWindow(true);
						}
						else {
							handled |= g_btnSettings->onPress(&pt);

							if (g_btnSettings->isHighlighted()) { // settings
								handled |= g_btnInfo->onPress(&pt);
								handled |= g_btnFullscreen->onPress(&pt);
								handled |= g_btnLockSettings->onPress(&pt);
								handled |= g_btnPostFaderMeter->onPress(&pt);
								handled |= g_btnShowOfflineDevices->onPress(&pt);
								handled |= g_btnLockToMixMIX->onPress(&pt);
								handled |= g_btnReconnect->onPress(&pt);
								for (int n = 0; n < 2; n++) {
									handled |= g_btnLockToMixAUX[n]->onPress(&pt);
								}
								for (int n = 0; n < 4; n++) {
									handled |= g_btnLockToMixCUE[n]->onPress(&pt);
									handled |= g_btnLabelAUX1[n]->onPress(&pt);
									handled |= g_btnLabelAUX2[n]->onPress(&pt);
								}
								handled |= g_btnServerlistScan->onPress(&pt);
								for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
									handled |= (*it)->onPress(&pt);
								}
							}
							else {
								if (g_msg.length()) {
									g_msg = "";
									setRedrawWindow(true);
								}
								handled |= g_btnMuteAll->onPress(&pt);
								//Click Mix/Send Buttons
								handled |= g_btnMix->onPress(&pt);
								for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
									handled |= itB->second->onPress(&pt);
								}
								//Click Connect Buttons
								for (size_t sel = 0; sel < g_btnConnect.size(); sel++) {
									handled |= g_btnConnect[sel]->onPress(&pt);
								}
								//Click Select Channels
								handled |= g_btnSelectChannels->onPress(&pt);
								//Click Reorder Channels
								handled |= g_btnReorderChannels->onPress(&pt);
								handled |= g_btnChannelWidth->onPress(&pt);
								handled |= g_btnPageLeft->onPress(&pt);
								handled |= g_btnPageRight->onPress(&pt);

								if (!handled) {
									SDL_Rect rc = { (int)g_channel_offset_x, 0 };
									SDL_GetWindowSize(g_window, &rc.w, &rc.h);
									rc.w -= (int)(2.0f * g_channel_offset_x);
									if (g_ua_server_connected.length() == 0 && SDL_PointInRect(&pt, &rc))//offline
									{
										if (!IS_UA_SERVER_REFRESHING) {
											getServerList();
										}
									}
									else {
										if (e.button.clicks == 2) {
											Vector2D cursor_pt = { (float)e.button.x, (float)e.button.y };

											Channel* channel = getChannelByPosition(cursor_pt);
											if (channel) {
												handled = true;
												if (!g_btnSelectChannels->isHighlighted() && !g_btnReorderChannels->isHighlighted()) {

													Vector2D relative_cursor_pt = cursor_pt;
													relative_cursor_pt.subtractX(g_channel_offset_x);
													relative_cursor_pt.setX(fmodf(relative_cursor_pt.getX(), g_channel_width));
													relative_cursor_pt.subtractY(g_channel_offset_y);

													if (channel->isTouchOnPan(&relative_cursor_pt)) {
														if (g_btnMix->isHighlighted()) {
															if (channel->stereo)
																channel->changePan(-1.0, true);
															else
																channel->changePan(0, true);
														}
														else {
															for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
																if (itB->second->isHighlighted()) {
																	Send* send = channel->getSendByName(itB->first);
																	if (send) {
																		send->changePan(0, true);
																	}
																	break;
																}
															}
														}
														setRedrawWindow(true);
													}
													else if (channel->isTouchOnPan2(&relative_cursor_pt)) {
														if (g_btnMix->isHighlighted()) {
															channel->changePan2(1.0, true);
														}
														setRedrawWindow(true);
													}
													else if (channel->isTouchOnFader(&relative_cursor_pt) && channel->type != MASTER) //level
													{
														if (g_btnMix->isHighlighted()) {
															channel->changeLevel(UNITY, true);
														}
														else {
															for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
																if (itB->second->isHighlighted()) {
																	Send* send = channel->getSendByName(itB->first);
																	if (send) {
																		send->changeLevel(UNITY, true);
																	}
																	break;
																}
															}
														}
														setRedrawWindow(true);
													}
												}
											}
										}
										if (e.button.which != SDL_TOUCH_MOUSEID) {
											Vector2D pt = Vector2D((float)e.button.x, (float)e.button.y);
											onTouchDown(&pt, NULL);
										}
									}
								}
							}
						}
					}
					break;
				}
				case SDL_MOUSEMOTION: {
					if (!g_btnSettings->isHighlighted()) {
						SDL_Event* p_event = &e;

						//only use last mousemotion event
						SDL_Event events[16];
						SDL_PumpEvents();
						int ev_num = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION);

						if (ev_num) {
							p_event = &events[ev_num - 1];
						}

						if (p_event->motion.which != SDL_TOUCH_MOUSEID) {
							if (p_event->motion.state & SDL_BUTTON_LMASK) {
								Vector2D pt = Vector2D((float)p_event->button.x, (float)p_event->button.y);
								onTouchDrag(&pt, NULL);
							}
						}
					}
					break;
				}
				case SDL_MOUSEBUTTONUP: {

					for (vector<Button*>::iterator it = g_btnConnect.begin(); it != g_btnConnect.end(); ++it) {
						(*it)->onRelease();
					}
					for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
						(*it).second->onRelease();
					}
					g_btnSelectChannels->onRelease();
					g_btnReorderChannels->onRelease();
					g_btnChannelWidth->onRelease();
					g_btnPageLeft->onRelease();
					g_btnPageRight->onRelease();
					g_btnMix->onRelease();
					g_btnSettings->onRelease();
					g_btnMuteAll->onRelease();

					g_btnLockSettings->onRelease();
					g_btnPostFaderMeter->onRelease();
					g_btnShowOfflineDevices->onRelease();
					g_btnLockToMixMIX->onRelease();
					for (int n = 0; n < 4; n++) {
						g_btnLockToMixCUE[n]->onRelease();
						g_btnLabelAUX1[n]->onRelease();
						g_btnLabelAUX2[n]->onRelease();
					}
					for (int n = 0; n < 2; n++) {
						g_btnLockToMixAUX[n]->onRelease();
					}
					g_btnReconnect->onRelease();
					g_btnServerlistScan->onRelease();
					g_btnInfo->onRelease();
					g_btnFullscreen->onRelease();

					for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
						(*it)->onRelease();
					}

					if (!g_btnSettings->isHighlighted()) {
						if (e.button.which != SDL_TOUCH_MOUSEID) {
							if (e.button.button == SDL_BUTTON_LEFT) {
								onTouchUp(true, NULL);
							}
						}
					}
					break;
				}
				case SDL_MOUSEWHEEL: {
					if (!g_btnSettings->isHighlighted() && !g_btnReorderChannels->isHighlighted() && !g_btnSelectChannels->isHighlighted()) {
						if (g_msg.length()) {
							g_msg = "";
							setRedrawWindow(true);
						}

						int x, y;
						SDL_GetMouseState(&x, &y);
						Vector2D cursor_pt = { (float)x, (float)y };

						Channel* channel = getChannelByPosition(cursor_pt);
						if (channel) {
							Vector2D relative_pos_pt = cursor_pt;
							relative_pos_pt.subtractX(g_channel_offset_x);
							relative_pos_pt.setX(fmodf(relative_pos_pt.getX(), g_channel_width));
							relative_pos_pt.subtractY(g_channel_offset_y);
							if (channel->isTouchOnFader(&relative_pos_pt) || channel->isTouchOnPan(&relative_pos_pt)
								|| channel->isTouchOnPan2(&relative_pos_pt)) {
								float move = (float)e.wheel.y * 20.0f;
								if (e.wheel.direction & SDL_MOUSEWHEEL_FLIPPED) {
									move = -move;
								}
								onTouchDown(&cursor_pt, NULL);
								cursor_pt.addX(move);
								cursor_pt.subtractY(move);
								onTouchDrag(&cursor_pt, NULL);
								onTouchUp(true, NULL);
							}
						}
					}
					break;
				}
				case SDL_KEYDOWN: {
					switch (e.key.keysym.sym) {
					case SDLK_ESCAPE: {
						SDL_SetWindowFullscreen(g_window, 0);
						g_btnFullscreen->setCheck(false);
						break;
					}
					case SDLK_f: {
						toggleFullscreen();
						break;
					}
					}
					break;
				}
				case SDL_DISPLAYEVENT: {
					if (e.display.event == SDL_DISPLAYEVENT_ORIENTATION) {
						int w, h;
						SDL_GetWindowSize(g_window, &w, &h);
						float win_width = (float)w;
						float win_height = (float)h;
						gfx->AbortUpdate();
						gfx->DrawShape(0, 0, 0, 0, win_width, win_height);
						gfx->Update();
					}
					break;
				}
				case SDL_WINDOWEVENT: {
					switch (e.window.event) {
					case SDL_WINDOWEVENT_EXPOSED: {
						setRedrawWindow(true);
						break;
					}
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					case SDL_WINDOWEVENT_RESIZED:
					case SDL_WINDOWEVENT_MAXIMIZED:
					case SDL_WINDOWEVENT_RESTORED: {
						gfx->Resize();
						updateLayout();
						updateMaxPages();
						updateSubscriptions();
						break;
					}
					}
					break;
				}
				case SDL_QUIT:
				case SDL_APP_TERMINATING: {
					running = false;
					break;
				}
				case SDL_USEREVENT: {
					switch (e.user.code - g_userEventBeginNum) {
					case EVENT_CONNECT:
						disconnect();
						if (g_ua_server_last_connection == -1 && g_btnConnect.size() > 0) {
							connect(g_btnConnect[0]->getId() - ID_BTN_CONNECT);
						}
						else {
							connect(g_ua_server_last_connection);
						}
						break;
					case EVENT_DISCONNECT:
						disconnect();
						break;
					case EVENT_DEVICES_INITIATE_RELOAD:
						if (!isLoading()) {
							cleanUpUADevices();
							setLoadingState(true);
							tcpClientSend("get /devices");
						}
						break;
					case EVENT_DEVICES_LOAD:
					{
						vector<string>* strDevices = (vector<string>*)e.user.data1;
						cleanUpUADevices();
						//load device info
						tcpClientSend("subscribe /devices/0/CueBusCount");
						for (vector<string>::iterator it = strDevices->begin(); it != strDevices->end(); ++it) {
							UADevice* dev = new UADevice(*it);
							if (dev) {
								g_ua_devices.push_back(dev);
								tcpClientSend("get /devices/" + dev->id + "/inputs");
							}
						}
						if (!g_ua_devices.empty()) {
							tcpClientSend("get /devices/0/auxs");
							tcpClientSend("get /devices/0/outputs");
						}
						SAFE_DELETE(strDevices);
						break;
					}
					case EVENT_SENDS_LOAD:
					{
						int *cueBusCount = (int*)e.user.data1;
						if (*cueBusCount + 2 != (int)g_btnSends.size()) {
							cleanUpSendButtons();

							// cues abfragen
							for (int n = 0; n < *cueBusCount; n++) {
								string name = "CUE " + to_string(n + 1);
								Button* btn = addSendButton(name);
								loadServerSettings(g_ua_server_connected, btn);
							}

							// 2 aux sends
							for (int n = 0; n < 2; n++) {
								string name = "AUX " + to_string(n + 1);
								Button* btn = addSendButton(name);
								loadServerSettings(g_ua_server_connected, btn);
							}
							setRedrawWindow(true);
						}
						break;
					}
					case EVENT_INPUTS_LOAD:
					{
						vector<string>* pIds = (vector<string>*)e.user.data1;
						string* devStr = (string*)e.user.data2;

						UADevice* dev = getDeviceByUAId(*devStr);
						if (dev) {
							//load device info
							for (vector<string>::iterator it = pIds->begin(); it != pIds->end(); ++it) {

								if (g_channelsById.find(dev->id + ".input." + *it) == g_channelsById.end()) {
									Channel* channel = new Channel(dev, *it, INPUT);
									g_channelsById.insert({ dev->id + ".input." + *it, channel });
									int order = (int)g_channelsInOrder.size();
									g_channelsInOrder.insert({ order, channel });
									channel->init();
									dev->channelsTotal++;

									size_t n = 2;
									for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
										n = n % g_btnSends.size();
										string sendId = to_string(n++);
										Send* send = new Send(channel, sendId);
										channel->sendsByName.insert({ it->first, send });
										channel->sendsById.insert({ sendId, send });
										send->init();
									}
								}
							}
							if (dev == g_ua_devices.back()) {
								loadServerSettings(g_ua_server_connected);
							}
						}
						SAFE_DELETE(pIds);
						break;
					}
					case EVENT_AUXS_LOAD:
					{
						vector<string>* pIds = (vector<string>*)e.user.data1;
						string* devStr = (string*)e.user.data2;

						UADevice* dev = getDeviceByUAId(*devStr);
						if (dev && dev->id == "0") {
							//load device info
							for (vector<string>::iterator it = pIds->begin(); it != pIds->end(); ++it) {

								if (g_channelsById.find(dev->id + ".aux." + *it) == g_channelsById.end()) {
									Channel* channel = new Channel(dev, *it, AUX);
									g_channelsById.insert({ dev->id + ".aux." + *it, channel });
									int order = (int)g_channelsInOrder.size() + 1024; // ans ende sortieren
									g_channelsInOrder.insert({ order, channel });
									channel->init();

									size_t n = 0;
									for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
										if (n >= g_btnSends.size() - 2) {
											break;
										}
										else {
											n = n % g_btnSends.size();
										}
										string sendId = to_string(n++);
										Send* send = new Send(channel, sendId);
										channel->sendsByName.insert({ it->first, send });
										channel->sendsById.insert({ sendId, send });
										send->init();
									}
								}
							}
						}
						SAFE_DELETE(pIds);
						break;
					}
					case EVENT_OUTPUTS_LOAD:
					{
						vector<string>* pIds = (vector<string>*)e.user.data1;
						string* devStr = (string*)e.user.data2;

						UADevice* dev = getDeviceByUAId(*devStr);
						if (dev && dev->id == "0") {
							//load device info
							for (vector<string>::iterator it = pIds->begin(); it != pIds->end(); ++it) {

								if (g_channelsById.find(dev->id + ".master." + *it) == g_channelsById.end()) {
									Channel* channel = new Channel(dev, *it, MASTER);
									g_channelsById.insert({ dev->id + ".master." + *it, channel });
									int order = (int)g_channelsInOrder.size() + 2048; // ans ende sortieren
									g_channelsInOrder.insert({ order, channel });
									channel->init();
								}
							}
						}
						SAFE_DELETE(pIds);
						break;
					}
					case EVENT_CHANNEL_STATE_CHANGED:
					{
						g_activeChannelsCount = -1;
						g_visibleChannelsCount = -1;
						browseToSelectedChannel(g_browseToChannel);
						updateSubscriptions();
						updateMaxPages();
						setRedrawWindow(true);
						break;
					}
					case EVENT_UPDATE_SUBSCRIPTIONS:
						updateSubscriptions();
						break;
					case EVENT_POST_FADER_METERING:
						g_btnPostFaderMeter->setEnable(!g_settings.lock_settings);
						g_btnPostFaderMeter->setCheck((bool)e.user.data1);
						setRedrawWindow(true);
						break;
					case EVENT_AFTER_UPDATE_PROPERTIES:
						updateMaxPages();
						updateSubscriptions();
						setRedrawWindow(true);
						break;
					case EVENT_DEVICE_ONLINE:
					{
						string* devId = (string*)e.user.data1;
						UADevice* dev = getDeviceByUAId(*devId);
						if (dev)
						{
							dev->online = (bool)e.user.data2;
							pushEvent(EVENT_UPDATE_SUBSCRIPTIONS);
						}
						SAFE_DELETE(devId);
						break;
					}
					case EVENT_RESET_ORDER:
					{
						g_channelsInOrder.clear();
						int n = 0;
						for (vector<UADevice*>::iterator it = g_ua_devices.begin(); it != g_ua_devices.end(); ++it) {
							for (int i = 0; i < (*it)->channelsTotal; i++) {
								Channel* channel = getChannelByUAIds((*it)->id, to_string(i), INPUT);
								if (channel) {
									g_channelsInOrder.insert({ n, channel });
									n++;
								}
							}
						}
						for (int i = 0; i < 2; i++) {
							Channel* channel = getChannelByUAIds(g_ua_devices.front()->id, to_string(i), AUX);
							if (channel) {
								g_channelsInOrder.insert({ n, channel });
								n++;
							}
						}
						Channel* channel = getChannelByUAIds(g_ua_devices.front()->id, g_masterChannelId, MASTER);
						if (channel) {
							g_channelsInOrder.insert({ n, channel });
						}
						g_btnReorderChannels->setCheck(false);
						updateSubscriptions();
						setRedrawWindow(true);
						break;
					}
					}
					break;
				}
				default:
					continue;
				}
            }

			if (getRedrawWindow()) {
                setRedrawWindow(false);
                draw();
            }
        }
    }
    else
    {
		writeLog(LOG_ERROR, "loadGfx failed");
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","loadGfx failed", NULL);
    }

#ifndef __ANDROID__
		if(!g_settings.lock_settings)
			g_settings.save();
#endif
    cleanUp();

	return 0;
}

void cleanUp() {
	writeLog(LOG_INFO, "terminate threads");
	terminateAllPingThreads();
    disconnect();

#ifdef _WIN32
    ReleaseNetwork();
#endif

	writeLog(LOG_INFO, "clean up gui objects");
    cleanUpConnectionButtons();
    cleanUpStaticButtons();

	writeLog(LOG_INFO, "clean up fonts and gfx");
    gfx->DeleteFont(g_fntMain);
    gfx->DeleteFont(g_fntInfo);
    gfx->DeleteFont(g_fntChannelBtn);
    gfx->DeleteFont(g_fntLabel);
    gfx->DeleteFont(g_fntFaderScale);
    gfx->DeleteFont(g_fntOffline);
    releaseAllGfx();

    SAFE_DELETE(gfx);
}