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
#include <mutex>

std::mutex g_draw_mutex;
//mutex g_redraw_mutex;

#define EVENT_CONNECT		1
#define EVENT_DISCONNECT	2

GFXEngine *gfx = NULL;

SDL_Window *g_window=NULL;
SDL_TimerID g_timer_network_serverlist = 0;
SDL_TimerID g_timer_network_reconnect = 0;
SDL_TimerID g_timer_network_timeout = 0;
SDL_TimerID g_timer_network_send_timeout_msg = 0;

Settings g_settings;
map<string, string> serverSettingsJSON;

string g_msg = "";

vector<string> g_ua_server_list;
string g_ua_server_connected = "";
int g_ua_server_last_connection = -1;

int g_browseToChannel = 0;

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
float g_channel_slider_height;

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
//Button* g_btnLockToMixHP = NULL;
Button* g_btnReconnectOn = NULL;

Button* g_btnServerlistScan = NULL;

Button* g_btnInfo = NULL;

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
GFXSurface *g_gsFader;
GFXSurface *g_gsPan;
GFXSurface *g_gsPanPointer;
GFXSurface *g_gsLabel[4];

vector<SDL_Thread*> pingThreadsList;

unsigned long long g_server_refresh_start_time = -UA_SERVER_RESFRESH_TIME;
vector<UADevice*> g_ua_devices;
unordered_map<string, Channel*> g_channelsById;
map<int, Channel*> g_channelsInOrder;
set<string> g_channelsMutedBeforeAllMute;


string json_workaround_secure_unicode_characters(string input)
{
	int sz = 0;
	do
	{
		sz = input.find("\\u", sz);
		if (sz == -1)
			return input;

		input.replace(sz, 2,  "\\\\u");
		sz += 3;
	} while (true);

	return input;
}

string unescape_to_utf8(string input)
{
	int sz = 0;
	do
	{
		sz = input.find("\\u", sz);
		if (sz == -1)
			return input;

		string num = input.substr(sz+2, 4);

		char c = strtol(num.c_str(), NULL, 16);

		input.replace(sz, 6, 1, c);
		sz++;
	} while (true);

	return input;
}

int calculateMinChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 3.0f;
	int channels = floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int calculateAverageChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 5.0f;
	int channels = floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int calculateMaxChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 9.0f;
	int channels = floor(mixer_width / max_channel_width);

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

void updateMaxPages() {
	int max_pages = (getAllChannelsCount(false) - 1) / calculateChannelsPerPage();
	if (g_page >= max_pages) {
		g_page = max_pages;
		g_browseToChannel = getMiddleSelectedChannel();
	}
}

void updateLayout() {

	int channels_per_page = calculateChannelsPerPage();
	
	int w,h;
	SDL_GetWindowSize(g_window, &w, &h);
	float win_width = (float)w;
	float win_height = (float)h;
	/*
	int smaller_btn_height = win_height /7;
	if (smaller_btn_height < g_btn_height)
		g_btn_height = smaller_btn_height;
	*/

	g_btn_width = (win_width / (2.0f + channels_per_page)) / 1.2f;
	if (g_btn_width < MIN_BTN_WIDTH)
		g_btn_width = MIN_BTN_WIDTH;
	if (g_btn_width > MAX_BTN_WIDTH)
		g_btn_width = MAX_BTN_WIDTH;

	g_btn_height = win_height / 10.0f;

	if (g_btn_height > g_btn_width)
		g_btn_height = g_btn_width;

	g_channel_width = ((win_width - 2.0f * (g_btn_width * 1.2f + SPACE_X)) / channels_per_page);
	g_channel_height = win_height / 1.02f;

	g_channel_offset_x = (win_width - channels_per_page * g_channel_width) / 2.0f;
	g_channel_offset_y = (win_height - g_channel_height) / 2.0f;

	g_channel_btn_size = g_channel_width * 0.4f;
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
	g_btnSettings->rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
	g_btnSettings->rc.y = round(g_channel_offset_y);
	g_btnSettings->rc.w = round(g_btn_width);
	g_btnSettings->rc.h = round(g_btn_height);

	g_btnMuteAll->rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
	g_btnMuteAll->rc.y = round(g_channel_offset_y + g_btn_height + SPACE_Y);
	g_btnMuteAll->rc.w = round(g_btn_width);
	g_btnMuteAll->rc.h = round(g_btn_height);

	float sends_offset_y = (win_height - g_channel_offset_y + g_btn_height - (g_btn_height + SPACE_Y) * g_btnSends.size()) / 2.0f;
	
	int n = 0;
	for(vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		it->second->rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
		it->second->rc.y = round(sends_offset_y + (g_btn_height + SPACE_Y) * (n++));
		it->second->rc.w = round(g_btn_width);
		it->second->rc.h = round(g_btn_height);
	}

	g_btnMix->rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
	g_btnMix->rc.y = round(win_height - g_channel_offset_y - g_btn_height);
	g_btnMix->rc.w = round(g_btn_width);
	g_btnMix->rc.h = round(g_btn_height);

	//btnleiste rechts
	for (int n = 0; n < g_btnConnect.size(); n++)
	{
		g_btnConnect[n]->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
		g_btnConnect[n]->rc.y = round(g_channel_offset_y + n * (g_btn_height + SPACE_Y));
		g_btnConnect[n]->rc.w = round(g_btn_width);
		g_btnConnect[n]->rc.h = round(g_btn_height);
	}

	g_btnSelectChannels->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
	g_btnSelectChannels->rc.y = round(win_height / 2.0f - (g_btn_height + SPACE_Y / 2.0f));
	g_btnSelectChannels->rc.w = round(g_btn_width);
	g_btnSelectChannels->rc.h = round(g_btn_height);

	g_btnReorderChannels->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
	g_btnReorderChannels->rc.y = round(win_height / 2.0f - (g_btn_height + SPACE_Y / 2.0f)) + g_btn_height + SPACE_Y;
	g_btnReorderChannels->rc.w = round(g_btn_width);
	g_btnReorderChannels->rc.h = round(g_btn_height);

	g_btnChannelWidth->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
	g_btnChannelWidth->rc.y = round(win_height - g_channel_offset_y - 3.0f * g_btn_height - g_main_fontsize - SPACE_Y * 3.0f);
	g_btnChannelWidth->rc.w = round(g_btn_width);
	g_btnChannelWidth->rc.h = round(g_btn_height);

	g_btnPageLeft->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
	g_btnPageLeft->rc.y = round(win_height - g_channel_offset_y - 2.0f * g_btn_height - SPACE_Y);
	g_btnPageLeft->rc.w = round(g_btn_width);
	g_btnPageLeft->rc.h = round(g_btn_height);

	g_btnPageRight->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
	g_btnPageRight->rc.y = round(win_height - g_channel_offset_y - g_btn_height);
	g_btnPageRight->rc.w = round(g_btn_width);
	g_btnPageRight->rc.h = round(g_btn_height);

	g_btnLockSettings->rc.w = round(g_btn_width);
	g_btnLockSettings->rc.h = round(g_btn_height / 2.0f);

	g_btnPostFaderMeter->rc.w = round(g_btn_width);
	g_btnPostFaderMeter->rc.h = round(g_btn_height / 2.0f);

	g_btnShowOfflineDevices->rc.w = round(g_btn_width);
	g_btnShowOfflineDevices->rc.h = round(g_btn_height / 2.0f);

	g_btnLockToMixMIX->rc.w = round(g_btn_width / 2.0f);
	g_btnLockToMixMIX->rc.h = round(g_btn_height / 2.0f);
//	g_btnLockToMixHP->rc.w = round(g_btn_width);
//	g_btnLockToMixHP->rc.h = round(g_btn_height / 2.0f);

	for (int n = 0; n < 2; n++) {
		g_btnLockToMixAUX[n]->rc.w = round(g_btn_width / 2.0f);
		g_btnLockToMixAUX[n]->rc.h = round(g_btn_height / 2.0f);
	}
	for (int n = 0; n < 4; n++) {
		g_btnLockToMixCUE[n]->rc.w = round(g_btn_width / 2.0f);
		g_btnLockToMixCUE[n]->rc.h = round(g_btn_height / 2.0f);

		g_btnLabelAUX1[n]->rc.w = round(g_btn_width / 2.0f);
		g_btnLabelAUX1[n]->rc.h = round(g_btn_height / 2.0f);

		g_btnLabelAUX2[n]->rc.w = round(g_btn_width / 2.0f);
		g_btnLabelAUX2[n]->rc.h = round(g_btn_height / 2.0f);
	}

	g_btnReconnectOn->rc.w = round(g_btn_width);
	g_btnReconnectOn->rc.h = round(g_btn_height / 2.0f);

	g_btnServerlistScan->rc.w = round(g_btn_width);
	g_btnServerlistScan->rc.h = round(g_btn_height / 2.0f);

	g_btnInfo->rc.x = g_channel_offset_x + 20.0f;
	g_btnInfo->rc.y = 20.0f;
	g_btnInfo->rc.w = round(g_btn_width);
	g_btnInfo->rc.h = round(g_btn_height / 2.0f);

	for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
		(*it)->rc.w = round(g_btn_width);
		(*it)->rc.h = round(g_btn_height / 2.0f);
	}

	browseToSelectedChannel(g_browseToChannel);
	g_browseToChannel = getMiddleSelectedChannel();
}

Button::Button(int _id, std::string _text, int _x, int _y, int _w, int _h,
					bool _checked, bool _enabled, bool _visible)
{
	this->id=_id;
	this->text=_text;
	this->rc.x=_x;
	this->rc.y=_y;
	this->rc.w=_w;
	this->rc.h=_h;
	this->checked=_checked;
	this->enabled=_enabled;
	this->visible=_visible;
}

bool Button::isClicked(SDL_Point *pt)
{
	if(this->visible && this->enabled)
	{
		return (bool)SDL_PointInRect(pt, &this->rc);
	}
	return false;
}

void Button::draw(int color, string overrideName)
{
	if (!this->visible)
		return;

	int btn_switch = 0;
	if (this->checked)
	{
		btn_switch = 1;
	}

//	DrawColor(x, y, width, height, bgClr);
	Vector2D stretch = Vector2D(this->rc.w, this->rc.h);

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
	if (!this->enabled) {
		gfx->SetFilter_Saturation(gs, -0.1f);
		gfx->SetFilter_Brightness(gs, 0.9f);
		gfx->SetFilter_Contrast(gs, 0.7f);
		clr = RGB(70, 70, 70);
	}
	gfx->Draw(gs, this->rc.x, this->rc.y, NULL, GFX_NONE, 1.0f, 0, NULL, &stretch);

	gfx->SetFilter_Saturation(gs, 0.0f);
	gfx->SetFilter_Brightness(gs, 1.0f);
	gfx->SetFilter_Contrast(gs, 1.0f);

	string text = overrideName.length() == 0 ? this->text : overrideName;

	float max_width = (float)this->rc.w * 0.9;
	Vector2D sz = gfx->GetTextBlockSize(g_fntMain, text, GFX_CENTER | GFX_AUTOBREAK, max_width);
	Vector2D szText(max_width, this->rc.h);
	gfx->SetColor(g_fntMain, clr);
	gfx->Write(g_fntMain, this->rc.x + ((float)this->rc.w - max_width) / 2.0f, this->rc.y + this->rc.h / 2.0f - sz.getY() / 2.0f, text, GFX_CENTER | GFX_AUTOBREAK, &szText, 0, 0.8);
}

Touchpoint::Touchpoint() {
	memset(this, 0, sizeof(Touchpoint));
}

Send::Send(Channel *channel, string id)
{
	this->init();
	this->id = id;
	this->channel = channel;

	string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";
	string cmd = "subscribe /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Bypass/";
	tcpClientSend(cmd.c_str());
}

Send::~Send() {
	string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";
	string cmd = "unsubscribe /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Bypass/";
	tcpClientSend(cmd.c_str());

	if (this->subscribed) {
		this->updateSubscription(false);
	}
}

void Send::init()
{
	this->id = "";
//	this->name = "";
	this->gain = fromDbFS(-144.0);
	this->pan = 0;
	this->bypass = false;
	this->channel = NULL;
	this->meter_level = fromDbFS(-144.0);
	this->meter_level2 = fromDbFS(-144.0);
	this->subscribed = false;
	this->clip = false;
	this->clip2 = false;
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

void Send::changeGain(double gain_change, bool absolute)
{
	double _gain = fromMeterScale(toMeterScale(this->gain) + gain_change);
	if (absolute)
		_gain = gain_change;

	if (_gain > 4.0)
	{
		_gain = 4.0;
	}
	else if (_gain < fromDbFS(-144.0))
	{
		_gain = fromDbFS(-144.0);
	}

	if (this->gain != _gain)
	{
		this->gain = _gain;

		string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";

		string msg = "set /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Gain/value/ " + to_string(toDbFS(this->gain));
		tcpClientSend(msg.c_str());
	}
}

void Send::pressBypass(int state)
{
	if (state == SWITCH)
		this->bypass = !this->bypass;
	else
		this->bypass = (bool)state;

	string value = "true";
	if (!this->bypass)
		value = "false";

	string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";

	string msg = "set /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id + "/Bypass/value/ " + value;
	tcpClientSend(msg.c_str());
}


void Send::updateSubscription(bool subscribe)
{
	if (this->subscribed != subscribe) {

		this->subscribed = subscribe;

		string type_str = this->channel->type == INPUT ? "/inputs/" : "/auxs/";

		string root = "";
		if (!subscribe) {
			root += "un";
		}
		root += "subscribe /devices/" + this->channel->device->id + type_str + this->channel->id + "/sends/" + this->id;

		string cmd = root + "/Gain/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/Pan/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/0/MeterLevel/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/0/MeterClip/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/1/MeterLevel/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/1/MeterClip/";
		tcpClientSend(cmd.c_str());

		//	cmd = root + "/Pan2/";
		//	tcpClientSend(cmd.c_str());

		//	cmd = "get /devices/" + this->device->id + "/inputs/" + this->id + "/sends/" + to_string(n);
		//	tcpClientSend(cmd.c_str());
	}
}

Channel::Channel(UADevice *device, string id, int type) {

	this->init();

	this->label_gfx = rand() % 4;
	this->label_rotation = (float)(rand() % 100) / 2000 - 0.025;
	this->id = id;
	this->device = device;
	this->type = type;

	string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";

	string root = "subscribe /devices/" + this->device->id + type_str  + this->id;

	string cmd = root + "/Stereo/";
	tcpClientSend(cmd.c_str());

	cmd = root + "/Name/";
	tcpClientSend(cmd.c_str());

	//get mute state outside of updateSubscriptions for 'Mute All' to work
	cmd = root + "/Mute/";
	tcpClientSend(cmd.c_str());

	if (this->type == INPUT) {
		cmd = root + "/StereoName/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/ChannelHidden/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/EnabledByUser/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/Active/";
		tcpClientSend(cmd.c_str());
	}
	else {
		this->active = true;
		this->hidden = false;
		this->enabledByUser = true;
	}

	root = "get /devices/" + this->device->id + type_str + this->id;
	cmd = root + "/sends";
	tcpClientSend(cmd.c_str());
}

Channel::~Channel()
{
	string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";

	string cmd = "unsubscribe /devices/" + this->device->id + type_str + this->id + "/Mute/";
	tcpClientSend(cmd.c_str());

	if (this->subscribed) {
		this->updateSubscription(false);
	}

	for(unordered_map<string, Send*>::iterator it = this->sendsByName.begin(); it != this->sendsByName.end(); ++it) {
		SAFE_DELETE(it->second);
	}
	this->sendsById.clear();
	this->sendsByName.clear();
}

bool Channel::isVisible(bool only_selected) {

	if (only_selected && !this->selected_to_show || !this->device->online && !g_btnShowOfflineDevices->checked) {
		return false;
	}

	return (!this->hidden && (!this->stereo || (stoi(this->id, NULL, 10) % 2) == 0 || this->type == AUX)
		&& this->enabledByUser && this->active);
}

void Channel::init()
{
	this->id = "";
	this->name = "";
	this->level = fromDbFS(-144.0);
	this->pan = 0;
	this->solo = false;
	this->mute = false;
	this->post_fader = false;
	memset(&this->touch_point,0,sizeof(Touchpoint));
	this->device = NULL;
	this->stereo = false;
	this->stereoname = "";
	this->pan2 = 0;
	this->hidden = true;
	this->enabledByUser = false;
	this->active = false;
	this->selected_to_show = true;

	this->fader_group = 0;

	this->meter_level = fromDbFS (-144.0);
	this->meter_level2 = fromDbFS(-144.0);

	this->subscribed = false;

	this->clip = false;
	this->clip2 = false;
}

bool Channel::isTouchOnFader(Vector2D *pos)
{
	int y = (g_fader_label_height + g_channel_pan_height + g_channel_btn_size);
	int height = g_channel_height - y - g_channel_btn_size;
	return (pos->getX() > 0 && pos->getX() < g_channel_width
		&& pos->getY() >y && pos->getY() < y + height);
}

bool Channel::isTouchOnPan(Vector2D *pos)
{
	int width = g_channel_width;

	if (this->stereo)
		width /= 2;

	return (pos->getX() > 0 && pos->getX() < width
		&& pos->getY() > g_fader_label_height && pos->getY() < (g_fader_label_height + g_channel_pan_height));
}

bool Channel::isTouchOnPan2(Vector2D *pos)
{
	int width = g_channel_width / 2;

	return (pos->getX() > width && pos->getX() < g_channel_width
		&& pos->getY() > g_fader_label_height && pos->getY() < (g_fader_label_height + g_channel_pan_height));
}

bool Channel::isTouchOnGroup1(Vector2D *pos)
{
	int x = (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_channel_height - g_channel_btn_size;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnGroup2(Vector2D *pos)
{
	int x = g_channel_width / 2 + (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_channel_height - g_channel_btn_size;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnMute(Vector2D *pos)
{
	int x = g_channel_width / 2 + (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnSolo(Vector2D *pos)
{
	if (this->type == AUX)
		return false;

	int x = (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::isTouchOnPostFader(Vector2D* pos)
{
	if (this->type != AUX)
		return false;

	int x = (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_fader_label_height + g_channel_pan_height;

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

	char msg[256];
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
	double _level = fromMeterScale(toMeterScale(this->level) + level_change);

	if (absolute)
		_level = level_change;

	if (_level > 4.0)
	{
		_level = 4.0;
	}
	else if (_level < fromDbFS(-144.0))
	{
		_level = fromDbFS(-144.0);
	}

	if (this->level != _level)
	{
		this->level = _level;

		string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
		string msg = "set /devices/" + this->device->id + type_str + this->id + "/FaderLevel/value/ " + to_string(toDbFS(_level));
		tcpClientSend(msg.c_str());
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

	string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";
	string msg = "set /devices/" + this->device->id + type_str + this->id + "/Mute/value/ " + value;
	tcpClientSend(msg.c_str());
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

	size_t pos = name.find_last_of("?");
	if (pos != string::npos) {
		this->properties = name.substr(pos + 1);
	}
	else {
		this->properties = "";
	}

	if (this->isOverriddenShow()) {
		this->selected_to_show = true;
	}
	else if (this->isOverriddenHide()) {
		this->selected_to_show = false;
		updateMaxPages();
	}
	updateSubscriptions();
	setRedrawWindow(true);
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

void Channel::setName(string name) {
	this->name = name;
	this->updateProperties();
}

void Channel::setStereoname(string stereoname) {
	this->stereoname = stereoname;
	this->updateProperties();
}

void Channel::setStereo(bool stereo) {
	this->stereo = stereo;
	this->updateProperties();
}

bool Channel::getColor(unsigned int *color) {
	if (color) {
		if (this->type == AUX) {
			*color = RGB(0, 70, 200);
			return true;
		}
		else {
			if (properties.length() > 0) {
				if (properties.find("r") != string::npos || properties.find("R") != string::npos) {
					*color = RGB(150, 10, 10);
					return true;
				}
				else if (properties.find("g") != string::npos || properties.find("G") != string::npos) {
					*color = RGB(00, 140, 20);
					return true;
				}
				else if (properties.find("b") != string::npos || properties.find("B") != string::npos) {
					*color = RGB(00, 70, 200);
					return true;
				}
				else if (properties.find("y") != string::npos || properties.find("Y") != string::npos) {
					*color = RGB(180, 180, 10);
					return true;
				}
				else if (properties.find("p") != string::npos || properties.find("P") != string::npos) {
					*color = RGB(100, 30, 180);
					return true;
				}
				else if (properties.find("o") != string::npos || properties.find("O") != string::npos) {
					*color = RGB(200, 100, 30);
					return true;
				}
			}
		}
	}
	return false;
}

bool Channel::isOverriddenShow() {
	return (properties.length() > 0 && properties.find("s") != string::npos || properties.find("S") != string::npos);
}

bool Channel::isOverriddenHide() {
	return (properties.length() > 0 && properties.find("h") != string::npos || properties.find("H") != string::npos);
}

void Channel::updateSubscription(bool subscribe)
{
	bool subscribeMix = subscribe && g_btnMix->checked;

	if (this->subscribed != subscribeMix) {

		this->subscribed = subscribeMix;

		string type_str = this->type == INPUT ? "/inputs/" : "/auxs/";

		string root = "";
		if (!subscribeMix) {
			root += "un";
		}
		root += "subscribe /devices/" + this->device->id + type_str + this->id;

		string cmd = root + "/FaderLevel/";
		tcpClientSend(cmd.c_str());

		if (this->type != AUX) {
			cmd = root + "/Solo/";
			tcpClientSend(cmd.c_str());
		}
		else {
			cmd = root + "/SendPostFader/";
			tcpClientSend(cmd.c_str());
		}

		cmd = root + "/Pan/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/Pan2/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/0/MeterLevel/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/0/MeterClip/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/1/MeterLevel/";
		tcpClientSend(cmd.c_str());

		cmd = root + "/meters/1/MeterClip/";
		tcpClientSend(cmd.c_str());
	}

	for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
		unordered_map<string, Send*>::iterator itS = this->sendsByName.find((*itB).first);
		if (itS != this->sendsByName.end()) {
			bool subscribeSend = subscribe && (*itB).second->checked;
			itS->second->updateSubscription(subscribeSend);
		}
	}
}

UADevice::UADevice(string us_deviceId) {
	this->online = false;
	this->id = us_deviceId;

	//more device info
	//	tcpClientSend("get /devices/" + this->id + "/");

	string msg = "subscribe /devices/" + this->id + "/DeviceOnline/";
	tcpClientSend(msg.c_str());

	//	msg = "subscribe /devices/" + this->id;
	//	tcpClientSend(msg.c_str());

	//	msg = "subscribe /MeterPulse/";
	//	tcpClientSend(msg.c_str());
}
UADevice::~UADevice() {
}

int getActiveChannelsCount(int flag)
{
	if (g_channelsInOrder.empty()) {
		return 0;
	}

	int visibleChannelsCount = 0;
	int activeChannelsCount = 0;

	bool btn_select = g_btnSelectChannels->checked;

	for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
		if (!it->second->enabledByUser || !it->second->active)
			continue;
		if (it->second->isVisible(!btn_select))
			visibleChannelsCount++;
		activeChannelsCount++;
	}

	if(flag==UA_VISIBLE)
		return visibleChannelsCount;

	return activeChannelsCount;
}

void clearChannels() {

	for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
		it->second->updateSubscription(false);
		SAFE_DELETE(it->second);
	}
	g_channelsById.clear();
	g_channelsInOrder.clear();
}

void updateSubscriptions() {

	int count = 0;
	int channels_per_page = calculateChannelsPerPage();

	bool btn_select = g_btnSelectChannels->checked;

	for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
		bool subscribe = false;
		if (it->second->isVisible(!btn_select)) {
			if (count >= g_page * channels_per_page && count < (g_page + 1) * channels_per_page) {
				subscribe = true;
			}
			count++;
		}
		it->second->updateSubscription(subscribe);
	}
}

int getAllChannelsCount(bool countWithHidden)
{
	if (countWithHidden)
		return getActiveChannelsCount(UA_ALL_ENABLED_AND_ACTIVE);
	else
		return getActiveChannelsCount(UA_VISIBLE);

	return 0;
}

UADevice *getDeviceByUAId(string ua_device_id)
{
	if (ua_device_id.length() == 0)
		return NULL;

	for (int n = 0; n < g_ua_devices.size(); n++)
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
	string type_str = (type == INPUT) ? ".input." : ".aux.";

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

	bool btn_select = g_btnSelectChannels->checked;

	int index = 0;
	for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
		if (!it->second->isVisible(!btn_select))
			continue;

		if (touch_input && it->second->touch_point.id == touch_input->touchId
			&& it->second->touch_point.finger_id == touch_input->fingerId
			|| _is_mouse && it->second->touch_point.is_mouse)
		{
			return it->second;
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
		double channelIndex = (int)floor(pt.getX() / g_channel_width);

		int channels_per_page = calculateChannelsPerPage();
		bool btn_select = g_btnSelectChannels->checked;

		if (channelIndex >= 0 && channelIndex < channels_per_page)
		{
			channelIndex += g_page * channels_per_page;
			Channel* channel = NULL;
			int visibleChannelsCount = getActiveChannelsCount(UA_VISIBLE);
			if (channelIndex < visibleChannelsCount)
			{
				int n = 0;
				for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {

					channel = it->second;
					if (!it->second->isVisible(!btn_select))
					{
						channelIndex++;
					}
					if (n >= channelIndex || channelIndex >= g_channelsInOrder.size()) {
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
		for (int n = 0; n < g_ua_server_list.size(); n++)
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

			toLog("UA-Server found on " + g_ua_server_list.back());
		}

		SAFE_DELETE(tcpClient);
	}
	catch (string error) {
		if (g_settings.extended_logging) {
			toLog("No UA-Server found on " + ip);
		}
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
				if (g_settings.extended_logging) {
					toLog("Error in UA_GetServerListCallback at SDL_CreateThread " + ip);
				}
			}
			else {
				pingThreadsList.push_back(thread);
			}
		}
		else {
			if (g_settings.extended_logging) {
				toLog("Error in UA_GetServerListCallback at SDL_CreateThread " + ip + "(out of memory)");
			}
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
					if (g_settings.extended_logging) {
						toLog("Error in UA_GetServerListCallback at SDL_CreateThread " + ip);
					}
				}
				else {
					pingThreadsList.push_back(thread);
				}
			}
			else {
				if (g_settings.extended_logging) {
					toLog("Error in UA_GetServerListCallback at SDL_CreateThread " + ip + "(out of memory)");
				}
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
	for (int n = 0; n < g_btnConnect.size(); n++) {
		SAFE_DELETE(g_btnConnect[n]);
	}
	g_btnConnect.clear();
}

void cleanUpSendButtons()
{
	g_draw_mutex.lock();

	for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		SAFE_DELETE(it->second);
	}
	g_btnSends.clear();

	g_draw_mutex.unlock();
}

void setRedrawWindow(bool redraw)
{
//	const lock_guard<mutex> lock(g_redraw_mutex);
	g_redraw = redraw;
}

bool getRedrawWindow()
{
//	const lock_guard<mutex> lock(g_redraw_mutex);
	return g_redraw;
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
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		event.type = SDL_USEREVENT;
		event.user.code = EVENT_CONNECT;
		SDL_PushEvent(&event);
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
	toLog(g_msg);
	disconnect();

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
	return; //DEBUG
#ifndef SIMULATION
	
	if (g_timer_network_send_timeout_msg != 0) {
		SDL_RemoveTimer(g_timer_network_send_timeout_msg);
	}
	g_timer_network_send_timeout_msg = SDL_AddTimer(5000, timerCallbackNetworkSendTimeoutMsg, NULL);

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

void tcpClientProc(int msg, string data)
{
	if (!g_tcpClient)
		return;

	switch (msg)
	{
	case MSG_CLIENT_CONNECTED:
//		OutputDebugString("Connected");
		break;
	case MSG_CLIENT_DISCONNECTED:
//		OutputDebugString("Disconnected");
		break;
	case MSG_CLIENT_CONNECTION_LOST:
	{
		// try to reconnect
		if (g_settings.reconnect_time && g_timer_network_reconnect == 0) {
			g_timer_network_reconnect = SDL_AddTimer(g_settings.reconnect_time, timerCallbackReconnect, NULL);
		}

		//send message disconnect
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		event.type = SDL_USEREVENT;
		event.user.code = EVENT_DISCONNECT;
		SDL_PushEvent(&event);
		break;
	}
	case MSG_TEXT:
	{
		setNetworkTimeout();

		string tcp_msg = json_workaround_secure_unicode_characters(data);

		if(g_settings.extended_logging) toLog("UA <- " + tcp_msg);
		
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
			int i = 0;
			int lpos = 1;
			do
			{
				int rpos = path.find_first_of('/', lpos);
				if (rpos != -1)
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
			} while (lpos != -1 && i < 12);

			//daten anhand path_parameter verarbeiten
			if (path_parameter[0] == "Session") // devices laden
			{	
				cleanUpUADevices();
				tcpClientSend("get /devices");
			}
			else if (path_parameter[0] == "PostFaderMetering") // devices laden
			{
				g_btnPostFaderMeter->enabled = !g_settings.lock_settings;
				g_btnPostFaderMeter->checked = element["data"];
				setRedrawWindow(true);
			}
			else if (path_parameter[0] == "devices") // devices laden
			{
				if (path_parameter[1].length() == 0)
				{
					int i = 0;
					const dom::object obj = element["data"]["children"];

					cleanUpUADevices();

					//load device info

					string msg = "subscribe /devices/0/CueBusCount";
					tcpClientSend(msg.c_str());
				
					for (dom::object::iterator it = obj.begin(); it != obj.end(); ++it) {
						string id { it.key() };
						UADevice* dev = new UADevice(id);
						g_ua_devices.push_back(dev);

						string msg = "get /devices/" + dev->id + "/inputs";
						tcpClientSend(msg.c_str());

						msg = "get /devices/" + dev->id + "/auxs";
						tcpClientSend(msg.c_str());
					}					
				}
				else if (path_parameter[2] == "DeviceOnline")
				{
					UADevice *dev = getDeviceByUAId(path_parameter[1]);
					if (dev)
					{
						dev->online = element["data"];
						updateSubscriptions();
					}
				}
				else if (path_parameter[2] == "CueBusCount")
				{
					UADevice *dev = getDeviceByUAId(path_parameter[1]);
					if (dev)
					{
						int sends_count = (int)((int64_t)element["data"]);

						if (sends_count + 2 != g_btnSends.size()) {

							cleanUpSendButtons();

							// cues abfragen
							for (int n = 0; n < sends_count; n++) {
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
					}
				}
				else if (path_parameter[2] == "inputs" || path_parameter[2] == "auxs") // inputs laden
				{
					if (path_parameter[3].length() == 0) {
						UADevice* dev = getDeviceByUAId(path_parameter[1]);
						if (dev)
						{
							const dom::object obj = element["data"]["children"];

							string type_str = path_parameter[2] == "inputs" ? ".input." : ".aux.";

							//load device info
							for (dom::object::iterator it = obj.begin(); it != obj.end(); ++it) {
								string id{ it.key() };

								if (path_parameter[2] == "auxs" && dev->id != "0") {
									continue;
								}

								if (g_channelsById.find(dev->id + type_str + id) == g_channelsById.end()) {
									Channel* channel = new Channel(dev, id, path_parameter[2] == "inputs" ? INPUT : AUX);
									g_channelsById.insert({ dev->id + type_str + id, channel });
									int order = g_channelsInOrder.size();
									if (channel->type == AUX) {
										order += 1024; // ans ende sortieren
									}
									g_channelsInOrder.insert({ order, channel });

									const dom::object obj = element["data"]["children"];
									//load device info
									int n = channel->type == AUX ? 0 : 2;
									for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
										if (channel->type==AUX && n >= g_btnSends.size() - 2) {
											break;
										}
										else {
											n = n % g_btnSends.size();
										}
										string sendId = to_string(n++);
										Send* send = new Send(channel, sendId);
										channel->sendsByName.insert({ it->first, send });
										channel->sendsById.insert({ sendId, send });
									}
								}
							}

							loadServerSettings(g_ua_server_connected, dev);
						}
					}
					else {

						Channel* channel = getChannelByUAIds(path_parameter[1], path_parameter[3],
							path_parameter[2] == "inputs" ? INPUT : AUX);
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
												send->meter_level = fromDbFS(db);
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
												send->meter_level2 = fromDbFS(db);
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
										else if (path_parameter[6] == "Gain" && send->channel->touch_point.action != TOUCH_ACTION_LEVEL)//sends
										{
											if (path_parameter[7] == "value")
											{
												send->gain = fromDbFS(element["data"]);
												setRedrawWindow(true);
											}
										}
										else if (path_parameter[6] == "Pan" && send->channel->touch_point.action != TOUCH_ACTION_PAN)//sends
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
												send->bypass = element["data"];
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
									channel->meter_level = fromDbFS(db);
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
									channel->meter_level2 = fromDbFS(db);
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
							else if (path_parameter[4] == "Name") {
								if (path_parameter[5] == "value") {
									string_view sv = element["data"];
									string value{ sv };
									channel->setName(unescape_to_utf8(value));
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
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Stereo") {
								if (path_parameter[5] == "value") {
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
							else if (path_parameter[4] == "Pan2" && channel->touch_point.action != TOUCH_ACTION_PAN2)
							{
								if (path_parameter[5] == "value")
								{
									channel->pan2 = element["data"];
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "ChannelHidden")
							{
								if (path_parameter[5] == "value")
								{
									channel->hidden = element["data"];
									browseToSelectedChannel(g_browseToChannel);
									updateSubscriptions();
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "EnabledByUser")
							{
								if (path_parameter[5] == "value")
								{
									channel->enabledByUser = element["data"];
									browseToSelectedChannel(g_browseToChannel);
									updateSubscriptions();
									setRedrawWindow(true);
								}
							}
							else if (path_parameter[4] == "Active")
							{
								if (path_parameter[5] == "value")
								{
									channel->active = element["data"];
									browseToSelectedChannel(g_browseToChannel);
									updateSubscriptions();
									setRedrawWindow(true);
								}
							}
						}
					}
				}
			}

			try
			{
				//ua-error
				const char *c;
				if (element["error"].get_c_str().get(c) == SUCCESS) {
					string s(c);
					if (g_settings.extended_logging) {
						toLog("UA-Error " + s);
					}
				}
			}
			catch (simdjson_error e) {}
		}
		catch (simdjson_error e)
		{
			if (g_settings.extended_logging) {
				toLog("JSON-Error: " + string(e.what()));
			}
		}
		tcp_msg = "";
		break;
	}
	}
}

void tcpClientSend(const char *msg)
{
	if (g_tcpClient)
	{
		if(g_settings.extended_logging) toLog("UA -> " + string(msg));
		g_tcpClient->Send(msg);
	}
}

void drawScaleMark(int x, int y, int scale_value, double total_scale) {
	Vector2D sz = gfx->GetTextBlockSize(g_fntFaderScale, "-1234567890");
	double v = fromDbFS((double)scale_value);
	y = y - toMeterScale(v) * total_scale + total_scale;

	gfx->DrawShape(GFX_RECTANGLE, RGB(170, 170, 170), x + g_channel_width * 0.25, y, g_channel_width * 0.1, 1, GFX_NONE, 0.8f);
	gfx->Write(g_fntFaderScale, x + g_channel_width * 0.24, y - sz.getY() * 0.5, to_string(scale_value), GFX_RIGHT, NULL,
		GFX_NONE, 0.8f);
}

void drawChannel(Channel *channel, float _x, float _y, float _width, float _height) {

	if (!channel)
		return;

	float x = _x;
	float y = _y;
	float width = _width;
	float height = _height;

	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	double level = 0;
	bool mute = false;
	double pan = 0;
	double pan2 = 0;
	double meter_level = fromDbFS(-144.0);
	double meter_level2 = fromDbFS(-144.0);
	bool clip = false;
	bool clip2 = false;
	bool post_fader = false;

	Vector2D sz;
	Vector2D stretch;

	if (g_btnMix->checked) {
		level = channel->level;
		mute = channel->mute;
		pan = channel->pan;
		pan2 = channel->pan2;
		meter_level = channel->meter_level;
		meter_level2 = channel->meter_level2;
		clip = channel->clip;
		clip2 = channel->clip2;
		post_fader = channel->post_fader;
	}
	else {
		for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
			if (it->second->checked) {
				
				string type_str = channel->type == INPUT ? ".input." : ".aux.";
				unordered_map<string, Send*>::iterator itSend = channel->sendsByName.find(it->first);
				if (itSend == channel->sendsByName.end()) {
					return;
				}
				level = itSend->second->gain;
				mute = itSend->second->bypass;
				pan = itSend->second->pan;
				meter_level = itSend->second->meter_level;
				meter_level2 = itSend->second->meter_level2;
				clip = itSend->second->clip;
				clip2 = itSend->second->clip2;
				break;
			}
		}
	}

	string name = channel->getName();

	if (channel->type == AUX) {
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

	bool btn_select = g_btnSelectChannels->checked;

	unsigned int color = 0;
	if (channel->getColor(&color)) {
		gfx->SetFilter_ColorShift(g_gsFader, 0.3f, color);
		gfx->SetFilter_Contrast(g_gsFader, 1.6f);

		gfx->SetFilter_ColorShift(g_gsLabel[channel->label_gfx], 0.4f, color);
		gfx->SetFilter_Contrast(g_gsLabel[channel->label_gfx], 1.6f);
	}
	else {
		gfx->SetFilter_ColorShift(g_gsFader, 0.0f, 0);
		gfx->SetFilter_Contrast(g_gsFader, 1.0f);
		gfx->SetFilter_ColorShift(g_gsLabel[channel->label_gfx], 0.0f, 0);
		gfx->SetFilter_Contrast(g_gsLabel[channel->label_gfx], 1.0f);
	}

	//SELECT CHANNELS
	if (btn_select) {
		if (!channel->selected_to_show) {
			gray = true;
		}
		else {
			highlight = true;
		}
	}

	if(highlight) {
		unsigned long clr = RGB(200, 200, 50);

		Vector2D pt = {_x - 1, _y};
		Channel *prev_channel = getChannelByPosition(pt);
		if (!prev_channel || !prev_channel->selected_to_show) {
			gfx->DrawShape(GFX_RECTANGLE, clr, _x - SPACE_X, 0, SPACE_X, _height * 1.02, GFX_NONE, 0.5); // links
		}

		gfx->DrawShape(GFX_RECTANGLE, clr, _x, 0, _width - SPACE_X, SPACE_Y, GFX_NONE, 0.5); // oben
		gfx->DrawShape(GFX_RECTANGLE, clr, _x, _height * 1.02 - SPACE_Y, _width - SPACE_X, SPACE_Y, GFX_NONE, 0.5); // unten
		gfx->DrawShape(GFX_RECTANGLE, clr, _x + _width - SPACE_X, 0, SPACE_X, _height * 1.02, GFX_NONE, 0.5); // rechts
	}

	//LABEL überspringen und am Ende zeichenen (wg. asugrauen)
	y += g_fader_label_height;
	height -= g_fader_label_height;

	if (channel->type == INPUT) {
		//PAN
		//tracker
		if (g_btnMix->checked || !channel->stereo) {
			float pan_width = width;
			if (channel->stereo) {
				pan_width /= 2;
				float local_PAN_TRACKER_HEIGHT = pan_width;
				if (local_PAN_TRACKER_HEIGHT > g_pantracker_height)
					local_PAN_TRACKER_HEIGHT = g_pantracker_height;

				stretch = Vector2D(local_PAN_TRACKER_HEIGHT, local_PAN_TRACKER_HEIGHT);
				gfx->Draw(g_gsPan, x + pan_width + (pan_width - local_PAN_TRACKER_HEIGHT) / 2, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2, NULL,
					GFX_NONE, 1.0, 0, NULL, &stretch);

				gfx->Draw(g_gsPanPointer, x + pan_width + (pan_width - local_PAN_TRACKER_HEIGHT) / 2, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2, NULL,
					GFX_NONE, 1.0, DEG2RAD(pan2 * 140), NULL, &stretch);

				gfx->Draw(g_gsPan, x + (pan_width - local_PAN_TRACKER_HEIGHT) / 2, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2, NULL,
					GFX_NONE, 1.0, 0, NULL, &stretch);

				gfx->Draw(g_gsPanPointer, x + (pan_width - local_PAN_TRACKER_HEIGHT) / 2, y + (g_channel_pan_height - local_PAN_TRACKER_HEIGHT) / 2, NULL,
					GFX_NONE, 1.0, DEG2RAD(pan * 140), NULL, &stretch);
			}
			else {
				stretch = Vector2D(g_pantracker_height, g_pantracker_height);
				gfx->Draw(g_gsPan, x + (pan_width - g_pantracker_height) / 2, y + (g_channel_pan_height - g_pantracker_height) / 2, NULL,
					GFX_NONE, 1.0, 0, NULL, &stretch);

				gfx->Draw(g_gsPanPointer, x + (pan_width - g_pantracker_height) / 2, y + (g_channel_pan_height - g_pantracker_height) / 2, NULL,
					GFX_NONE, 1.0, DEG2RAD(pan * 140), NULL, &stretch);
			}

		}
	}

	gfx->SetColor(g_fntMain, RGB(20, 20, 20));
	gfx->SetColor(g_fntFaderScale, RGB(170,170,170));
	gfx->SetColor(g_fntChannelBtn, RGB(80,80,80));
		
	y += g_channel_pan_height;
	height -= g_channel_pan_height;

	int x_offset = (width/2 - g_channel_btn_size) / 2;
	COLORREF clr = 0;
	//SOLO
	if (g_btnMix->checked) {
		int btn_switch = 0;
		if (channel->type == AUX && !channel->post_fader || channel->type != AUX && channel->solo) {
			btn_switch = 1;
		}

		GFXSurface *gsBtn = g_gsButtonRed[btn_switch];
		string txt = "S";
		if (channel->type == AUX) {
			gsBtn = g_gsButtonBlue[btn_switch];
			txt = "PRE";
		}

		stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
		gfx->Draw(gsBtn, x + x_offset, y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

		sz = gfx->GetTextBlockSize(g_fntChannelBtn, txt, GFX_CENTER);
		gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2, y + (g_channel_btn_size - sz.getY()) / 2, txt, GFX_CENTER);
	}

	//MUTE
	int btn_switch = 0;
	if (mute) {
		btn_switch = 1;
	}

	stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
	gfx->Draw(g_gsButtonYellow[btn_switch], x + x_offset + width/2, y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

	sz = gfx->GetTextBlockSize(g_fntChannelBtn, "M", GFX_CENTER);
	gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2 + width / 2, y + (g_channel_btn_size - sz.getY())/2, "M", GFX_CENTER);

	y += g_channel_btn_size;
	height -= g_channel_btn_size;

	//GROUPLABEL
	float group_y = _height - g_channel_btn_size;
	sz = gfx->GetTextBlockSize(g_fntFaderScale, "Group");
	gfx->Write(g_fntFaderScale, x + g_channel_width / 2, group_y - sz.getY() - SPACE_Y *2, "Group", GFX_CENTER, NULL, GFX_NONE, 0.9);

	//GROUP1
	btn_switch = 0;
	if (channel->fader_group == 1) {
		btn_switch = 1;
	}

	stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
	gfx->Draw(g_gsButtonGreen[btn_switch], x + x_offset, group_y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

	sz = gfx->GetTextBlockSize(g_fntChannelBtn, "1", GFX_CENTER);
	gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2, group_y + (g_channel_btn_size - sz.getY()) / 2, "1", GFX_CENTER);

	//GROUP2
	btn_switch = 0;
	if (channel->fader_group == 2) {
		btn_switch = 1;
	}

	stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
	gfx->Draw(g_gsButtonBlue[btn_switch], x + x_offset + width / 2, group_y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

	sz = gfx->GetTextBlockSize(g_fntChannelBtn, "2", GFX_CENTER);
	gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2 + width / 2, group_y + (g_channel_btn_size - sz.getY()) / 2, "2", GFX_CENTER);

	//FADER
	//rail
	height -= g_channel_btn_size + sz.getY() + SPACE_Y; //Fader group unterhalb
	float fader_rail_height = height - g_fadertracker_height;
	stretch = Vector2D(g_faderrail_width, fader_rail_height);
	gfx->Draw(g_gsFaderrail, x + (width - g_faderrail_width) / 2.0f, y + g_fadertracker_height / 2.0f, NULL, GFX_NONE, 1.0f, 0, NULL, &stretch);

	float o = g_fadertracker_height / 2.0f; // toMeterScale(12.0f)* fader_rail_height;
	drawScaleMark(x, y + o, 12,  fader_rail_height);
	drawScaleMark(x, y + o, 6, fader_rail_height);
	drawScaleMark(x, y + o, 0, fader_rail_height);
	drawScaleMark(x, y + o, -6, fader_rail_height);
	drawScaleMark(x, y + o, -12, fader_rail_height);
	drawScaleMark(x, y + o, -20, fader_rail_height);
	drawScaleMark(x, y + o, -32, fader_rail_height);
	drawScaleMark(x, y + o, -52, fader_rail_height);
	drawScaleMark(x, y + o, -84, fader_rail_height);

	//tracker
	stretch = Vector2D(g_fadertracker_width, g_fadertracker_height);

	gfx->Draw(g_gsFader, x + (width - g_fadertracker_width) / 2.0f, y + height - toMeterScale(level) * (height - g_fadertracker_height) - g_fadertracker_height, NULL, 
			GFX_NONE, 1.0f, 0, NULL, &stretch);

	//Clip LED
	gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f - 1.0f,
		y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height - 1.0f - 7.0f,
		7.0f, 7.0f);

	gfx->DrawShape(GFX_RECTANGLE, clip ? METER_COLOR_RED : METER_COLOR_BG, x + g_channel_width * 0.75f,
		y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height - 6.0f,
		5.0f, 5.0f);

	if (channel->stereo) {
		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f + 5.0f,
			y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height - 1.0f - 7.0f,
			7.0f, 7.0f);

		gfx->DrawShape(GFX_RECTANGLE, clip2 ? METER_COLOR_RED : METER_COLOR_BG, x + g_channel_width * 0.75f + 6.0f,
			y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height - 6.0f,
			5.0f, 5.0f);
	}

	//LEVELMETER
	g_channel_slider_height = g_channel_height - g_fader_label_height - g_channel_pan_height;
	float threshold = fader_rail_height * (float)toMeterScale(fromDbFS(METER_THRESHOLD));

	gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f - 1.0f,
		y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height - 1.0f,
		7.0f, fader_rail_height * toMeterScale(DB_UNITY) + 2.0f - threshold);

	gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BG, x + g_channel_width * 0.75f,
		y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height,
		5.0f, fader_rail_height * toMeterScale(DB_UNITY) - threshold);

	if (meter_level > fromDbFS(METER_THRESHOLD)) {
		float amplitude = round(fader_rail_height* (float)toMeterScale(meter_level));

		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_GREEN, x + g_channel_width * 0.75f + 1.0f,
			y + o + fader_rail_height - amplitude,
			3.0f, amplitude - threshold);

		if (meter_level > fromDbFS(-9.0)) {
			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_YELLOW, x + g_channel_width * 0.75f + 1.0f,
				y + o + fader_rail_height - amplitude,
				3.0f, amplitude - fader_rail_height * toMeterScale(fromDbFS(-9.0)));
		}
	}

	if (channel->stereo) {

		gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BORDER, x + g_channel_width * 0.75f + 5.0f,
			y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height - 1.0f,
			7.0f, fader_rail_height* toMeterScale(DB_UNITY) + 2.0f - threshold);

			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_BG, x + g_channel_width * 0.75f + 6.0f,
				y + o - toMeterScale(DB_UNITY) * fader_rail_height + fader_rail_height,
				5.0f, fader_rail_height * toMeterScale(DB_UNITY) - threshold);

		if (meter_level > fromDbFS(METER_THRESHOLD)) {
			float amplitude = round(fader_rail_height * (float)toMeterScale(meter_level2));

			gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_GREEN, x + g_channel_width * 0.75f + 7.0f,
				y + o + fader_rail_height - amplitude,
				3.0f, amplitude - threshold);

			if (meter_level2 > fromDbFS(-9.0)) {
				gfx->DrawShape(GFX_RECTANGLE, METER_COLOR_YELLOW, x + g_channel_width * 0.75f + 7.0f,
					y + o + fader_rail_height - amplitude,
					3.0f, amplitude - fader_rail_height * toMeterScale(fromDbFS(-9.0)));
			}
		}
	}

	if (gray) {
		gfx->DrawShape(GFX_RECTANGLE, RGB(20,20,20), _x, 0, _width, _height * 1.02, GFX_NONE, 0.7);
	}

	//selektion gesperrt (serverseitig überschrieben)
	if (btn_select && (channel->isOverriddenHide() || channel->isOverriddenShow())) {
		gfx->DrawShape(GFX_RECTANGLE, RGB(255, 0, 0), _x, 0, _width, _height * 1.02, GFX_NONE, 0.1);

		gfx->SetColor(g_fntMain, WHITE);
		gfx->SetShadow(g_fntMain, BLACK, 1);
		sz = gfx->GetTextBlockSize(g_fntMain, "locked", GFX_CENTER);
		gfx->Write(g_fntMain, _x + _width / 2, 0 + (_height - sz.getY()) / 2, "locked", GFX_CENTER);
		gfx->SetShadow(g_fntMain, 0, 0);
	}

	//LABEL
	stretch = Vector2D(_width * 1.1, g_fader_label_height);
	gfx->Draw(g_gsLabel[channel->label_gfx], _x - _width * 0.05, _y, NULL, GFX_NONE, 1.0, channel->label_rotation, NULL, &stretch);

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

	gfx->Write(g_fntLabel, _x, _y + (g_fader_label_height - sz.getY()) / 2, name, GFX_CENTER, &max_size);
}

int getMiddleSelectedChannel() {

	int channels_per_page = calculateChannelsPerPage();
	int index = 0;

	for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
		if (!it->second->isVisible(true))
			continue;

		if (index >= g_page * channels_per_page + channels_per_page / 2) {
			break;
		}
		index++;
	}
	return index;
}

void browseToSelectedChannel(int index) {

	int channels_per_page = calculateChannelsPerPage();

	if (channels_per_page) {
		g_page = index / channels_per_page;
	}
}

void draw() {
	
	g_draw_mutex.lock();

	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float win_width = (float)w;
	float win_height = (float)h;
	Vector2D sz;
	Vector2D stretch;

	//background
	gfx->DrawShape(GFX_RECTANGLE, BLACK, 0, 0, win_width, win_height);

	int channels_per_page = calculateChannelsPerPage();

	//bg right
	stretch = Vector2D(g_channel_offset_x, win_height);
	gfx->Draw(g_gsBgRight, g_channel_offset_x + g_channel_width * channels_per_page, 0, NULL, 
		GFX_NONE, 1.0, 0, NULL, &stretch);

	//bg left
	stretch = Vector2D(g_channel_offset_x, win_height);
	gfx->Draw(g_gsBgRight, 0, 0, NULL, GFX_HFLIP, 1.0, 0, NULL, &stretch);

	//draw buttons
	g_btnSettings->draw(BTN_COLOR_YELLOW);
	g_btnMuteAll->draw(BTN_COLOR_RED);
	for (vector<pair<string, Button*>>::iterator it = g_btnSends.begin(); it != g_btnSends.end(); ++it) {
		if (it->second->text == "AUX 1") {
			string text = g_settings.label_aux1;
			if (g_settings.label_aux1 == g_settings.label_aux2) {
				text += " 1";
			}
			text += "\nSend";
			it->second->draw(BTN_COLOR_BLUE, text);
		}
		else if (it->second->text == "AUX 2") {
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

	for (int n = 0; n < g_btnConnect.size(); n++) {
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
	gfx->Write(g_fntMain, g_btnPageLeft->rc.x + g_btnPageLeft->rc.w / 2,
		g_btnPageLeft->rc.y - SPACE_Y * 2.0f - g_main_fontsize, str_page, GFX_CENTER);

	if (g_ua_server_connected.length() == 0) { //offline
		sz = gfx->GetTextBlockSize(g_fntOffline, "Offline");
		gfx->SetColor(g_fntOffline, RGB(100, 100, 100));
		gfx->Write(g_fntOffline, win_width/2, (win_height - sz.getY())/2, "Offline", GFX_CENTER);

		string refresh_txt = ".refreshing serverlist.";
		unsigned long long time = GetTickCount64() / 1000;

		for (int n = 0; n < (time % 4); n++)
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
		bool btn_select = g_btnSelectChannels->checked;
		
		//BG
		stretch = Vector2D(g_channel_width * channels_per_page, win_height);
		gfx->Draw(g_gsChannelBg, g_channel_offset_x, 0, NULL, 
			GFX_NONE, 1.0, 0, NULL, &stretch);

		Channel* reorderChannel = NULL;
		float reorderChannelXcenter = 0.0f;
		for (map<int, Channel*>::iterator it = g_channelsInOrder.begin(); it != g_channelsInOrder.end(); ++it) {
			if (!it->second->isVisible(!btn_select))
				continue;

			if (count >= g_page * channels_per_page && count < (g_page + 1) * channels_per_page) {
				float x = g_channel_offset_x + offset * g_channel_width;

				//channel seperator
				gfx->DrawShape(GFX_RECTANGLE, BLACK, x + g_channel_width - SPACE_X / 2.0f, 0, SPACE_X, win_height, GFX_NONE, 0.7f);

				if (it->second->touch_point.action != TOUCH_ACTION_REORDER) {
					drawChannel(it->second, x, g_channel_offset_y, g_channel_width, g_channel_height);
				}
				else {
					reorderChannel = it->second;
					reorderChannelXcenter = x + g_channel_width / 2.0f;
				}

				offset++;
			}
			count++;
		}

		if (reorderChannel) {
			float offset = reorderChannel->touch_point.pt_start_x - g_channel_offset_x;
			offset = -fmodf(offset, g_channel_width);
			float x = reorderChannel->touch_point.pt_end_x + offset;

			float fmove = (reorderChannel->touch_point.pt_end_x - reorderChannel->touch_point.pt_start_x) / g_channel_width;
			int move = (int)(fmove > 0.0f ? floor(fmove) : ceil(fmove));
			if (move) {
				reorderChannelXcenter += (float)move * g_channel_width + g_channel_width * 0.5f * (float)(abs(move) / move);
				gfx->DrawShape(GFX_RECTANGLE, RGB(200, 200, 50), reorderChannelXcenter - 4.0f, 0, 8.0f, win_height, GFX_NONE, 0.5f);
			}

			gfx->DrawShape(GFX_RECTANGLE, BLACK, x, 0, g_channel_width, win_height, GFX_NONE, 0.5f);
			drawChannel(reorderChannel, x, g_channel_offset_y, g_channel_width, g_channel_height);
		}
	}

	if (g_msg.length()) {
		sz = gfx->GetTextBlockSize(g_fntMain, g_msg);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->SetShadow(g_fntMain, BLACK, 1.0);

		gfx->DrawShape(GFX_RECTANGLE, WHITE, (win_width - sz.getX() - 20) / 2, (win_height - sz.getY() - 20) / 2, sz.getX() + 20, sz.getY() + 20, 0, 0.2);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, (win_width - sz.getX() - 16) / 2, (win_height - sz.getY() - 16) / 2, sz.getX() + 16, sz.getY() + 16, 0, 0.7);

		gfx->Write(g_fntMain, win_width / 2, (win_height - sz.getY()) / 2, g_msg, GFX_CENTER);
		gfx->SetShadow(g_fntMain, BLACK, 0);
		gfx->SetColor(g_fntMain, BLACK);
	}

	if (g_btnSettings->checked) {
		float x = g_channel_offset_x + 20.0f;
		float y = 20.0f;
		float vspace = 20.0f;

		float box_height = g_btn_height * (9.0f + max(1.0f, (float)g_btnsServers.size())) / 2.0f + 7.0f * vspace + 52.0f;
		float box_width = g_main_fontsize * 10.0f + 2.0f * g_btn_width + 40.0f;
		gfx->DrawShape(GFX_RECTANGLE, WHITE, g_channel_offset_x, 0, box_width, box_height, 0, 0.3);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, g_channel_offset_x + 2, 2, box_width - 4, box_height - 4, 0, 0.9);

		g_btnInfo->draw(BTN_COLOR_YELLOW);

		y += g_btn_height / 2.0f + vspace;

		string s = "Lock settings";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);
		g_btnLockSettings->rc.x = x + g_main_fontsize * 10.0f;
		g_btnLockSettings->rc.y = y;
		g_btnLockSettings->draw(BTN_COLOR_GREEN);
		y += g_btn_height / 2.0f + 2.0f + vspace;

		s = "Lock to mix";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);
		g_btnLockToMixMIX->rc.x = x + g_main_fontsize * 10.0f;
		g_btnLockToMixMIX->rc.y = y;
		g_btnLockToMixMIX->draw(BTN_COLOR_GREEN);

		for (int n = 0; n < 2; n++) {
			g_btnLockToMixAUX[n]->rc.x = (int)round(x + g_main_fontsize * 10.0f + (float)(1 + n) * g_btn_width / 2.0f);
			g_btnLockToMixAUX[n]->rc.y = (int)round(y);
			g_btnLockToMixAUX[n]->draw(BTN_COLOR_GREEN);
		}

		/*		g_btnLockToMixHP->rc.x = x + g_main_fontsize * 10.0f;
				g_btnLockToMixHP->rc.y = y;
				g_btnLockToMixHP->draw(BTN_COLOR_GREEN);
		*/

		y += g_btn_height / 2.0f + 2.0f;

		for (int n = 0; n < 4; n++) {
			g_btnLockToMixCUE[n]->rc.x = (int)round(x + g_main_fontsize * 10.0f + (float)n * g_btn_width / 2.0f);
			g_btnLockToMixCUE[n]->rc.y = (int)round(y);
			g_btnLockToMixCUE[n]->draw(BTN_COLOR_GREEN);
		}

		y += g_btn_height / 2.0f + 2.0f + vspace;

		s = "Label AUX 1";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);

		for (int n = 0; n < 4; n++) {
			g_btnLabelAUX1[n]->rc.x = (int)round(x + g_main_fontsize * 10.0f + (float)n * g_btn_width / 2.0f);
			g_btnLabelAUX1[n]->rc.y = (int)round(y);
			g_btnLabelAUX1[n]->draw(BTN_COLOR_GREEN);
		}

		y += g_btn_height / 2.0f + 2.0f;

		s = "Label AUX 2";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);

		for (int n = 0; n < 4; n++) {
			g_btnLabelAUX2[n]->rc.x = (int)round(x + g_main_fontsize * 10.0f + (float)n * g_btn_width / 2.0f);
			g_btnLabelAUX2[n]->rc.y = (int)round(y);
			g_btnLabelAUX2[n]->draw(BTN_COLOR_GREEN);
		}

		y += g_btn_height / 2.0f + 2.0f + vspace;


		s = "MIX Meter";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);
		g_btnPostFaderMeter->rc.x = x + g_main_fontsize * 10.0f;
		g_btnPostFaderMeter ->rc.y = y;
		g_btnPostFaderMeter->draw(BTN_COLOR_GREEN);
		y += g_btn_height / 2.0f + 2.0f + vspace;

		s = "Show offline devices";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);
		g_btnShowOfflineDevices->rc.x = x + g_main_fontsize * 10.0f;
		g_btnShowOfflineDevices->rc.y = y;
		g_btnShowOfflineDevices->draw(BTN_COLOR_GREEN);
		y += g_btn_height / 2.0f + 2.0f + vspace;

		s = "Try to reconnect\nautomatically";
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->SetColor(g_fntMain, WHITE);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);
		g_btnReconnectOn->rc.x = x + g_main_fontsize * 10.0f;
		g_btnReconnectOn->rc.y = y;
		g_btnReconnectOn->draw(BTN_COLOR_GREEN);
		y += g_btn_height / 2.0f + 2.0f + vspace;

		s = "Server list";
		gfx->SetColor(g_fntMain, WHITE);
		sz = gfx->GetTextBlockSize(g_fntMain, s);
		gfx->Write(g_fntMain, x, y + (g_btn_height / 2.0f - sz.getY()) / 2.0f, s, GFX_LEFT);

		g_btnServerlistScan->rc.x = x + g_main_fontsize * 10.0f;
		g_btnServerlistScan->rc.y = y;
		g_btnServerlistScan->draw(BTN_COLOR_GREEN);

		for (vector<Button*>::iterator it = g_btnsServers.begin(); it != g_btnsServers.end(); ++it) {
			(*it)->rc.x = x + g_main_fontsize * 10.0f + (g_btn_width + 2.0f);
			(*it)->rc.y = y;
			(*it)->draw(BTN_COLOR_GREEN);
			y += g_btn_height / 2.0f + 2.0f;
		}

		gfx->SetColor(g_fntMain, BLACK);
	}

	if (g_btnInfo->checked) {
		sz = gfx->GetTextBlockSize(g_fntInfo, INFO_TEXT);
		gfx->SetColor(g_fntInfo, RGB(220,220,220));
		gfx->SetShadow(g_fntInfo, BLACK, 1.0);

		gfx->DrawShape(GFX_RECTANGLE, WHITE, (win_width - sz.getX() - 30) / 2, (win_height - sz.getY() - 30) / 2, sz.getX() + 30, sz.getY() + 30, 0, 0.3);
		gfx->DrawShape(GFX_RECTANGLE, BLACK, (win_width - sz.getX() - 26) / 2, (win_height - sz.getY() - 26) / 2, sz.getX() + 26, sz.getY() + 26, 0, 0.9);

		gfx->Write(g_fntInfo, win_width / 2, (win_height - sz.getY()) / 2, INFO_TEXT, GFX_CENTER);
		gfx->SetShadow(g_fntInfo, BLACK, 0);
		gfx->SetColor(g_fntInfo, BLACK);
	}

	gfx->Update();

	g_draw_mutex.unlock();
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

	g_btnSelectChannels->enabled=true;
	g_btnReorderChannels->enabled = true;
	g_btnChannelWidth->enabled=true;
	g_btnPageLeft->enabled=true;
	g_btnPageRight->enabled=true;
	g_btnMix->enabled=true;
	g_btnMuteAll->enabled = true;

	g_ua_server_last_connection = connection_index;

	setRedrawWindow(true);

	return true;
}
#endif

#ifndef SIMULATION
bool connect(int connection_index)
{
	if (connection_index != g_ua_server_last_connection) {
		g_page = 0;
		g_btnMuteAll->checked = false;
		g_channelsMutedBeforeAllMute.clear();
	}

	if (g_ua_server_list[connection_index].length() == 0)
		return false;

	//connect to UA
	try
	{
		g_msg = "Connecting to " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT;
		draw(); // force draw

		g_tcpClient = new TCPClient(g_ua_server_list[connection_index].c_str(), UA_TCP_PORT, &tcpClientProc);
		g_ua_server_connected = g_ua_server_list[connection_index];
		toLog("UA:  Connected on " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT);

		tcpClientSend("subscribe /Session");
		tcpClientSend("subscribe /PostFaderMetering");
		tcpClientSend("get /devices");

		g_btnSelectChannels->enabled = true;
		g_btnReorderChannels->enabled = true;
		g_btnChannelWidth->enabled = true;
		g_btnPageLeft->enabled = true;
		g_btnPageRight->enabled = true;
		g_btnMix->enabled = true;
		g_btnMuteAll->enabled = true;

		for (int n = 0; n < g_btnConnect.size(); n++) {
			if (g_btnConnect[n]->id - ID_BTN_CONNECT == connection_index) {
				g_btnConnect[n]->checked = true;
				break;
			}
		}

		g_ua_server_last_connection = connection_index;

		g_msg = "";

		setNetworkTimeout();

		setRedrawWindow(true);
	}
	catch (string error)
	{
		g_msg = "Connection failed on " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT + ": " + string(error);

		toLog("UA:  " + g_msg);

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
	}

	g_btnSelectChannels->enabled=false;
	g_btnReorderChannels->enabled = false;
	g_btnChannelWidth->enabled=false;
	g_btnPageLeft->enabled=false;
	g_btnPageRight->enabled=false;
	g_btnMix->enabled=false;
	g_btnPostFaderMeter->enabled = false;
	g_btnMuteAll->enabled = false;

	saveServerSettings(g_ua_server_connected);

	if (g_tcpClient)
	{
		toLog("UA:  Disconnect from " + g_ua_server_connected + ":" + UA_TCP_PORT);

		delete g_tcpClient;
		g_tcpClient = NULL;
	}

	g_ua_server_connected = "";

	cleanUpSendButtons();

	cleanUpUADevices();

	for (int n = 0; n < g_btnConnect.size(); n++)
	{
		g_btnConnect[n]->checked=false;
	}

	g_btnMix->checked=true;

	setRedrawWindow(true);
}

string getCheckedConnectionButton()
{
	for (int n = 0; n < g_btnConnect.size(); n++)
	{
		if (g_btnConnect[n]->checked)
		{
			return g_btnConnect[n]->text;
		}
	}
	return "";
}

void onTouchDown(Vector2D *mouse_pt, SDL_TouchFingerEvent *touchinput)
{
	Vector2D pos_pt = { -1,-1 };

	if (touchinput)
	{
		int w,h;
		SDL_GetWindowSize(g_window, &w, &h);
		pos_pt.setX(touchinput->x * w); //touchpoint ist normalisiert
		pos_pt.setY(touchinput->y * h);
	}
	else if (mouse_pt)
	{
		pos_pt = *mouse_pt;
	}

	Channel *channel = getChannelByPosition(pos_pt);
	if (channel && channel->touch_point.id == 0 && !channel->touch_point.is_mouse)
	{
		Vector2D relative_pos_pt = pos_pt;
		relative_pos_pt.subtractX(g_channel_offset_x);
		relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
		relative_pos_pt.subtractY(g_channel_offset_y);

		if (g_btnSelectChannels->checked)
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}
			else
				channel->touch_point.is_mouse = true;

			channel->touch_point.action = TOUCH_ACTION_SELECT;
			channel->selected_to_show = !channel->selected_to_show;

			if (channel->isOverriddenShow())
				channel->selected_to_show = true;
			else if (channel->isOverriddenHide())
				channel->selected_to_show = false;

			setRedrawWindow(true);
		}
		else if (g_btnReorderChannels->checked)
		{
			if (!g_reorder_dragging) {
				g_reorder_dragging = true;
				if (touchinput)
				{
					channel->touch_point.id = touchinput->touchId;
					channel->touch_point.finger_id = touchinput->fingerId;
					channel->touch_point.pt_start_x = pos_pt.getX();
					channel->touch_point.pt_start_y = pos_pt.getY();
					channel->touch_point.action = TOUCH_ACTION_REORDER;
				}
				else if (mouse_pt)
				{
					channel->touch_point.is_mouse = true;
					channel->touch_point.pt_start_x = mouse_pt->getX();
					channel->touch_point.pt_start_y = mouse_pt->getY();
					channel->touch_point.action = TOUCH_ACTION_REORDER;
				}
			}
		}
		else if (channel->isTouchOnPan(&relative_pos_pt))
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
				channel->touch_point.pt_start_x = pos_pt.getX();
				channel->touch_point.pt_start_y = pos_pt.getY();
				channel->touch_point.action = TOUCH_ACTION_PAN;
			}
			else if(mouse_pt)
			{
				channel->touch_point.is_mouse = true;
				channel->touch_point.pt_start_x = mouse_pt->getX();
				channel->touch_point.pt_start_y = mouse_pt->getY();
				channel->touch_point.action = TOUCH_ACTION_PAN;
			}
		}
		else if (channel->isTouchOnPan2(&relative_pos_pt))
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
				channel->touch_point.pt_start_x = pos_pt.getX();
				channel->touch_point.pt_start_y = pos_pt.getY();
				channel->touch_point.action = TOUCH_ACTION_PAN2;
			}
			else if (mouse_pt)
			{
				channel->touch_point.is_mouse = true;
				channel->touch_point.pt_start_x = mouse_pt->getX();
				channel->touch_point.pt_start_y = mouse_pt->getY();
				channel->touch_point.action = TOUCH_ACTION_PAN2;
			}
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

			if (g_btnMix->checked)
			{
				if (channel->fader_group)
				{
					int state = (int)!channel->mute;
					for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
						if (it->second->isVisible(false))
						{
							if (it->second->fader_group == channel->fader_group)
							{
								it->second->pressMute(state);
								setRedrawWindow(true);
							}
						}
					}
				}
				else
				{
					channel->pressMute();
					setRedrawWindow(true);
				}
			}
			else {
				for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
					if ((*itB).second->checked) {
						Send* send = channel->getSendByName((*itB).first);
						if (send) {
							if (channel->fader_group) {
								int state = (int)!send->bypass;
								for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
									if (it->second->isVisible(false)) {
										if (it->second->fader_group == channel->fader_group) {
											send = it->second->getSendByName((*itB).first);
											if (send) {
												send->pressBypass(state);
												setRedrawWindow(true);
											}
										}
									}
								}
							}
							else {
								send->pressBypass();
								setRedrawWindow(true);
							}
						}
						break;
					}
				}
			}
		}
		else if (channel->isTouchOnSolo(&relative_pos_pt))
		{
			if(touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}
			else
				channel->touch_point.is_mouse = true;
			channel->touch_point.action = TOUCH_ACTION_SOLO;

			if (g_btnMix->checked)
			{
				if (channel->fader_group)
				{
					int state = (int)!channel->solo;
					for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
						if (it->second->isVisible(false))
						{
							if (it->second->fader_group == channel->fader_group)
							{
								it->second->pressSolo(state);
								setRedrawWindow(true);
							}
						}
					}
				}
				else
				{
					channel->pressSolo();
					setRedrawWindow(true);
				}
			}
		}
		else if (channel->isTouchOnPostFader(&relative_pos_pt))
		{
			if (touchinput)
			{
				channel->touch_point.id = touchinput->touchId;
				channel->touch_point.finger_id = touchinput->fingerId;
			}
			else
				channel->touch_point.is_mouse = true;
			channel->touch_point.action = TOUCH_ACTION_POST_FADER;

			if (g_btnMix->checked)
			{
				if (channel->fader_group)
				{
					int state = (int)!channel->post_fader;
					for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
						if (it->second->isVisible(false))
						{
							if (it->second->fader_group == channel->fader_group)
							{
								it->second->pressPostFader(state);
								setRedrawWindow(true);
							}
						}
					}
				}
				else
				{
					channel->pressPostFader();
					setRedrawWindow(true);
				}
			}
		}
		else if (channel->isTouchOnGroup1(&relative_pos_pt))
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

			setRedrawWindow(true);
		}
		else if (channel->isTouchOnGroup2(&relative_pos_pt))
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

			setRedrawWindow(true);
		}
		else if (channel->isTouchOnFader(&relative_pos_pt))
		{
			string root = "unsubscribe /devices/" + channel->device->id + "/inputs/" + channel->id;
			string cmd = root + "/FaderLevel";
			tcpClientSend(cmd.c_str());

			if (channel->fader_group)
			{
				for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
					if (it->second->isVisible(false))
					{
						if (it->second->fader_group == channel->fader_group)
						{
							if (touchinput)
							{
								it->second->touch_point.id = touchinput->touchId;
								it->second->touch_point.finger_id = touchinput->fingerId;
								it->second->touch_point.pt_start_x = pos_pt.getX();
								it->second->touch_point.pt_start_y = pos_pt.getY();
								it->second->touch_point.action = TOUCH_ACTION_LEVEL;
							}
							else if (mouse_pt)
							{
								it->second->touch_point.is_mouse = true;
								it->second->touch_point.pt_start_x = mouse_pt->getX();
								it->second->touch_point.pt_start_y = mouse_pt->getY();
								it->second->touch_point.action = TOUCH_ACTION_LEVEL;
							}
						}
					}
				}
			}
			else
			{
				if (touchinput)
				{
					channel->touch_point.id = touchinput->touchId;
					channel->touch_point.finger_id = touchinput->fingerId;
					channel->touch_point.pt_start_x = pos_pt.getX();
					channel->touch_point.pt_start_y = pos_pt.getY();
					channel->touch_point.action = TOUCH_ACTION_LEVEL;
				}
				else if (mouse_pt)
				{
					channel->touch_point.is_mouse = true;
					channel->touch_point.pt_start_x = mouse_pt->getX();
					channel->touch_point.pt_start_y = mouse_pt->getY();
					channel->touch_point.action = TOUCH_ACTION_LEVEL;
				}
			}
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
		Vector2D pos_pt = { -1,-1 };

		if (touchinput)
		{
			int w,h;
			SDL_GetWindowSize(g_window, &w, &h);
			pos_pt.setX(touchinput->x * w); //touchpoint ist normalisiert
			pos_pt.setY(touchinput->y * h);
		}
		else if (mouse_pt) {
			pos_pt = *mouse_pt;
		}

//			printf("Drag: touch:%s mouse:%s y: %d\n", touchinput ? "true" : "false", mouse_pt ? "true":"false", (int)pos_pt.getY());

		channel->touch_point.pt_end_x = pos_pt.getX();
		channel->touch_point.pt_end_y = pos_pt.getY();

		if (channel->touch_point.action == TOUCH_ACTION_SELECT)
		{
			Channel* hover_channel = getChannelByPosition(pos_pt);
			if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
			{
				hover_channel->selected_to_show = channel->selected_to_show;
				//override als selected, wenn name mit ! beginnt
				if (hover_channel->isOverriddenShow())
					hover_channel->selected_to_show = true;
				//override als nicht selected, wenn name mit # endet
				else if (hover_channel->isOverriddenHide())
					hover_channel->selected_to_show = false;
				setRedrawWindow(true);
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_REORDER)
		{
			setRedrawWindow(true);
		}
		else if (channel->touch_point.action == TOUCH_ACTION_MUTE)
		{
			Channel* hover_channel = getChannelByPosition(pos_pt);

			if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
			{
				Vector2D relative_pos_pt = pos_pt;
				relative_pos_pt.subtractX(g_channel_offset_x);
				relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
				relative_pos_pt.subtractY(g_channel_offset_y);

				if (hover_channel->isTouchOnMute(&relative_pos_pt))
				{
					if (g_btnMix->checked)
					{
						if (hover_channel->fader_group)
						{
							int state = (int)channel->mute;
							for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
								if (it->second->isVisible(false))
								{
									if (it->second->fader_group == hover_channel->fader_group)
									{
										if (it->second->mute != (bool)state)
										{
											it->second->pressMute(state);
											setRedrawWindow(true);
										}
									}
								}
							}
						}
						else
						{
							if (hover_channel->mute != channel->mute)
							{
								hover_channel->pressMute(channel->mute);
								setRedrawWindow(true);
							}
						}
					}
					else {
						for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
							if ((*itB).second->checked) {
								Send* send = channel->getSendByName((*itB).first);
								Send* hover_send = hover_channel->getSendByName((*itB).first);

								if (send && hover_send) {
									if (hover_channel->fader_group) {
										int state = (int)send->bypass;
										for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
											if (it->second->isVisible(false)) {
												if (it->second->fader_group == hover_channel->fader_group) {
													send = it->second->getSendByName((*itB).first);
													if (send->bypass != (bool)state)
													{
														send->pressBypass(state);
														setRedrawWindow(true);
													}
												}
											}
										}
									}
									else
									{
										if (hover_send->bypass != send->bypass)
										{
											hover_send->pressBypass(send->bypass);
											setRedrawWindow(true);
										}
									}
								}
								break;
							}
						}
					}
				}
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_SOLO)
		{
			Channel* hover_channel = getChannelByPosition(pos_pt);
			if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
			{
				Vector2D relative_pos_pt = pos_pt;
				relative_pos_pt.subtractX(g_channel_offset_x);
				relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
				relative_pos_pt.subtractY(g_channel_offset_y);

				if (hover_channel->isTouchOnSolo(&relative_pos_pt))
				{
					if (g_btnMix->checked)
					{
						if (hover_channel->fader_group)
						{
							int state = (int)channel->solo;
							for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
								if (it->second->isVisible(false))
								{
									if (it->second->fader_group == hover_channel->fader_group)
									{
										if (it->second->solo != (bool)state)
										{
											it->second->pressSolo(state);
											setRedrawWindow(true);
										}
									}
								}
							}
						}
						else
						{
							if (hover_channel->solo != channel->solo)
							{
								hover_channel->pressSolo(channel->solo);
								setRedrawWindow(true);
							}
						}
					}
				}
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_POST_FADER)
		{
			Channel* hover_channel = getChannelByPosition(pos_pt);
			if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
			{
				Vector2D relative_pos_pt = pos_pt;
				relative_pos_pt.subtractX(g_channel_offset_x);
				relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
				relative_pos_pt.subtractY(g_channel_offset_y);

				if (hover_channel->isTouchOnPostFader(&relative_pos_pt))
				{
					if (g_btnMix->checked)
					{
						if (hover_channel->fader_group)
						{
							int state = (int)channel->post_fader;
							for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
								if (it->second->isVisible(false))
								{
									if (it->second->fader_group == hover_channel->fader_group)
									{
										if (it->second->post_fader != (bool)state)
										{
											it->second->pressPostFader(state);
											setRedrawWindow(true);
										}
									}
								}
							}
						}
						else
						{
							if (hover_channel->post_fader != channel->post_fader)
							{
								hover_channel->pressPostFader(channel->post_fader);
								setRedrawWindow(true);
							}
						}
					}
				}
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_GROUP)
		{
			Channel* hover_channel = getChannelByPosition(pos_pt);

			if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
			{
				Vector2D relative_pos_pt = pos_pt;
				relative_pos_pt.subtractX(g_channel_offset_x);
				relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
				relative_pos_pt.subtractY(g_channel_offset_y);

				if (hover_channel->isTouchOnGroup1(&relative_pos_pt)
					|| hover_channel->isTouchOnGroup2(&relative_pos_pt))
				{
				//	if (Button_GetCheck(g_hwndBtnMix) == BST_CHECKED)
					{
						if (hover_channel->fader_group != channel->fader_group)
						{
							hover_channel->fader_group=channel->fader_group;
							setRedrawWindow(true);
						}
					}
				}
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_PAN)
		{
			double pan_move = (double)(channel->touch_point.pt_end_x - channel->touch_point.pt_start_x) / g_channel_width;

			if (abs(pan_move) > UA_SENDVALUE_RESOLUTION)
			{
				channel->touch_point.pt_start_x = channel->touch_point.pt_end_x;
				channel->touch_point.pt_start_y = channel->touch_point.pt_end_y;

				if (g_btnMix->checked)
				{
					channel->changePan(pan_move);
				}
				else
				{
					for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
						if (itB->second->checked)
						{
							Send* send = channel->getSendByName(itB->first);
							if (send) {
								send->changePan(pan_move);
							}
							break;
						}
					}
				}
				setRedrawWindow(true);
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_PAN2)
		{
			double pan_move = (double)(channel->touch_point.pt_end_x - channel->touch_point.pt_start_x) / g_channel_width;

			if (abs(pan_move) > UA_SENDVALUE_RESOLUTION)
			{
				channel->touch_point.pt_start_x = channel->touch_point.pt_end_x;
				channel->touch_point.pt_start_y = channel->touch_point.pt_end_y;

				if (g_btnMix->checked)
				{
					channel->changePan2(pan_move);
					setRedrawWindow(true);
				}
			}
		}
		else if (channel->touch_point.action == TOUCH_ACTION_LEVEL)
		{
			double fader_move = -(double)(channel->touch_point.pt_end_y - channel->touch_point.pt_start_y) / g_channel_slider_height;

			if (abs(fader_move) > UA_SENDVALUE_RESOLUTION)
			{
				if (channel->fader_group)
				{
					for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
						if (it->second->isVisible(false))
						{
							if (it->second->fader_group == channel->fader_group)
							{
								it->second->touch_point.pt_start_x = it->second->touch_point.pt_end_x;
								it->second->touch_point.pt_start_y = it->second->touch_point.pt_end_y;

								if (g_btnMix->checked) {
									it->second->changeLevel(fader_move);
									setRedrawWindow(true);
								}
								else {
									for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
										if ((*itB).second->checked) {
											Send* send = it->second->getSendByName((*itB).first);
											if (send) {
												send->changeGain(fader_move);
												setRedrawWindow(true);
											}
											break;
										}
									}
								}
							}
						}
					}
				}
				else
				{
					channel->touch_point.pt_start_x = channel->touch_point.pt_end_x;
					channel->touch_point.pt_start_y = channel->touch_point.pt_end_y;

					if (g_btnMix->checked)
					{
						channel->changeLevel(fader_move);
						setRedrawWindow(true);
					}
					else
					{
						for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
							if ((*itB).second->checked)
							{
								Send* send = channel->getSendByName((*itB).first);
								if (send) {
									send->changeGain(fader_move);
									setRedrawWindow(true);
								}
								break;
							}
						}
					}
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
			float fmove = (channel->touch_point.pt_end_x - channel->touch_point.pt_start_x) / g_channel_width;
			int move = (int)(fmove > 0.0f ? floor(fmove) : ceil(fmove));

			map<int, Channel*> channelsInOrderCopy;

			int channelIndex = -1;
			int invisibleChannels = 0;
			if (move > 0) {
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
			else if (move < 0) {
				int n = g_channelsInOrder.size() - 1;
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

		if (channel->fader_group)
		{
			for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
				if (it->second->isVisible(false))
				{
					if (it->second->fader_group == channel->fader_group)
					{
						if (it->second->touch_point.action == TOUCH_ACTION_LEVEL)
						{
							string cmd = "subscribe /devices/" + it->second->device->id + "/inputs/"
								+ it->second->id + "/FaderLevel/";
							tcpClientSend(cmd.c_str());
						}
						if (touchinput)
						{
							it->second->touch_point.id = 0;
							it->second->touch_point.finger_id = 0;
						}
						if (is_mouse)
							it->second->touch_point.is_mouse = false;

						it->second->touch_point.action = TOUCH_ACTION_NONE;
					}
				}
			}
		}
		else
		{
			if (channel->touch_point.action == TOUCH_ACTION_LEVEL)
			{
				string cmd = "subscribe /devices/" + channel->device->id + "/inputs/" + channel->id + "/FaderLevel/";
				tcpClientSend(cmd.c_str());
			}
			if (touchinput)
			{
				channel->touch_point.id = 0;
				channel->touch_point.finger_id = 0;
			}
			if (is_mouse)
				channel->touch_point.is_mouse = false;

			channel->touch_point.action = TOUCH_ACTION_NONE;
		}
	}
}

void updateLabelAux1(Button* btn) {
	for (int n = 0; n < 4; n++) {
		g_btnLabelAUX1[n]->checked = false;
	}
	btn->checked = true;

	g_settings.label_aux1 = btn->text;

	setRedrawWindow(true);
}

void updateLabelAux2(Button* btn) {
	for (int n = 0; n < 4; n++) {
		g_btnLabelAUX2[n]->checked = false;
	}
	btn->checked = true;

	g_settings.label_aux2 = btn->text;

	setRedrawWindow(true);
}

void updateLockToMixSetting(Button *btn) {
	if(btn != g_btnLockToMixMIX)
		g_btnLockToMixMIX->checked = false;
/*	if (btn != g_btnLockToMixHP)
		g_btnLockToMixHP->checked = false;
*/	
	for (int n = 0; n < 2; n++) {
		if (btn != g_btnLockToMixAUX[n])
			g_btnLockToMixAUX[n]->checked = false;
	}
	for (int n = 0; n < 4; n++) {
		if (btn != g_btnLockToMixCUE[n])
			g_btnLockToMixCUE[n]->checked = false;
	}

	btn->checked = !btn->checked;

	if (btn->checked) {
		g_settings.lock_to_mix = btn->text;

		if (g_settings.lock_to_mix == "MIX") {
			g_btnMix->enabled = true;
			g_btnMix->checked = true;
		}
		else {
			g_btnMix->enabled = false;
			g_btnMix->checked = false;
		}
		for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
			itB->second->enabled = (itB->second->text == g_settings.lock_to_mix);
			itB->second->checked = itB->second->enabled;
		}
	}
	else {
		g_settings.lock_to_mix = "";
		g_btnMix->enabled = true;
		for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
			itB->second->enabled = true;
		}
	}
	updateAllMuteBtnText();
	updateSubscriptions();
	setRedrawWindow(true);
}

void createStaticButtons()
{
	g_btnSettings = new Button(ID_BTN_INFO, "Settings",0,0,g_btn_width, g_btn_height,false, true);
	g_btnMuteAll = new Button(ID_BTN_INFO, "Mute all", 0, 0, g_btn_width, g_btn_height, false, false);
	updateAllMuteBtnText();
	g_btnSelectChannels = new Button(ID_BTN_SELECT_CHANNELS, "Select\nChannels",0,0,g_btn_width, g_btn_height, false, false);
	g_btnReorderChannels = new Button(ID_BTN_SELECT_CHANNELS, "Reorder\nChannels", 0, 0, g_btn_width, g_btn_height, false, false);
	g_btnChannelWidth = new Button(ID_BTN_CHANNELWIDTH, "View:\nRegular",0,0,g_btn_width, g_btn_height, false, false);
	g_btnPageLeft = new Button(ID_BTN_PAGELEFT, "Page Left",0,0,g_btn_width, g_btn_height,false, false);
	g_btnPageRight = new Button(ID_BTN_PAGELEFT, "Page Right",0,0,g_btn_width, g_btn_height,false, false);
	g_btnMix = new Button(ID_BTN_MIX, "MIX", 0, 0, g_btn_width, g_btn_height, true,false);

	g_btnLockSettings = new Button(0, "On", 0, 0, g_btn_width, g_btn_height / 2.0f, g_settings.lock_settings, true);
	g_btnPostFaderMeter = new Button(0, "Post Fader", 0, 0, g_btn_width, g_btn_height / 2.0f, false, false);
	g_btnShowOfflineDevices = new Button(0, "On", 0, 0, g_btn_width, g_btn_height / 2.0f, g_settings.show_offline_devices, !g_settings.lock_settings);
	g_btnLockToMixMIX = new Button(0, "MIX", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.lock_to_mix == "MIX", !g_settings.lock_settings);

	for (int n = 0; n < 2; n++) {
		g_btnLockToMixAUX[n] = new Button(0, "AUX " + to_string(n + 1), 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, 
			g_settings.lock_to_mix == "AUX " + to_string(n + 1), !g_settings.lock_settings);
	}
	for (int n = 0; n < 4; n++) {
		g_btnLockToMixCUE[n] = new Button(0, "CUE " + to_string(n + 1), 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, 
			g_settings.lock_to_mix == "CUE " + to_string(n + 1), !g_settings.lock_settings);
	}

	g_btnLabelAUX1[0] = new Button(0, "AUX", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "AUX", !g_settings.lock_settings);
	g_btnLabelAUX1[1] = new Button(0, "FX", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "FX", !g_settings.lock_settings);
	g_btnLabelAUX1[2] = new Button(0, "Reverb", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "Reverb", !g_settings.lock_settings);
	g_btnLabelAUX1[3] = new Button(0, "Delay", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux1 == "Delay", !g_settings.lock_settings);

	g_btnLabelAUX2[0] = new Button(0, "AUX", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "AUX", !g_settings.lock_settings);
	g_btnLabelAUX2[1] = new Button(0, "FX", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "FX", !g_settings.lock_settings);
	g_btnLabelAUX2[2] = new Button(0, "Reverb", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "Reverb", !g_settings.lock_settings);
	g_btnLabelAUX2[3] = new Button(0, "Delay", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.label_aux2 == "Delay", !g_settings.lock_settings);

//	g_btnLockToMixHP = new Button(0, "HP", 0, 0, g_btn_width / 2.0f, g_btn_height / 2.0f, g_settings.lock_to_mix == "HP", !g_settings.lock_settings);

	g_btnReconnectOn = new Button(0, "On", 0, 0, g_btn_width, g_btn_height / 2.0f, g_settings.reconnect_time != 0, !g_settings.lock_settings);

	g_btnServerlistScan = new Button(0, "Scan network", 0, 0, g_btn_width, g_btn_height / 2.0f, true, !g_settings.lock_settings);

	g_btnInfo = new Button(0, "Info", 0, 0, g_btn_width, g_btn_height / 2.0f, false, true);
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
	
//	SAFE_DELETE(g_btnLockToMixHP);
	SAFE_DELETE(g_btnReconnectOn);
	SAFE_DELETE(g_btnServerlistScan);
	SAFE_DELETE(g_btnInfo);
}

Button *addSendButton(string name)
{
	g_draw_mutex.lock();
	Button* btn = new Button(ID_BTN_SENDS + g_btnSends.size(), name);
	g_btnSends.push_back({ name, btn });
	g_draw_mutex.unlock();

	updateLayout();
	updateSubscriptions();
	setRedrawWindow(true);

	return btn;
}

void updateChannelWidthButton() {
	switch (g_settings.channel_width) {
	case 1:
		g_btnChannelWidth->text = "View:\nNarrow";
		break;
	case 3:
		g_btnChannelWidth->text = "View:\nWide";
		break;
	default:
		g_btnChannelWidth->text = "View:\nRegular";
	}
}

void updateConnectButtons()
{
	int w,h;
	SDL_GetWindowSize(g_window,&w,&h);

	string connected_btn_text = getCheckedConnectionButton();

	cleanUpConnectionButtons();

	int sz = min((int)g_ua_server_list.size(), UA_MAX_SERVER_LIST);

	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	for(int i = 0; i < g_ua_server_list.size(); i++)
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
			g_btnConnect.push_back(new Button(ID_BTN_CONNECT + i, btn_text,
				w - g_channel_offset_x + round(SPACE_X + g_btn_width * 0.13f),
				g_channel_offset_y + g_btnConnect.size() * (g_btn_height + SPACE_Y),
				g_btn_width, g_btn_height, (btn_text == connected_btn_text)
			));

			if (g_btnConnect.size() >= UA_MAX_SERVER_LIST)
				break;
		}
	}

	if (g_btnSettings->checked) {
		releaseSettingsDialog();
		initSettingsDialog();
	}

	setRedrawWindow(true);

	//wenn keine verbindung, verbinde automatisch mit dem ersten server
	if (g_btnConnect.size() == 1 && connected_btn_text.length() == 0)
	{
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		event.type = SDL_USEREVENT;
		event.user.code = EVENT_CONNECT;
		SDL_PushEvent(&event);
	}
}

void updateAllMuteBtnText() {
	if (g_btnMuteAll) {
		if (g_settings.lock_to_mix.length()) {
			g_btnMuteAll->text = "Mute " + g_settings.lock_to_mix;
		}
		else {
			g_btnMuteAll->text = "Mute All";
		}
	}
}

GFXSurface* loadGfx(string filename, bool keepRAM = false) {
	string path = getDataPath(filename);
	GFXSurface *gs = gfx->LoadGfx(path);
	if (!gs) {
		toLog("loadGfx " + path + " failed");
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

	if (!(g_gsFader = loadGfx("fader.gfx", true)))
		return false;

	if (!(g_gsPan = loadGfx("poti.gfx")))
		return false;

	if (!(g_gsPanPointer = loadGfx("poti_pointer.gfx")))
		return false;

	for (int n = 0; n < 4; n++) {
		if (!(g_gsLabel[n] = loadGfx("label" + to_string(n + 1) + ".gfx", true)))
			return false;
	}

	return true;
}

void releaseAllGfx()
{
	for (int n = 0; n < 2; n++)
	{
		gfx->DeleteSurface(g_gsButtonYellow[n]);
		gfx->DeleteSurface(g_gsButtonRed[n]);
		gfx->DeleteSurface(g_gsButtonGreen[n]);
		gfx->DeleteSurface(g_gsButtonBlue[n]);
	}
	gfx->DeleteSurface(g_gsChannelBg);
	gfx->DeleteSurface(g_gsBgRight);
	gfx->DeleteSurface(g_gsFaderrail);
	gfx->DeleteSurface(g_gsFader);
	gfx->DeleteSurface(g_gsPan);
	gfx->DeleteSurface(g_gsPanPointer);
	gfx->DeleteSurface(g_gsLabel[0]);
	gfx->DeleteSurface(g_gsLabel[1]);
	gfx->DeleteSurface(g_gsLabel[2]);
	gfx->DeleteSurface(g_gsLabel[3]);
}

void initSettingsDialog() {
	
	g_btnServerlistScan->checked = true;

	for (vector<string>::iterator it = g_ua_server_list.begin(); it != g_ua_server_list.end(); ++it) {
		bool listed = false;
		for (int n = 0; n < UA_MAX_SERVER_LIST; n++) {
			if (*it == g_settings.serverlist[n]) {
				g_btnServerlistScan->checked = false;
				listed = true;
				break;
			}
		}
		g_btnsServers.push_back(new Button(0, *it, 0, 0, g_btn_width, g_btn_height / 2.0f, listed, !g_settings.lock_settings));

		if (g_btnsServers.size() >= UA_MAX_SERVER_LIST_SETTING)
			break;
	}
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

		if (!g_settings.lock_to_mix.length() && btnSend->text == selected_send)
		{
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				if (itB->second->id == btnSend->id)
					continue;
				itB->second->checked = false;
			}
			g_btnMix->checked = false;

			btnSend->checked = true;
		}
	}
	catch (simdjson_error e)
	{
		result = false;
	}
	
	// disable mix-button if locked to different mix
	if (g_settings.lock_to_mix.length()) {
		if (g_settings.lock_to_mix == "MIX") {
			g_btnMix->enabled = true;
			g_btnMix->checked = true;
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				itB->second->enabled = false;
				itB->second->checked = false;
			}
		}
		else {
			g_btnMix->enabled = false;
			g_btnMix->checked = false;
			btnSend->enabled = (btnSend->text == g_settings.lock_to_mix);
			btnSend->checked = (btnSend->text == g_settings.lock_to_mix);
		}
	}

	return result;
}

bool loadServerSettings(string server_name, UADevice* dev) // läd channel-select & group-setting
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

		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
			if (it->second->device->id != dev->id) {
				continue;
			}
			string type_str = it->second->type == INPUT ? ".input." : ".aux.";
			try
			{
				it->second->selected_to_show = element["ua_server"][it->second->device->id + type_str + it->second->id + " selected"];

				//override als selected, wenn name mit ! beginnt
				if (it->second->isOverriddenShow())
					it->second->selected_to_show = true;
				//override als nicht selected, wenn name mit # endet
				else if (it->second->isOverriddenHide())
					it->second->selected_to_show = false;
			}
			catch (simdjson_error e) {}
			try
			{
				int64_t value = element["ua_server"][it->second->device->id + type_str + it->second->id + " group"];
				it->second->fader_group = (int)value;
			}
			catch (simdjson_error e) {}
		}

		g_browseToChannel = (int)((int64_t)element["ua_server"]["first_visible_channel"]);
	}
	catch (simdjson_error e)
	{
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

		if (g_btnMix->checked)
		{
			json += "\"MIX\",\n";
		}
		else
		{
			for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
				if (itB->second->checked)
				{
					json += "\"" + itB->second->text + "\",\n";
					break;
				}
			}
		}

		string ua_dev;
		int channel = getMiddleSelectedChannel();
		json += "\"first_visible_channel\": " + to_string(channel) + ",\n";


		bool set_komma = false;
		for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {

			if (set_komma) {
				json += ",\n";
			}			

			string type_str = it->second->type == INPUT ? ".input." : ".aux.";
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
		catch (simdjson_error e) {}

		try {
			this->show_offline_devices = element["general"]["show_offline_devices"];
		}
		catch (simdjson_error e) {}

		try {
			string_view sv = element["general"]["lock_to_mix"];
			this->lock_to_mix = string{sv};
		}
		catch (simdjson_error e) {
			this->lock_to_mix = "";
		}

		try {
			string_view sv = element["general"]["label_aux1"];
			this->label_aux1 = string{ sv };
		}
		catch (simdjson_error e) {
			this->label_aux1 = "AUX 1";
		}

		try {
			string_view sv = element["general"]["label_aux2"];
			this->label_aux2 = string{ sv };
		}
		catch (simdjson_error e) {
			this->label_aux2 = "AUX 2";
		}

		updateAllMuteBtnText();

		try {
			int64_t reconnect_time = element["general"]["reconnect_time"];
			this->reconnect_time = reconnect_time;
		}
		catch (simdjson_error e) {
			this->reconnect_time = 10000;
		}

		try {
			this->extended_logging = element["general"]["extended_logging"];
		}
		catch (simdjson_error e) {}

		try {
			this->maximized = element["window"]["maximized"];
			this->fullscreen = element["window"]["fullscreen"];
		}
		catch (simdjson_error e) {}

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
			this->channel_width = channel_width;
		}
		catch (simdjson_error e) {
			this->channel_width = 0;
		}

		for (int n = 0; n < UA_MAX_SERVER_LIST; n++) {
			try {
				string_view sv = element["serverlist"][to_string(n+1)];
				this->serverlist[n] = string{sv};
			}
			catch (simdjson_error e) {
				this->serverlist[n] = "";
			}
		}
	}
	catch (simdjson_error e) {
		return false;
	}

	return true;
}

string Settings::toJSON() {
    string json;

    json = "{\n";

    //STARTUP
    json+= "\"general\": {\n";
    if(this->lock_settings)
        json+="\"lock_settings\": true,\n";
    else
        json+="\"lock_settings\": false,\n";
	if (this->show_offline_devices)
		json += "\"show_offline_devices\": true,\n";
	else
		json += "\"show_offline_devices\": false,\n";
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
        toLog("error loading asset manager");
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

	clearLog(APP_NAME + " " + APP_VERSION);

	unsigned int startup_delay = 0; // in ms

	for (int n = 0; n < argc; n++) {
		string arg = argv[n];
		size_t split = arg.find_first_of('=', 0);

		if (split != string::npos) {
			string key = arg.substr(0, split);
			string value = arg.substr(split + 1);

			if (key == "delay") {
				startup_delay = stoul(value);
			}
		}
	}

#ifdef _WIN32
	SetProcessDPIAware();
#endif

#ifndef __ANDROID__
	if (!g_settings.load())
	{
		toLog("settings.load failed");

		SDL_Rect rc;
		if (SDL_GetDisplayBounds(0, &rc) != 0) {
			toLog("SDL_GetDisplayBounds failed: " + string(SDL_GetError()));
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
        toLog("SDL_GetDisplayBounds failed: " + string(SDL_GetError()));
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","SDL_GetDisplayBounds failed", NULL);
        return 1;
    }

    g_settings.x = 0;
    g_settings.y = 0;
    g_settings.w=rc.w;
    g_settings.h=rc.h;
#endif

	createStaticButtons();

	if (!g_settings.extended_logging) toLog("Extended logging is deactivated.\nYou can activate it in settings.json but keep in mind that it will slow down performance.");
	
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

	try {
		gfx = new GFXEngine(WND_TITLE, g_settings.x, g_settings.y, g_settings.w, g_settings.h, flag, &g_window);
	}
	catch (invalid_argument e) {
		toLog("InitGfx failed : " + string(SDL_GetError()));
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","InitGfx failed", NULL);
		return 1;
	}
	SDL_SetWindowMinimumSize(g_window, 320, 240);
	if (loadAllGfx()) {
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
        while (running) {
            if (SDL_WaitEventTimeout(&e, 33) > 0)
                //	while (SDL_PollEvent(&e) > 0)
            {
                switch (e.type) {
                    case SDL_FINGERDOWN: {
                        if (!g_btnSettings->checked) {
                            onTouchDown(NULL, &e.tfinger);
                        }
                        break;
                    }
                    case SDL_FINGERUP: {
                        if (!g_btnSettings->checked) {
                            onTouchUp(false, &e.tfinger);
                        }
                        break;
                    }
                    case SDL_FINGERMOTION: {
                        if (!g_btnSettings->checked) {
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

                            SDL_Point pt = {e.button.x, e.button.y};

                            if (g_btnInfo->checked) {
                                g_btnInfo->checked = false;
                                setRedrawWindow(true);
                            } else {
                                if (g_btnSettings->isClicked(&pt)) {
                                    g_btnSettings->checked = !g_btnSettings->checked;
                                    if (g_btnSettings->checked) {
                                        initSettingsDialog();
                                    } else {
                                        releaseSettingsDialog();
                                    }
                                    setRedrawWindow(true);
                                    handled = true;
                                }

                                if (g_btnSettings->checked) {
                                    if (g_btnInfo->isClicked(&pt)) {
                                        g_btnInfo->checked = true;
                                        setRedrawWindow(true);
                                    }
                                    if (g_btnLockSettings->isClicked(&pt)) {
                                        g_btnLockSettings->checked = !g_btnLockSettings->checked;
                                        g_settings.lock_settings = g_btnLockSettings->checked;

                                        if (g_settings.lock_settings)
                                            g_settings.save();

           
										g_btnPostFaderMeter->enabled = !g_btnLockSettings->checked && !g_ua_devices.empty();
										g_btnShowOfflineDevices->enabled = !g_btnLockSettings->checked;
                                        g_btnLockToMixMIX->enabled = !g_btnLockSettings->checked;
//                                           g_btnLockToMixHP->enabled = true;
										for (int n = 0; n < 2; n++) {
											g_btnLockToMixAUX[n]->enabled = !g_btnLockSettings->checked;
										}
										for (int n = 0; n < 4; n++) {
											g_btnLockToMixCUE[n]->enabled = !g_btnLockSettings->checked;
											g_btnLabelAUX1[n]->enabled = !g_btnLockSettings->checked;
											g_btnLabelAUX2[n]->enabled = !g_btnLockSettings->checked;
										}

                                        g_btnReconnectOn->enabled = !g_btnLockSettings->checked;

                                        g_btnServerlistScan->enabled = !g_btnLockSettings->checked;
                                        for (vector<Button *>::iterator it = g_btnsServers.begin();
                                                it != g_btnsServers.end(); ++it) {
                                            (*it)->enabled = !g_btnLockSettings->checked;
                                        }

                                        setRedrawWindow(true);
									}
									else if (g_btnPostFaderMeter->isClicked(&pt)) {
										g_btnPostFaderMeter->checked = !g_btnPostFaderMeter->checked;

										if (g_btnPostFaderMeter->checked) {
											tcpClientSend("set /PostFaderMetering/value true");
										}
										else {
											tcpClientSend("set /PostFaderMetering/value false");
										}

										setRedrawWindow(true);
									}
									else if (g_btnShowOfflineDevices->isClicked(&pt)) {
										g_btnShowOfflineDevices->checked = !g_btnShowOfflineDevices->checked;
										g_settings.show_offline_devices = g_btnShowOfflineDevices->checked;
										updateMaxPages();
										setRedrawWindow(true);
									}
									else if (g_btnLockToMixMIX->isClicked(&pt)) {
                                        updateLockToMixSetting(g_btnLockToMixMIX);
									}
/*									else if (g_btnLockToMixHP->isClicked(&pt)) {
                                        updateLockToMixSetting(g_btnLockToMixHP);
									}
*/									
									else if (g_btnReconnectOn->isClicked(&pt)) {
										g_btnReconnectOn->checked = !g_btnReconnectOn->checked;
										g_settings.reconnect_time = g_btnReconnectOn->checked ? 10000 : 0;
										setRedrawWindow(true);
									}
									else if (g_btnServerlistScan->isClicked(&pt)) {
                                        g_btnServerlistScan->checked = true;

                                        for (vector<Button *>::iterator it = g_btnsServers.begin();
                                             it != g_btnsServers.end(); ++it) {
                                            (*it)->checked = false;
                                        }
                                        for (int i = 0; i < UA_MAX_SERVER_LIST; i++) {
                                            g_settings.serverlist[i] = "";
                                        }

                                        g_serverlist_defined = false;
                                        disconnect();
                                        getServerList();

                                        setRedrawWindow(true);
                                    }
									else {
										for (int n = 0; n < 2; n++) {
											if (g_btnLockToMixAUX[n]->isClicked(&pt)) {
												updateLockToMixSetting(g_btnLockToMixAUX[n]);
												break;
											}
										}
										for (int n = 0; n < 4; n++) {
											if (g_btnLockToMixCUE[n]->isClicked(&pt)) {
												updateLockToMixSetting(g_btnLockToMixCUE[n]);
												break;
											}
										}
										for (int n = 0; n < 4; n++) {
											if (g_btnLabelAUX1[n]->isClicked(&pt)) {
												updateLabelAux1(g_btnLabelAUX1[n]);
												break;
											}
										}
										for (int n = 0; n < 4; n++) {
											if (g_btnLabelAUX2[n]->isClicked(&pt)) {
												updateLabelAux2(g_btnLabelAUX2[n]);
												break;
											}
										}

                                        bool updateBtns = false;
                                        for (vector<Button *>::iterator it = g_btnsServers.begin();
                                             it != g_btnsServers.end(); ++it) {
                                            if ((*it)->isClicked(&pt)) {

                                                if ((*it)->checked) {
                                                    int count = 0;
                                                    for (vector<Button *>::iterator it2 = g_btnsServers.begin();
                                                         it2 != g_btnsServers.end(); ++it2) {
                                                        if ((*it2)->checked)
                                                            count++;
                                                    }
                                                    if (count > 1) {
                                                        (*it)->checked = false;
                                                        if ((*it)->text == g_ua_server_connected) {
                                                            disconnect();
                                                            g_ua_server_last_connection = -1;
                                                        }
                                                    }
                                                } else {
                                                    if (g_btnServerlistScan->checked) {
                                                        g_btnServerlistScan->checked = false;
                                                        disconnect();
                                                        g_ua_server_last_connection = -1;
                                                    }
                                                    int count = 0;
                                                    for (vector<Button *>::iterator it2 = g_btnsServers.begin();
                                                         it2 != g_btnsServers.end(); ++it2) {
                                                        if ((*it2)->checked)
                                                            count++;
                                                    }
                                                    if (count < UA_MAX_SERVER_LIST) {
                                                        (*it)->checked = true;
                                                        g_serverlist_defined = true;
                                                    }
                                                }

                                                for (int i = 0; i < UA_MAX_SERVER_LIST; i++) {
                                                    g_settings.serverlist[i] = "";
                                                }
                                                int i = 0;
                                                for (vector<Button *>::iterator it2 = g_btnsServers.begin();
                                                     it2 != g_btnsServers.end(); ++it2) {
                                                    if ((*it2)->checked) {
                                                        g_settings.serverlist[i] = (*it2)->text;
                                                        i++;
                                                    }
                                                }

                                                updateBtns = true;
                                                setRedrawWindow(true);
                                            }
                                        }
                                        if (updateBtns) {
                                            updateConnectButtons();
                                        }
                                    }
                                }
								else {
                                    if (g_msg.length()) {
                                        g_msg = "";
                                        setRedrawWindow(true);
                                    }
									//click Mute All Button
									if (g_btnMuteAll->isClicked(&pt)) {
										g_btnMuteAll->checked = !g_btnMuteAll->checked;

										if (g_btnMuteAll->checked) {
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

														if (send->bypass) {
															g_channelsMutedBeforeAllMute.insert(itB->first + "." + it->first);
														}
														if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == itB->second->text) {
															send->pressBypass(ON);
														}
													}
												}
											}
										}
										else {
											for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {

												if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == "MIX") {
													if (g_channelsMutedBeforeAllMute.find("MIX." + it->first) ==
														g_channelsMutedBeforeAllMute.end()) {
														it->second->pressMute(OFF);
													}
												}

												for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
													if (g_settings.lock_to_mix.length() == 0 || g_settings.lock_to_mix == itB->first) {
														if (g_channelsMutedBeforeAllMute.find(itB->first + "." + it->first) ==
															g_channelsMutedBeforeAllMute.end()) {
															Send* send = it->second->getSendByName(itB->first);
															if (send) {
																send->pressBypass(OFF);
															}
														}
													}
												}
											}
											g_channelsMutedBeforeAllMute.clear();
										}

										setRedrawWindow(true);
										handled = true;
									}
                                    //Click Send Buttons
									for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                        if (itB->second->isClicked(&pt)) {

											for (vector<pair<string, Button*>>::iterator itB2 = g_btnSends.begin(); itB2 != g_btnSends.end(); ++itB2) {
                                                itB2->second->checked = false;
                                            }
                                            g_btnMix->checked = false;
                                            itB->second->checked = true;

                                            updateSubscriptions();
                                            setRedrawWindow(true);
                                            handled = true;
                                        }
                                    }
                                    //click Mix Buttons
                                    if (g_btnMix->isClicked(&pt)) {

										for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                            itB->second->checked = false;
                                        }
                                        g_btnMix->checked = true;

                                        updateSubscriptions();
                                        setRedrawWindow(true);
                                        handled = true;
                                    }
                                    //Click Connect Buttons
                                    for (int sel = 0; sel < g_btnConnect.size(); sel++) {
                                        if (g_btnConnect[sel]->isClicked(&pt)) {
                                            if (!g_btnConnect[sel]->checked) {
                                                disconnect();
                                                connect(g_btnConnect[sel]->id - ID_BTN_CONNECT);
                                            } else {
                                                disconnect();
                                            }
                                            handled = true;
                                        }
                                    }
                                    //Click Select Channels
                                    if (g_btnSelectChannels->isClicked(&pt)) {
										g_browseToChannel = getMiddleSelectedChannel();
                                        g_btnSelectChannels->checked = !g_btnSelectChannels->checked;
										g_btnReorderChannels->checked = false;
                                        if (!g_btnSelectChannels->checked) {
                                            updateLayout();
                                        }
                                        updateMaxPages();
										browseToSelectedChannel(g_browseToChannel);
                                        updateSubscriptions();
                                        setRedrawWindow(true);
                                        handled = true;
                                    }
									//Click Reorder Channels
									else if (g_btnReorderChannels->isClicked(&pt)) {
										g_browseToChannel = getMiddleSelectedChannel();
										g_btnReorderChannels->checked = !g_btnReorderChannels->checked;
										g_btnSelectChannels->checked = false;
										if (!g_btnReorderChannels->checked) {
											updateLayout();
										}
										updateMaxPages();
										browseToSelectedChannel(g_browseToChannel);
										updateSubscriptions();
										setRedrawWindow(true);
										handled = true;
									}
                                    if (g_btnChannelWidth->isClicked(&pt)) {

                                        if (g_settings.channel_width == 0)
                                            g_settings.channel_width = 2;

                                        g_settings.channel_width++;
                                        if (g_settings.channel_width > 3)
                                            g_settings.channel_width = 1;

                                        updateLayout();
                                        updateChannelWidthButton();
                                        updateMaxPages();
										browseToSelectedChannel(g_browseToChannel);
                                        updateSubscriptions();
                                        setRedrawWindow(true);
                                        handled = true;
                                    }
                                    if (g_btnPageLeft->isClicked(&pt)) {
                                        if (g_page > 0) {
                                            g_page--;
                                            updateSubscriptions();
											g_browseToChannel = getMiddleSelectedChannel();
                                            setRedrawWindow(true);
                                        }
                                        handled = true;
                                    }
                                    else if (g_btnPageRight->isClicked(&pt)) {
                                        if (g_page < (getAllChannelsCount(false) - 1) / calculateChannelsPerPage()) {
                                            g_page++;
                                            updateSubscriptions();
											g_browseToChannel = getMiddleSelectedChannel();
                                            setRedrawWindow(true);
                                        }
                                        handled = true;
                                    }

                                    if (!handled) {
										SDL_Rect rc = { (int)g_channel_offset_x, 0 };
                                        SDL_GetWindowSize(g_window, &rc.w, &rc.h);
                                        rc.w -= 2 * g_channel_offset_x;
                                        if (g_ua_server_connected.length() == 0 && SDL_PointInRect(&pt, &rc))//offline
                                        {
                                            if (!IS_UA_SERVER_REFRESHING) {
                                                getServerList();
                                            }
                                        }
										else {
                                            if (e.button.clicks == 2) {
                                                Vector2D cursor_pt = {(float) e.button.x, (float) e.button.y};

												Channel* channel = getChannelByPosition(cursor_pt);
                                                if (channel) {
                                                    handled = true;
                                                    if (!g_btnSelectChannels->checked && !g_btnReorderChannels->checked) {

                                                        Vector2D relative_cursor_pt = cursor_pt;
                                                        relative_cursor_pt.subtractX(g_channel_offset_x);
                                                        relative_cursor_pt.setX((int) relative_cursor_pt.getX() % (int) g_channel_width);
                                                        relative_cursor_pt.subtractY(g_channel_offset_y);

                                                        if (channel->isTouchOnPan(&relative_cursor_pt)) {
                                                            if (g_btnMix->checked) {
                                                                if (channel->stereo)
                                                                    channel->changePan(-1, true);
                                                                else
                                                                    channel->changePan(0, true);
                                                            }
															else {
																for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                                                    if (itB->second->checked) {
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
                                                            if (g_btnMix->checked) {
                                                                channel->changePan2(1, true);
                                                            }
                                                            setRedrawWindow(true);
                                                        }
														else if (channel->isTouchOnFader(&relative_cursor_pt)) //level
                                                        {
                                                            if (g_btnMix->checked) {
                                                                channel->changeLevel(DB_UNITY, true);
                                                            }
															else {
																for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                                                    if (itB->second->checked) {
																		Send* send = channel->getSendByName(itB->first);
																		if (send) {
																			send->changeGain(DB_UNITY, true);
																		}
                                                                        break;
                                                                    }
                                                                }
                                                            }
                                                            setRedrawWindow(true);
                                                        }
                                                    }
                                                }
                                                if (!handled) {
                                                    SDL_Event event;
                                                    event.type = SDL_KEYDOWN;
                                                    event.key.keysym.sym = SDLK_f;
                                                    SDL_PushEvent(&event);
                                                }
                                            }
                                            if (e.button.which != SDL_TOUCH_MOUSEID) {
                                                Vector2D pt = Vector2D(e.button.x, e.button.y);
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
                        if (!g_btnSettings->checked) {
                            SDL_Event *p_event = &e;

                            //only use last mousemotion event
                            SDL_Event events[16];
                            SDL_PumpEvents();
                            int ev_num = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_MOUSEMOTION,
                                                        SDL_MOUSEMOTION);

                            if (ev_num) {
                                p_event = &events[ev_num - 1];
                            }

                            if (p_event->motion.which != SDL_TOUCH_MOUSEID) {
                                if (p_event->motion.state & SDL_BUTTON_LMASK) {
                                    Vector2D pt = Vector2D(p_event->button.x, p_event->button.y);
                                    onTouchDrag(&pt, NULL);
                                }
                            }
                        }
                        break;
                    }
                    case SDL_MOUSEBUTTONUP: {
                        if (!g_btnSettings->checked) {
                            if (e.button.which != SDL_TOUCH_MOUSEID) {
                                if (e.button.button == SDL_BUTTON_LEFT) {
                                    onTouchUp(true, NULL);
                                }
                            }
                        }
                        break;
                    }
                    case SDL_MOUSEWHEEL: {
                        if (!g_btnSettings->checked) {
                            if (g_msg.length()) {
                                g_msg = "";
                                setRedrawWindow(true);
                            }

                            int x, y;
                            SDL_GetMouseState(&x, &y);
                            Vector2D cursor_pt = {(float) x, (float) y};

                            Channel *channel = getChannelByPosition(cursor_pt);
                            if (channel) {
                                Vector2D relative_cursor_pt = cursor_pt;
                                relative_cursor_pt.subtractX(g_channel_offset_x);
                                relative_cursor_pt.setX((int) relative_cursor_pt.getX() % (int) g_channel_width);
                                relative_cursor_pt.subtractY(g_channel_offset_y);

                                double move = (float) e.wheel.y / 50;

                                if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                                    move = -move;
                                }

                                if (channel->isTouchOnPan(&relative_cursor_pt)) {
                                    if (g_btnMix->checked) {
                                        channel->changePan(move * 2);
                                    }
									else {
										for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                            if (itB->second->checked) {
												Send* send = channel->getSendByName(itB->first);
												if (send) {
													send->changePan(move);
												}
                                                break;
                                            }
                                        }
                                    }
                                    setRedrawWindow(true);
                                }
								else if (channel->isTouchOnPan2(&relative_cursor_pt)) {
                                    if (g_btnMix->checked) {
                                        channel->changePan2(move * 2);
                                        setRedrawWindow(true);
                                    }
                                }
								else if (channel->isTouchOnFader(&relative_cursor_pt)) {
                                    if (channel->fader_group) {
										for (unordered_map<string, Channel*>::iterator it = g_channelsById.begin(); it != g_channelsById.end(); ++it) {
                                            if (it->second->isVisible(false)) {
                                                if (it->second->fader_group == channel->fader_group) {
                                                    if (g_btnMix->checked) {
														it->second->changeLevel(move);
                                                        setRedrawWindow(true);
                                                    } else {
														for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                                            if (itB->second->checked) {
																Send* send = channel->getSendByName(itB->first);
																if (send) {
																	send->changeGain(move);
																	setRedrawWindow(true);
																}
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    } else {
                                        if (g_btnMix->checked) {
                                            channel->changeLevel(move);
                                            setRedrawWindow(true);
                                        } else {
											for (vector<pair<string, Button*>>::iterator itB = g_btnSends.begin(); itB != g_btnSends.end(); ++itB) {
                                                if (itB->second->checked) {
													Send* send = channel->getSendByName(itB->first);
													if (send) {
														send->changeGain(move);
														setRedrawWindow(true);
													}
                                                    break;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        break;
                    }
                    case SDL_KEYDOWN: {
                        switch (e.key.keysym.sym) {
                            case SDLK_ESCAPE: {
                                SDL_SetWindowFullscreen(g_window, 0);
                                break;
                            }
                            case SDLK_f: {
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
                                } else
                                    SDL_SetWindowFullscreen(g_window,
                                                            SDL_WINDOW_FULLSCREEN_DESKTOP);
                                SDL_WarpMouseGlobal(x, y);
                                break;
                            }
                        }
                        break;
                    }
                    case SDL_DISPLAYEVENT: {
                        if (e.display.event == SDL_DISPLAYEVENT_ORIENTATION) {
                            int w, h;
                            SDL_GetWindowSize(g_window, &w, &h);
                            float win_width = (float) w;
                            float win_height = (float) h;
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
                                setRedrawWindow(true);
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
                        if (e.user.code == EVENT_CONNECT) {
                            disconnect();
                            if (g_ua_server_last_connection == -1 && g_btnConnect.size() > 0) {
                                connect(g_btnConnect[0]->id - ID_BTN_CONNECT);
                            } else {
                                connect(g_ua_server_last_connection);
                            }
                        } else if (e.user.code == EVENT_DISCONNECT) {
                            disconnect();
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
        toLog("loadGfx failed");
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
	terminateAllPingThreads();
    disconnect();

#ifdef _WIN32
    ReleaseNetwork();
#endif

    cleanUpConnectionButtons();
    cleanUpStaticButtons();

    gfx->DeleteFont(g_fntMain);
    gfx->DeleteFont(g_fntInfo);
    gfx->DeleteFont(g_fntChannelBtn);
    gfx->DeleteFont(g_fntLabel);
    gfx->DeleteFont(g_fntFaderScale);
    gfx->DeleteFont(g_fntOffline);
    releaseAllGfx();

    SAFE_DELETE(gfx);
}