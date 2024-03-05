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

#define EVENT_CONNECT		1
#define EVENT_DISCONNECT	2

GFXEngine *gfx = NULL;

SDL_Window *g_window=NULL;
SDL_TimerID g_timer_network_serverlist = 0;
SDL_TimerID g_timer_network_timeout = 0;
SDL_TimerID g_timer_network_send_timeout_msg = 0;

Settings g_settings;

string g_msg = "";

int g_ua_server_list_index = 0;
string g_ua_server_list[UA_MAX_SERVER_LIST] = {};
string g_ua_server_connected = "";

mutex g_mutex_redraw;
bool g_redraw = true;
bool g_serverlist_defined = false;

//Channel Layout (werden in UpdateLayout-Funktion berechnet)
const float MIN_BTN_WIDTH = 60.0f;
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
float g_offline_fontsize;
float g_btnbold_fontsize;
float g_faderscale_fontsize;

int g_connection_btns = 0;
Button *g_btnConnect = NULL; //dyn. button array
Button *g_btnSends=NULL; //dyn. button array
Button *g_btnSelectChannels=NULL;
Button *g_btnChannelWidth=NULL;
Button *g_btnPageLeft=NULL;
Button *g_btnPageRight=NULL;
Button *g_btnMix=NULL;
Button *g_btnInfo=NULL;

int g_sends_count = 0;
TCPClient *g_tcpClient = NULL;
int g_page = 0;

GFXFont *g_fntMain = NULL;
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

unsigned long long g_server_refresh_start_time = -UA_SERVER_RESFRESH_TIME;
UADevice g_ua_devices[MAX_UA_DEVICES];

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

int CalculateMinChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 3.0f;
	int channels = floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int CalculateAverageChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 5.0f;
	int channels = floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int CalculateMaxChannelsPerPage() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float mixer_width = (float)w - 2.0f * (MIN_BTN_WIDTH * 1.2f + SPACE_X);
	float mixer_height = round((float)h / 1.02f);

	float max_channel_width = mixer_height / 9.0f;
	int channels = floor(mixer_width / max_channel_width);

	return channels < 1 ? 1 : channels;
}

int CalculateChannelsPerPage() {
	
	switch (g_settings.channel_width) {
	case 1:
		return CalculateMaxChannelsPerPage();
	case 3:
		return CalculateMinChannelsPerPage();
	}

	return CalculateAverageChannelsPerPage();
}

void UpdateMaxPages() {
	int max_pages = (GetAllChannelsCount(false) - 1) / CalculateChannelsPerPage();
	if (g_page >= max_pages) {
		g_page = max_pages;
	}
}

void UpdateLayout() {

	int channels_per_page = CalculateChannelsPerPage();
	
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

		g_fntLabel = gfx->CreateFontFromFile(getLocalPath("data/ChelseaMarket-Regular.ttf"), (int)round(g_channel_label_fontsize * 96.0f / 100.0f));
	}

	float new_main_fontsize = min(g_btn_height / 3.0f, g_btn_width / 7.0f);
	//reload mainfont if size changed
	if (g_main_fontsize != new_main_fontsize) {
		g_main_fontsize = new_main_fontsize;

		if (g_fntMain) {
			gfx->DeleteFont(g_fntMain);
			g_fntMain = NULL;
		}

		g_fntMain = gfx->CreateFontFromFile(getLocalPath("data/OpenSans-VariableFont_wdth,wght.ttf"), (int)round(g_main_fontsize * 96.0f / 100.0f));
	}

	float new_faderscale_fontsize = min(g_channel_btn_size / 4.0f, g_channel_label_fontsize);
	if (g_faderscale_fontsize != new_faderscale_fontsize) {
		g_faderscale_fontsize = new_faderscale_fontsize;

		if (g_fntFaderScale) {
			gfx->DeleteFont(g_fntFaderScale);
			g_fntFaderScale = NULL;
		}

		g_fntFaderScale = gfx->CreateFontFromFile(getLocalPath("data/OpenSans-VariableFont_wdth,wght.ttf"), (int)round(g_faderscale_fontsize * 96.0f / 100.0f));
	}

	float new_btnbold_fontsize = g_channel_btn_size / 3;
	if (g_btnbold_fontsize != new_btnbold_fontsize) {
		g_btnbold_fontsize = new_btnbold_fontsize;

		if (g_fntChannelBtn) {
			gfx->DeleteFont(g_fntChannelBtn);
			g_fntChannelBtn = NULL;
		}

		g_fntChannelBtn = gfx->CreateFontFromFile(getLocalPath("data/OpenSans-VariableFont_wdth,wght.ttf"), (int)round(g_btnbold_fontsize * 96.0f / 100.0f), false);
	}

	float new_offline_fontsize = min(win_width / 6, win_height / 3);

	//reload font if size changed
	if (g_offline_fontsize != new_offline_fontsize) {
		g_offline_fontsize = new_offline_fontsize;

		if (g_fntOffline) {
			gfx->DeleteFont(g_fntOffline);
			g_fntOffline = NULL;
		}

		g_fntOffline = gfx->CreateFontFromFile(getLocalPath("data/Changa-VariableFont_wght.ttf"), (int)round(g_offline_fontsize * 96.0f / 100.0f));
	}

	//btnleiste links
	g_btnInfo->rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
	g_btnInfo->rc.y = round(g_channel_offset_y);
	g_btnInfo->rc.w = round(g_btn_width);
	g_btnInfo->rc.h = round(g_btn_height);

	float sends_offset_y = (win_height - g_channel_offset_y - (g_btn_height + SPACE_Y) * g_sends_count) / 2.0f;
	for (int n = 0; n < g_sends_count; n++)
	{
		g_btnSends[n].rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
		g_btnSends[n].rc.y = round(sends_offset_y + (g_btn_height + SPACE_Y) * n);
		g_btnSends[n].rc.w = round(g_btn_width);
		g_btnSends[n].rc.h = round(g_btn_height);
	}

	g_btnMix->rc.x = round(g_channel_offset_x - g_btn_width * 1.18f + SPACE_X);
	g_btnMix->rc.y = round(win_height - g_channel_offset_y - g_btn_height);
	g_btnMix->rc.w = round(g_btn_width);
	g_btnMix->rc.h = round(g_btn_height);

	//btnleiste rechts
	for (int n = 0; n < g_connection_btns; n++)
	{
		g_btnConnect[n].rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
		g_btnConnect[n].rc.y = round(g_channel_offset_y + n * (g_btn_height + SPACE_Y));
		g_btnConnect[n].rc.w = round(g_btn_width);
		g_btnConnect[n].rc.h = round(g_btn_height);
	}

	g_btnSelectChannels->rc.x = round(win_width - g_channel_offset_x + SPACE_X + g_btn_width * 0.13f);
	g_btnSelectChannels->rc.y = round(win_height / 2.0f - (g_btn_height + SPACE_Y / 2.0f));
	g_btnSelectChannels->rc.w = round(g_btn_width);
	g_btnSelectChannels->rc.h = round(g_btn_height);

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

bool Button::IsClicked(SDL_Point *pt)
{
	if(this->visible && this->enabled)
	{
		return (bool)SDL_PointInRect(pt, &this->rc);
	}
	return false;
}

void Button::DrawButton(int color)
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

	if (color == BTN_COLOR_RED)
	{
		gfx->Draw(g_gsButtonRed[btn_switch], this->rc.x, this->rc.y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);
	}
	else if (color == BTN_COLOR_GREEN)
	{
		gfx->Draw(g_gsButtonGreen[btn_switch], this->rc.x, this->rc.y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);
	}
	else if (color == BTN_COLOR_BLUE)
	{
		gfx->Draw(g_gsButtonBlue[btn_switch], this->rc.x, this->rc.y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);
	}
	else
	{
		gfx->Draw(g_gsButtonYellow[btn_switch], this->rc.x, this->rc.y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);
	}

	COLORREF clr = RGB(0, 0, 0);
	if (!this->enabled)
	{
		clr = RGB(100, 100, 100);
	}

	float max_width = (float)this->rc.w * 0.9;
	Vector2D sz = gfx->GetTextBlockSize(g_fntMain, this->text, GFX_CENTER | GFX_AUTOBREAK, max_width);
	Vector2D szText(max_width, this->rc.h);
	gfx->SetColor(g_fntMain, clr);
	gfx->Write(g_fntMain, this->rc.x + ((float)this->rc.w - max_width) / 2.0f, this->rc.y + this->rc.h / 2 - sz.getY() / 2, this->text, GFX_CENTER | GFX_AUTOBREAK, &szText);
}

Touchpoint::Touchpoint() {
	memset(this, 0, sizeof(Touchpoint));
}

ChannelIndex::ChannelIndex(int _deviceIndex, int _inputIndex) {
	this->deviceIndex = _deviceIndex;
	this->inputIndex = _inputIndex;
}
bool ChannelIndex::IsValid() {
	return (this->deviceIndex != -1 && this->inputIndex != -1);
}

Send::Send()
{
	this->Clear();
}
void Send::Clear()
{
	this->ua_id = "";
	// this->name = "";
	this->gain = 0;
	// this->gainDb = 0;
	this->pan = 0;
	this->bypass = false;
	this->channel = NULL;
	this->meter_level = -80.0;
	this->meter_level2 = -80.0;
	this->subscribed = false;
}

void Send::ChangePan(double pan_change, bool absolute)
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

		string msg = "set /devices/" + this->channel->device->ua_id + "/inputs/" + this->channel->ua_id + "/sends/" + this->ua_id + "/Pan/value/ " + to_string(this->pan);
		UA_TCPClientSend(msg.c_str());
	}
}

void Send::ChangeGain(double gain_change, bool absolute)
{
	double _gain = this->gain + gain_change;
	if (absolute)
		_gain = gain_change;

	if (_gain > 1)
	{
		_gain = 1;
	}
	else if (_gain < 0)
	{
		_gain = 0;
	}

	if (this->gain != _gain)
	{
		this->gain = _gain;

		string msg = "set /devices/" + this->channel->device->ua_id + "/inputs/" + this->channel->ua_id + "/sends/" + this->ua_id + "/GainTapered/value/ " + to_string(this->gain);
		UA_TCPClientSend(msg.c_str());
	}
}

void Send::PressBypass(int state)
{
	if (state == SWITCH)
		this->bypass = !this->bypass;
	else
		this->bypass = (bool)state;

	string value = "true";
	if (!this->bypass)
		value = "false";

	string msg = "set /devices/" + this->channel->device->ua_id + "/inputs/" + this->channel->ua_id + "/sends/" + this->ua_id + "/Bypass/value/ " + value;
	UA_TCPClientSend(msg.c_str());
}

Channel::Channel()
{
	this->label_gfx = rand() % 4;
	this->label_rotation = (float)(rand() % 100) / 2000 - 0.025;

	this->Clear();
}
Channel::~Channel()
{
	this->sendCount = 0;
	if (this->sends)
	{
		delete[] this->sends;
		this->sends = NULL;
	}
}
void Channel::Clear()
{
	this->ua_id = "";
	this->name = "";
	this->level = 0;
	// this->levelDb = 0;
	this->pan = 0;
	this->solo = false;
	this->mute = false;
	memset(&this->touch_point,0,sizeof(Touchpoint));
	this->device = NULL;
	this->sends = NULL;
	this->sendCount = 0;
	this->stereo = false;
	this->stereoname = "";
	this->pan2 = 0;
	this->hidden = true;
	this->enabledByUser = false;
	this->active = false;
	this->selected_to_show = true;

	this->fader_group = 0;

	this->meter_level = -80.0;
	this->meter_level2 = -80.0;

	this->subscribed = false;
}
void Channel::AllocSends(int _sendCount)
{
	this->sendCount = _sendCount;
	this->sends = new Send[_sendCount];
}
void Channel::LoadSends(int sendIndex, string us_sendId)
{
	this->sends[sendIndex].ua_id = us_sendId;
	this->sends[sendIndex].channel = this;
}
void Channel::SubscribeSend(bool subscribe, int n)
{
	if(n >= 0 && n < this->sendCount && this->sends[n].ua_id.length()) {

		if (this->sends[n].subscribed != subscribe) {

			this->sends[n].subscribed = subscribe;

			string root = "";
			if (!subscribe) {
				root += "un";
			}
			root += "subscribe /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/sends/" + this->sends[n].ua_id;

			string cmd = root + "/GainTapered/";
			UA_TCPClientSend(cmd.c_str());

			//	cmd = root + "/Gain/";
			//	UA_TCPClientSend(cmd.c_str());

			cmd = root + "/Bypass/";
			UA_TCPClientSend(cmd.c_str());

			cmd = root + "/Pan/";
			UA_TCPClientSend(cmd.c_str());

			cmd = root + "/meters/0/MeterLevel/";
			UA_TCPClientSend(cmd.c_str());

			cmd = root + "/meters/1/MeterLevel/";
			UA_TCPClientSend(cmd.c_str());

			//	cmd = root + "/Pan2/";
			//	UA_TCPClientSend(cmd.c_str());

			//	cmd = "get /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/sends/" + to_string(n);
			//	UA_TCPClientSend(cmd.c_str());
		}
	}
}

bool Channel::IsTouchOnFader(Vector2D *pos)
{
	int y = (g_fader_label_height + g_channel_pan_height + g_channel_btn_size);
	int height = g_channel_height - y - g_channel_btn_size;
	return (pos->getX() > 0 && pos->getX() < g_channel_width
		&& pos->getY() >y && pos->getY() < y + height);
}

bool Channel::IsTouchOnPan(Vector2D *pos)
{
	int width = g_channel_width;

	if (this->stereo)
		width /= 2;

	return (pos->getX() > 0 && pos->getX() < width
		&& pos->getY() > g_fader_label_height && pos->getY() < (g_fader_label_height + g_channel_pan_height));
}

bool Channel::IsTouchOnPan2(Vector2D *pos)
{
	int width = g_channel_width / 2;

	return (pos->getX() > width && pos->getX() < g_channel_width
		&& pos->getY() > g_fader_label_height && pos->getY() < (g_fader_label_height + g_channel_pan_height));
}

bool Channel::IsTouchOnGroup1(Vector2D *pos)
{
	int x = (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_channel_height - g_channel_btn_size;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::IsTouchOnGroup2(Vector2D *pos)
{
	int x = g_channel_width / 2 + (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_channel_height - g_channel_btn_size;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::IsTouchOnMute(Vector2D *pos)
{
	int x = g_channel_width / 2 + (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

bool Channel::IsTouchOnSolo(Vector2D *pos)
{
	int x = (g_channel_width / 2 - g_channel_btn_size) / 2;
	int y = g_fader_label_height + g_channel_pan_height;

	return (pos->getX() > x && pos->getX() < x + g_channel_btn_size
		&& pos->getY() > y && pos->getY() < y + g_channel_btn_size);
}

void Channel::ChangePan(double pan_change, bool absolute)
{
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
		string msg = "set /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/Pan/value/ " + to_string(this->pan);
		UA_TCPClientSend(msg.c_str());
	}
}

void Channel::ChangePan2(double pan_change, bool absolute)
{
	double _pan = this->pan2 + pan_change;
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

	char msg[256];
	if (this->pan2 != _pan)
	{
		this->pan2 = _pan;
		string msg = "set /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/Pan2/value/ " + to_string(this->pan2);
		UA_TCPClientSend(msg.c_str());
	}
}

void Channel::ChangeLevel(double level_change, bool absolute)
{
	double _level = this->level + level_change;

	if (absolute)
		_level = level_change;

	if (_level > 1)
	{
		_level = 1;
	}
	else if (_level < 0)
	{
		_level = 0;
	}

	if (this->level != _level)
	{
		this->level = _level;

		string msg;
		msg = "set /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/FaderLevelTapered/value/ " + to_string(_level);
		UA_TCPClientSend(msg.c_str());
	}
}

void Channel::PressMute(int state)
{
	if (state == SWITCH)
		this->mute = !this->mute;
	else
		this->mute = (bool)state;


	string value = "true";
	if (!this->mute)
		value = "false";

	string msg = "set /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/Mute/value/ " + value;
	UA_TCPClientSend(msg.c_str());
}

void Channel::PressSolo(int state)
{
	if (state == SWITCH)
		this->solo = !this->solo;
	else
		this->solo = (bool)state;

	string value = "true";
	if (!this->solo)
		value = "false";

	string msg = "set /devices/" + this->device->ua_id + "/inputs/" + this->ua_id + "/Solo/value/ " + value;
	UA_TCPClientSend(msg.c_str());
}

bool Channel::IsOverriddenShow(bool ignoreStereoname) {
	string name = this->name;
	if (!ignoreStereoname && this->stereo)
		name = this->stereoname;

	return (name.length() > 0 && name.substr(0, 1) == "!");
}

bool Channel::IsOverriddenHide(bool ignoreStereoname) {
	string name = this->name;
	if (!ignoreStereoname && this->stereo)
		name = this->stereoname;

	return (name.length() > 0 && name.substr(0, 1) == "#");
}

UADevice::UADevice()
{
	ua_id = "";
	inputsCount = 0;
	inputs = NULL;
	auxsCount = 0;
	auxs = NULL;
}
UADevice::~UADevice()
{
	this->Release();
}
void UADevice::LoadDevice(string us_deviceId)
{
	this->ua_id = us_deviceId;

	//more device info
	//	UA_TCPClientSend("get /devices/" + this->ua_id + "/");

	string msg = "subscribe /devices/" + this->ua_id + "/DeviceOnline/";
	UA_TCPClientSend(msg.c_str());

//	msg = "subscribe /devices/" + this->ua_id;
//	UA_TCPClientSend(msg.c_str());

//	msg = "subscribe /MeterPulse/";
//	UA_TCPClientSend(msg.c_str());
}
void UADevice::AllocChannels(int type, int _channelCount)
{
	int *channelsCount = NULL;
	Channel **channels = NULL;

	if (type == UA_AUX)
	{
		channelsCount = &this->auxsCount;
		channels = &this->auxs;
	}
	else
	{
		channelsCount = &this->inputsCount;
		channels = &this->inputs;
	}

	*channelsCount = _channelCount;
	*channels = new Channel[_channelCount];
}
bool UADevice::IsChannelVisible(int type, int index, bool only_selected)
{
	int *channelsCount = NULL;
	Channel *channels = NULL;

	if (type == UA_AUX)
	{
		channelsCount = &this->auxsCount;
		channels = this->auxs;
	}
	else
	{
		channelsCount = &this->inputsCount;
		channels = this->inputs;
	}

	if (*channelsCount < 1 || !channels)
		return false;

	if (only_selected && !channels[index].selected_to_show)
		return false;

	bool b_stereo_hide = false;
	
	if ((index % 2) == 1)
	{
		b_stereo_hide = channels[index].stereo;
	}

	return (this->ua_id.length() && !channels[index].hidden && !b_stereo_hide
		&& channels[index].enabledByUser && channels[index].active);
}
bool UADevice::LoadChannels(int type, int channelIndex, string us_inputId)
{
	int *channelsCount = NULL;
	Channel *channels = NULL;

	if (type == UA_AUX)
	{
		channelsCount = &this->auxsCount;
		channels = this->auxs;
	}
	else
	{
		channelsCount = &this->inputsCount;
		channels = this->inputs;
	}

	if (channelIndex > *channelsCount)
		return false;

	channels[channelIndex].ua_id = us_inputId;
	channels[channelIndex].device = this;

	string root = "subscribe /devices/" + this->ua_id + "/inputs/" + channels[channelIndex].ua_id;
	//	string root = "subscribe /devices/" + this->ua_id + "/bus/" + this->channels[channelIndex].ua_id;

	string cmd = root + "/ChannelHidden/";
	UA_TCPClientSend(cmd.c_str());

	cmd = root + "/EnabledByUser/";
	UA_TCPClientSend(cmd.c_str());

	cmd = root + "/Active/";
	UA_TCPClientSend(cmd.c_str());

	cmd = root + "/Stereo/";
	UA_TCPClientSend(cmd.c_str());

	cmd = root + "/Name/";
	UA_TCPClientSend(cmd.c_str());

	cmd = root + "/StereoName/";
	UA_TCPClientSend(cmd.c_str());

	root = "get /devices/" + this->ua_id + "/inputs/" + to_string(channelIndex);
	cmd = root + "/sends";
	UA_TCPClientSend(cmd.c_str());

	return true;
}

void UADevice::SubscribeChannel(bool subscribe, int type, int n)
{
	int *channelsCount = NULL;
	Channel *channels = NULL;

	if (type == UA_AUX)
	{
		channelsCount = &this->auxsCount;
		channels = this->auxs;
	}
	else
	{
		channelsCount = &this->inputsCount;
		channels = this->inputs;
	}

	if(n >= 0 && n < *channelsCount)
	{
		if (channels[n].ua_id.length())
		{
			bool subscribeMix = subscribe && g_btnMix->checked;

			if (channels[n].subscribed != subscribeMix) {

				channels[n].subscribed = subscribeMix;

				string root = "";
				if (!subscribeMix) {
					root += "un";
				}
				root += "subscribe /devices/" + this->ua_id + "/inputs/" + channels[n].ua_id;
				//	string root = "subscribe /devices/" + this->ua_id + "/bus/" + this->channels[n].ua_id;

				string cmd = root + "/FaderLevelTapered/";
				UA_TCPClientSend(cmd.c_str());

				//	cmd = root + "/FaderLevel/";
				//	UA_TCPClientSend(cmd.c_str());

				cmd = root + "/Mute/";
				UA_TCPClientSend(cmd.c_str());

				cmd = root + "/Solo/";
				UA_TCPClientSend(cmd.c_str());

				cmd = root + "/Pan/";
				UA_TCPClientSend(cmd.c_str());

				cmd = root + "/Pan2/";
				UA_TCPClientSend(cmd.c_str());

				cmd = root + "/meters/0/MeterLevel/";
				UA_TCPClientSend(cmd.c_str());

				cmd = root + "/meters/1/MeterLevel/";
				UA_TCPClientSend(cmd.c_str());
			}
			
			for (int i = 0; i < g_sends_count; i++) {
				bool subscribeSend = subscribe && g_btnSends[i].checked;
				channels[n].SubscribeSend(subscribeSend, i);
			}
		}
	}
}

int UADevice::GetActiveChannelsCount(int type, int flag)
{
	int *channelsCount = NULL;
	Channel *channels = NULL;

	if (type == UA_AUX)
	{
		channelsCount = &this->auxsCount;
		channels = this->auxs;
	}
	else
	{
		channelsCount = &this->inputsCount;
		channels = this->inputs;
	}

	if (*channelsCount < 1 || !channels)
		return 0;

	int visibleChannelsCount = 0;
	int activeChannelsCount = 0;

	bool btn_select = (g_btnSelectChannels->checked);

	for (int n = 0; n < *channelsCount; n++)
	{
		if (!channels[n].enabledByUser || !channels[n].active)
			continue;
		if (this->IsChannelVisible(type, n, !btn_select))
			visibleChannelsCount++;
		activeChannelsCount++;
	}

	if(flag==UA_VISIBLE)
		return visibleChannelsCount;

	return activeChannelsCount;
}

void UADevice::ClearChannels() {
	if (this->inputs)
	{
		while (this->inputsCount > 0)
		{
			this->inputsCount--;
			this->SubscribeChannel(false, UA_INPUT, this->inputsCount);
			this->inputs[this->inputsCount].Clear();
		}
		delete[] inputs;
		inputs = NULL;
	}
	if (this->auxs)
	{
		while (this->auxsCount > 0)
		{
			this->auxsCount--;
			this->auxs[this->auxsCount].Clear();
		}
		delete[] auxs;
		auxs = NULL;
	}

	inputsCount = 0;
	auxsCount = 0;
}

void UADevice::Release()
{
	this->ClearChannels();
	ua_id = "";
}

void UpdateSubscriptions() {

	int count = 0;
	int channels_per_page = CalculateChannelsPerPage();

	for (int i = 0; i < MAX_UA_DEVICES; i++) {
		if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputsCount && g_ua_devices[i].inputs) {
			for (int n = 0; n < g_ua_devices[i].inputsCount; n++) {
				bool subscribe = false;
				if (g_ua_devices[i].IsChannelVisible(UA_INPUT, n, !g_btnSelectChannels->checked)) {
					if (count >= g_page * channels_per_page && count < (g_page + 1) * channels_per_page) {
						subscribe = true;
					}
					count++;
				}
				g_ua_devices[i].SubscribeChannel(subscribe, UA_INPUT, n);
			}
		}
	}
}

int GetAllChannelsCount(bool countWithHidden)
{
	int channelCount = 0;

	for (int i = 0; i < MAX_UA_DEVICES; i++)
	{
		if (g_ua_devices[i].ua_id.length())
		{
			if (countWithHidden)
				channelCount += g_ua_devices[i].GetActiveChannelsCount(UA_INPUT, UA_ALL_ENABLED_AND_ACTIVE);
			else
				channelCount += g_ua_devices[i].GetActiveChannelsCount(UA_INPUT, UA_VISIBLE);
		}
	}
	return channelCount;
}

Channel *GetChannelPtr(ChannelIndex *cIndex)
{
	if (!cIndex->IsValid())
		return NULL;

	return &g_ua_devices[cIndex->deviceIndex].inputs[cIndex->inputIndex];
}

Channel *GetChannelPtr(int deviceIndex, int inputIndex)
{
	ChannelIndex index = ChannelIndex(deviceIndex, inputIndex);
	return GetChannelPtr(&index);
}

UADevice *GetDevicePtrByUAId(string ua_device_id)
{
	if (ua_device_id.length() == 0)
		return NULL;

	for (int n = 0; n < MAX_UA_DEVICES; n++)
	{
		if (g_ua_devices[n].ua_id == ua_device_id)
		{
			return &g_ua_devices[n];
		}
	}
	return NULL;
}

Send *GetSendPtrByUAIds(Channel *channel, string ua_send_id)
{
	if (!channel || ua_send_id.length() == 0)
		return NULL;

	for (int n = 0; n < channel->sendCount; n++)
	{
		if (ua_send_id == channel->sends[n].ua_id)
		{
			return &channel->sends[n];
		}
	}

	return NULL;
}

int GetSendIndexByUAIds(Channel *channel, string ua_send_id)
{
	if (!channel || ua_send_id.length() == 0)
		return -1;

	for (int n = 0; n < channel->sendCount; n++)
	{
		if (ua_send_id == channel->sends[n].ua_id)
		{
			return n;
		}
	}

	return -1;
}

Channel *GetChannelPtrByUAIds(string ua_device_id, string ua_input_id)
{
	UADevice *dev = GetDevicePtrByUAId(ua_device_id);
	if (!dev)
		return NULL;

	if (ua_input_id.length() == 0)
		return NULL;

	if (!dev->inputs || dev->inputsCount < 1)
		return NULL;

	for (int n = 0; n < dev->inputsCount; n++)
	{
		if (dev->inputs[n].ua_id == ua_input_id)
		{
			return &dev->inputs[n];
		}
	}
	return NULL;
}

ChannelIndex GetChannelIdByTouchpointId(bool _is_mouse, SDL_TouchFingerEvent *touch_input)
{
	if (!touch_input && !_is_mouse)
		return ChannelIndex(-1, -1);

	bool btn_select = (g_btnSelectChannels->checked);

	for (int i = 0; i < MAX_UA_DEVICES; i++)
	{
		if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputsCount && g_ua_devices[i].inputs)
		{
			for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
			{
				if (!g_ua_devices[i].IsChannelVisible(UA_INPUT, n, !btn_select))
					continue;

				if (touch_input && g_ua_devices[i].inputs[n].touch_point.id == touch_input->touchId 
					&& g_ua_devices[i].inputs[n].touch_point.finger_id == touch_input->fingerId
					|| _is_mouse && g_ua_devices[i].inputs[n].touch_point.is_mouse)
				{
					return ChannelIndex(i, n);
				}
			}
		}
	}

	return ChannelIndex(-1, -1);
}

ChannelIndex GetChannelIdByPosition(Vector2D pt) // client-position; e.g. touch / mouseclick
{
	//offset
	pt.subtractX(g_channel_offset_x);
	pt.subtractY(g_channel_offset_y);

	if (pt.getY() >= 0 && pt.getY() < g_channel_height)
	{
		double div = (double)pt.getX() / g_channel_width;

		int channels_per_page = CalculateChannelsPerPage();

		div = floor(div);
		if (div >= 0 && div < channels_per_page)
		{
			int deviceIndex = 0;
			int channelIndex = (int)div;

			channelIndex += g_page * channels_per_page;

			for (int i = 0; i < MAX_UA_DEVICES; i++)
			{
				if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputsCount && g_ua_devices[i].inputs)
				{
					int visibleChannelsCount = g_ua_devices[i].GetActiveChannelsCount(UA_INPUT, UA_VISIBLE);
					if (channelIndex >= visibleChannelsCount)
					{
						channelIndex -= visibleChannelsCount;
						deviceIndex = i + 1;
						continue;
					}
					else
					{
						bool btn_select = (g_btnSelectChannels->checked);
						for (int n = 0; n <= channelIndex; n++)
						{
							if (!g_ua_devices[i].IsChannelVisible(UA_INPUT, n, !btn_select))
							{
								channelIndex++;
							}
						}
						break;
					}
				}
			}


			//überprüfen ob channel existiert
			if (g_ua_devices[deviceIndex].ua_id.length() == 0)
			{
				deviceIndex = -1;
				channelIndex = -1;
			}
			else if(channelIndex >= g_ua_devices[deviceIndex].inputsCount)
			{
				channelIndex = -1;
			}

			return ChannelIndex(deviceIndex, channelIndex);
		}
	}

	return ChannelIndex(-1,-1);
}

static int UA_PingServer(void *param)
{
	if (!param)
		return 0;

	char *ip = (char*)param;

	if (g_ua_server_list_index >= UA_MAX_SERVER_LIST)
	{
		free(param);
		return 0;
	}

	try
	{
		TCPClient *tcpClient = new TCPClient(ip, UA_TCP_PORT, NULL);

		char computername[NI_MAXHOST];
		GetComputerNameByIP(ip, computername);

		bool exists = false;
		for (int n = 0; n < g_ua_server_list_index; n++)
		{
			if (g_ua_server_list[n] == string(computername))
			{
				exists = true;
				break;
			}
		}

		if (!exists)
		{
			g_ua_server_list[g_ua_server_list_index] = string(computername);
			g_ua_server_list_index++;

			UpdateConnectButtons();
			
			toLog("UA-Server found on " + g_ua_server_list[g_ua_server_list_index]);
		}

		tcpClient->Disconnect();
		delete tcpClient;
		tcpClient = NULL;
	}
	catch (char *error)
	{
//		char msg[256];
//		sprintf(msg, "No UA Server on %s\n", ip);
//		OutputDebugString(msg);
	}

	free(param);

	return 0;
}

void UA_GetServerListCallback(char *ip)
{
	if (strncmp(ip, "127.",4) == 0)
	{
		//check localhost
		void *scan_ip_buffer = malloc(sizeof(char) * 17);
		strcpy_s((char*)scan_ip_buffer, 17, ip);
		string thread_name = "PingUAServer on " + string((char*)scan_ip_buffer);
		SDL_CreateThread(UA_PingServer, thread_name.c_str(), scan_ip_buffer);
	}
	else
	{
		string ip_fragment = string(ip);
		int cut = ip_fragment.find_last_of('.');
		ip_fragment = ip_fragment.substr(0, cut + 1);

		for (int n = 0; n < 256; n++)
		{
			string full_ip = ip_fragment + to_string(n);
			void *scan_ip_buffer = malloc(sizeof(char) * 17);
			strcpy_s((char*)scan_ip_buffer, 17, full_ip.c_str());
			string thread_name = "PingUAServer on " + string((char*)scan_ip_buffer);
			SDL_Thread  *thread = SDL_CreateThread(UA_PingServer, thread_name.c_str(), scan_ip_buffer);
		}
	}
}


void CleanUpConnectionButtons()
{
	g_connection_btns = 0;
	if (g_btnConnect)
	{
		delete[] g_btnConnect;
		g_btnConnect = NULL;
	}
}

void CleanUpSendButtons()
{
	if (g_btnSends)
	{
		delete[] g_btnSends;
		g_btnSends = NULL;
	}
}

void SetRedrawWindow(bool redraw)
{
	const lock_guard<mutex> lock(g_mutex_redraw);
	g_redraw = redraw;
}

bool GetRedrawWindow()
{
	const lock_guard<mutex> lock(g_mutex_redraw);
	return g_redraw;
}

Uint32 TimerCallbackRefreshServerList(Uint32 interval, void *param) //g_timer_network_serverlist
{
	if (!IS_UA_SERVER_REFRESHING)
	{
		SetRedrawWindow(true);
		SDL_RemoveTimer(g_timer_network_serverlist);
		g_timer_network_serverlist = 0;
		return 0;
	}

	SetRedrawWindow(true);
	return 1;
}

Uint32 TimerCallbackNetworkTimeout(Uint32 interval, void* param) //g_timer_network_timeout
{
	g_msg = "Network timeout";
	toLog(g_msg);
	Disconnect();

	SetRedrawWindow(true);
	SDL_RemoveTimer(g_timer_network_timeout);
	g_timer_network_timeout = 0;

	return 1;
}

Uint32 TimerCallbackNetworkSendTimeoutMsg(Uint32 interval, void* param) //g_timer_network_timeout
{
	UA_TCPClientSend("get /devices/0/Name/"); // just ask for sth to check if the connection is alive
	SDL_RemoveTimer(g_timer_network_send_timeout_msg);
	g_timer_network_send_timeout_msg = 0;

	return 1;
}

void SetNetworkTimeout() {
#ifndef SIMULATION

	if (g_timer_network_send_timeout_msg != 0) {
		SDL_RemoveTimer(g_timer_network_send_timeout_msg);
	}
	g_timer_network_send_timeout_msg = SDL_AddTimer(5000, TimerCallbackNetworkSendTimeoutMsg, NULL);

	if (g_timer_network_timeout != 0) {
		SDL_RemoveTimer(g_timer_network_timeout);
	}
	g_timer_network_timeout = SDL_AddTimer(10000, TimerCallbackNetworkTimeout, NULL);
#endif
}

#ifdef SIMULATION
bool UA_GetServerList()
{
	g_ua_server_list_index = 0;

	CleanUpConnectionButtons();

	SetRedrawWindow(true);

	g_ua_server_list[0] = "Simulation";
	g_ua_server_list_index++;

	UpdateConnectButtons();

	return true;
}
#endif

#ifndef SIMULATION
bool UA_GetServerList()
{
	if (g_serverlist_defined)
		return false;

	if (IS_UA_SERVER_REFRESHING)
		return false;

	g_ua_server_list_index = 0;

	CleanUpConnectionButtons();

	g_server_refresh_start_time = GetTickCount64(); //10sec

	if(g_timer_network_serverlist == 0)
		g_timer_network_serverlist = SDL_AddTimer(1000, TimerCallbackRefreshServerList, NULL);

	SetRedrawWindow(true);

	if (!GetClientIPs(UA_GetServerListCallback))
		return false;

	return true;
}
#endif

void UA_TCPClientProc(int msg, string data)
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
		SetNetworkTimeout();

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
			string path{sv};

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
			if (path_parameter[0] == "devices") // devices laden
			{
				if (path_parameter[1].length() == 0)
				{
					int i = 0;
					const dom::object obj = element["data"]["children"];

					//load device info
					for (const dom::key_value_pair device : obj)
					{
						string value{device.key};
						g_ua_devices[i].LoadDevice(value);
						i++;
					}
				}
			/*	else if (path_parameter[2] == "MultiUnitOrder")
				{
				/*	Disconnect();
					for (int sel = 0; sel < g_connection_btns; sel++) {
						if (g_btnConnect[sel].checked) {
							Connect(sel); // thread unsafe!
						}
					}
				}*/
				else if (path_parameter[2] == "DeviceOnline")
				{
					UADevice *dev = GetDevicePtrByUAId(path_parameter[1]);
					if (dev)
					{
						dev->online = element["data"];
						UpdateSubscriptions();

					//	string msg = "get /devices/" + dev->ua_id + "/MultiUnitOrder/value";
					//	UA_TCPClientSend(msg.c_str());

						if (dev->online) { // load device

							if (dev == &g_ua_devices[0]) { // CueBusCount nur für erstes device erfragen
								string msg = "subscribe /devices/" + dev->ua_id + "/CueBusCount";
								UA_TCPClientSend(msg.c_str());
							}

							string msg = "get /devices/" + dev->ua_id + "/inputs";
							UA_TCPClientSend(msg.c_str());
						}
						else {
							dev->ClearChannels();
							SetRedrawWindow(true);
						}
					}
				}
				else if (path_parameter[2] == "CueBusCount")
				{
					UADevice *dev = GetDevicePtrByUAId(path_parameter[1]);
					if (dev)
					{
						g_sends_count = (int)((int64_t)element["data"]) + 2; // +2 wegen aux
						CreateSendButtons(g_sends_count);

						// sendnamen abfragen
						for (int n = 0; n < g_sends_count; n++) {
							//namen der sends erfragen
							string msg = "get /devices/" + dev->ua_id + "/inputs/0/sends/" + to_string(n) + "/Name/value/";
							UA_TCPClientSend(msg.c_str());
						}

						SetRedrawWindow(true);
					}
				}
				if (path_parameter[2] == "inputs" && path_parameter[3] == "0" && 
					path_parameter[4] == "sends" && path_parameter[6] == "Name" && path_parameter[7] == "value") //sendsnames
				{
					//data
					string_view sv = element["data"];
					string value{ sv };
					value = unescape_to_utf8(value);

					int index = stoi(path_parameter[5]);
					if (index >= 0 && index < g_sends_count)
					{
						g_btnSends[index].text = value;
						LoadServerSettings(g_ua_server_connected, &g_btnSends[index]);
					}

					SetRedrawWindow(true);
				}
				else if (path_parameter[2] == "inputs" && path_parameter[3].length() == 0) // inputs laden
				{
					UADevice *dev = GetDevicePtrByUAId(path_parameter[1]);
					if (dev)
					{
						const dom::object obj = element["data"]["children"];

						int sz = obj.size();
						dev->AllocChannels(UA_INPUT, sz);

						int i = 0;
						//load device info
						for (const dom::key_value_pair device : obj)
						{
							string value{device.key};
							dev->LoadChannels(UA_INPUT, i, value);
							i++;
						}

						LoadServerSettings(g_ua_server_connected, dev);
					//	dev->SubscribeChannels(UA_INPUT);
					}
				}
				else if (path_parameter[2] == "auxs" && path_parameter[3].length() == 0) // aux laden
				{
					UADevice *dev = GetDevicePtrByUAId(path_parameter[1]);
					if (dev)
					{
						const dom::object obj = element["data"]["children"];

						int sz = obj.size();
						dev->AllocChannels(UA_AUX, sz);

						int i = 0;
						//load device info
						for (const dom::key_value_pair device : obj)
						{
							string value{ device.key };
							dev->LoadChannels(UA_INPUT, i, value);
							i++;
						}

						LoadServerSettings(g_ua_server_connected, dev);
					//	dev->SubscribeChannels(UA_INPUT);
					}
				}
				else if(path_parameter[2] == "inputs")// input-nachrichten verarbeiten
				{
					Channel *channel = GetChannelPtrByUAIds(path_parameter[1], path_parameter[3]);
					if (channel)
					{
						if (path_parameter[4] == "sends")//input sends
						{
							if (path_parameter[5].length() == 0)//inputs send enumeration
							{
								const dom::object obj = element["data"]["children"];

								int sz = obj.size();
								channel->AllocSends(sz);

								int i = 0;
								//load device info
								for (const dom::key_value_pair device : obj)
								{
									string value{device.key};
									channel->LoadSends(i, value);
									i++;
								}
							//	channel->SubscribeSends();
							}
							else
							{
								Send *send = GetSendPtrByUAIds(channel, path_parameter[5]);
								if (send)
								{
									if (path_parameter[6] == "meters")//inputs
									{
										if (path_parameter[7] == "0" && path_parameter[8] == "MeterLevel" && path_parameter[9] == "value")//inputs
										{
											double old_level = send->meter_level;
											send->meter_level = element["data"];

											if ((int)(old_level * 10) != (int)(send->meter_level * 10))
											{
												SetRedrawWindow(true);
											}
										}

										if (path_parameter[7] == "1" && path_parameter[8] == "MeterLevel" && path_parameter[9] == "value")//inputs
										{
											double old_level = send->meter_level2;
											send->meter_level2 = element["data"];

											if ((int)(old_level * 10) != (int)(send->meter_level2 * 10))
											{
												SetRedrawWindow(true);
											}
										}
									}
									else if (path_parameter[6] == "GainTapered" && send->channel->touch_point.action != TOUCH_ACTION_LEVEL)//sends
									{
										if (path_parameter[7] == "value")
										{
											send->gain = element["data"];
											SetRedrawWindow(true);
										}
									}
									/* else if (path_parameter[6] == "Gain")//sends
									{
										if (path_parameter[7] == "value")
										{
											send->gainDb = element["data"];
											SetRedrawWindow(true);
										}
									}*/
									else if (path_parameter[6] == "Pan" && send->channel->touch_point.action != TOUCH_ACTION_PAN)//sends
									{
										if (path_parameter[7] == "value")
										{
											send->pan = element["data"];
											SetRedrawWindow(true);
										}
									}
									else if (path_parameter[6] == "Bypass")//sends
									{
										if (path_parameter[7] == "value")
										{
											send->bypass = element["data"];
											SetRedrawWindow(true);
										}
									}
								}
							}
						}
						else if (path_parameter[4] == "meters")//inputs
						{
							if (path_parameter[5] == "0" && path_parameter[6] == "MeterLevel" && path_parameter[7] == "value")//inputs
							{
								double old_level = channel->meter_level;
								channel->meter_level = element["data"];

								if ((int)(old_level * 10) != (int)(channel->meter_level * 10))
								{
									SetRedrawWindow(true);
								}
							}
							if (path_parameter[5] == "1" && path_parameter[6] == "MeterLevel" && path_parameter[7] == "value")//inputs
							{
								double old_level = channel->meter_level2;
								channel->meter_level2 = element["data"];

								if ((int)(old_level * 10) != (int)(channel->meter_level2 * 10))
								{
									SetRedrawWindow(true);
								}
							}
						}
						else if (path_parameter[4] == "FaderLevelTapered" && channel->touch_point.action != TOUCH_ACTION_LEVEL)//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->level = element["data"];
								SetRedrawWindow(true);
							}
						}
						/* else if (path_parameter[4] == "FaderLevel")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->levelDb = element["data"];
								SetRedrawWindow(true);
							}
						}*/
						else if (path_parameter[4] == "Name")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								//data
								string_view sv = element["data"];
								string value{sv};
								channel->name = unescape_to_utf8(value);

								// kanal in selektion übernehmen, wenn name mit ! beginnt
								if (channel->IsOverriddenShow(true)) {
									channel->selected_to_show = true;
									UpdateSubscriptions();
								}
								// kanal ausblenden wenn name mit # beginnt
								else if (channel->IsOverriddenHide(true)) {
									channel->selected_to_show = false;
									int max_pages = (GetAllChannelsCount(false) - 1) / CalculateChannelsPerPage();
									if (g_page >= max_pages) {
										g_page = max_pages;
									}
									UpdateSubscriptions();
								}

								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "Pan" && channel->touch_point.action != TOUCH_ACTION_PAN)//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->pan = element["data"];
								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "Solo")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->solo = element["data"];
								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "Mute")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->mute = element["data"];
								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "Stereo")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->stereo = element["data"];
								UpdateSubscriptions();

								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "StereoName")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								//data
								string_view sv = element["data"];
								string value{sv};
								channel->stereoname = unescape_to_utf8(value);

								// kanal in selektion übernehmen, wenn name mit ! beginnt
								if (channel->IsOverriddenShow()) {
									channel->selected_to_show = true;
									UpdateSubscriptions();
								}
								// kanal ausblenden wenn name mit # beginnt
								else if (channel->IsOverriddenHide()) {
									channel->selected_to_show = false;
									int max_pages = (GetAllChannelsCount(false) - 1) / CalculateChannelsPerPage();
									if (g_page >= max_pages) {
										g_page = max_pages;
									}
									UpdateSubscriptions();
								}

								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "Pan2" && channel->touch_point.action != TOUCH_ACTION_PAN2)//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->pan2 = element["data"];
								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "ChannelHidden")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->hidden = element["data"];
								UpdateSubscriptions();
								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "EnabledByUser")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->enabledByUser = element["data"];
								UpdateSubscriptions();
								SetRedrawWindow(true);
							}
						}
						else if (path_parameter[4] == "Active")//inputs
						{
							if (path_parameter[5] == "value")//inputs
							{
								channel->active = element["data"];
								UpdateSubscriptions();
								SetRedrawWindow(true);
							}
						}
					}
				}
			}

			try
			{
				//ua-error
				string_view sv = element["error"];
				string s{sv};
				if(g_settings.extended_logging) toLog("UA-Error " + s);
			}
			catch (simdjson_error e) {}
		}
		catch (simdjson_error e)
		{
			if(g_settings.extended_logging) toLog("JSON-Error: " + string(e.what()));
		}
		tcp_msg = "";
		break;
	}
	}
}

void UA_TCPClientSend(const char *msg)
{
	if (g_tcpClient)
	{
		if(g_settings.extended_logging) toLog("UA -> " + string(msg));
		g_tcpClient->Send(msg);
	}
}

void DrawScaleMark(int x, int y, const char *txt, double scale_value, double total_scale) {
	double marker_y = total_scale - scale_value * total_scale;
	gfx->DrawShape(GFX_RECTANGLE, RGB(170, 170, 170), x + g_channel_width *0.25, y + g_fadertracker_height / 2 + marker_y, g_channel_width *0.1, 1, GFX_NONE, 0.9);
	Vector2D sz = gfx->GetTextBlockSize(g_fntFaderScale, "-1234567890");
	gfx->Write(g_fntFaderScale, x + g_channel_width * 0.24, y + g_fadertracker_height / 2 + marker_y - sz.getY() * 0.5, txt, GFX_RIGHT, NULL,
		GFX_NONE, 0.9);
}

void DrawChannel(ChannelIndex index, float _x, float _y, float _width, float _height) {
	float x = _x;
	float y = _y;
	float width = _width;
	float height = _height;
	Channel *channel = GetChannelPtr(&index);
	if (!channel)
		return;

	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	double level = 0;
	bool mute = false;
	double pan = 0;
	double pan2 = 0;
	double meter_level = -80.0;
	double meter_level2 = -80.0;

	Vector2D sz;
	Vector2D stretch;

	if (g_btnMix->checked) {
		level = channel->level;
		mute = channel->mute;
		pan = channel->pan;
		pan2 = channel->pan2;
		meter_level = channel->meter_level;
		meter_level2 = channel->meter_level2;
	}
	else if(channel) {
		for (int n = 0; n < channel->sendCount && n < g_sends_count; n++) {
			if (g_btnSends[n].checked) {
				level = channel->sends[n].gain;
				mute = channel->sends[n].bypass;
				pan = channel->sends[n].pan;
				meter_level = channel->sends[n].meter_level;
				meter_level2 = channel->sends[n].meter_level2;
				break;
			}
		}
	}

	bool gray = false;
	bool highlight = false;

	//SELECT CHANNELS
	if (g_btnSelectChannels->checked) {
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
		ChannelIndex prev_channel_index = GetChannelIdByPosition(pt);
		Channel *prev_channel = GetChannelPtr(&prev_channel_index);
		if(!prev_channel || !prev_channel->selected_to_show)
			gfx->DrawShape(GFX_RECTANGLE, clr, _x - SPACE_X, 0, SPACE_X, _height * 1.02, GFX_NONE, 0.5); // links

		gfx->DrawShape(GFX_RECTANGLE, clr, _x, 0, _width - SPACE_X, SPACE_Y, GFX_NONE, 0.5); // oben
		gfx->DrawShape(GFX_RECTANGLE, clr, _x, _height * 1.02 - SPACE_Y, _width - SPACE_X, SPACE_Y, GFX_NONE, 0.5); // unten
		gfx->DrawShape(GFX_RECTANGLE, clr, _x + _width - SPACE_X, 0, SPACE_X, _height * 1.02, GFX_NONE, 0.5); // rechts
	}

	//LABEL überspringen und am Ende zeichenen (wg. asugrauen)
	y += g_fader_label_height;
	height -= g_fader_label_height;

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
		if (channel->solo) {
			btn_switch = 1;
		}

		stretch = Vector2D(g_channel_btn_size, g_channel_btn_size);
		gfx->Draw(g_gsButtonRed[btn_switch], x + x_offset, y, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

		sz = gfx->GetTextBlockSize(g_fntChannelBtn, "S", GFX_CENTER);
		gfx->Write(g_fntChannelBtn, x + x_offset + g_channel_btn_size / 2, y + (g_channel_btn_size - sz.getY()) / 2, "S", GFX_CENTER);
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
	gfx->Draw(g_gsFaderrail, x + (width - g_faderrail_width) / 2, y + g_fadertracker_height / 2, NULL, GFX_NONE, 1.0, 0, NULL, &stretch);

	DrawScaleMark(x, y, "+12", UA_PLUS_12DB, fader_rail_height);
	DrawScaleMark(x, y, "+6", UA_PLUS_6DB, fader_rail_height);
	DrawScaleMark(x, y, "0", UA_UNITY, fader_rail_height);
	DrawScaleMark(x, y, "-6", UA_MINUS_6DB, fader_rail_height);
	DrawScaleMark(x, y, "-12", UA_MINUS_12DB, fader_rail_height);
	DrawScaleMark(x, y, "-20", UA_MINUS_20DB, fader_rail_height);
	DrawScaleMark(x, y, "-32", UA_MINUS_32DB, fader_rail_height);
	DrawScaleMark(x, y, "-56", UA_MINUS_56DB, fader_rail_height);
	DrawScaleMark(x, y, "-inf" , UA_MINUS_INFINITY, fader_rail_height);

	//tracker
	stretch = Vector2D(g_fadertracker_width, g_fadertracker_height);
	gfx->Draw(g_gsFader, x + (width - g_fadertracker_width) / 2, y + height - level * (height - g_fadertracker_height) - g_fadertracker_height, NULL, 
			GFX_NONE, 1.0, 0, NULL, &stretch);

	//LEVELMETER
	g_channel_slider_height = g_channel_height - g_fader_label_height - g_channel_pan_height;

	double led_height = round(UA_UNITY * fader_rail_height);
	double led_top = round(fader_rail_height - led_height);
	gfx->DrawShape(GFX_RECTANGLE, RGB(50, 50, 50), x + g_channel_width * 0.75, y + g_fadertracker_height / 2 + led_top, 5, led_height);
	double scale_level = meter_level / -76.0; // eigentlich -80?
	double level_height = round(led_height * (1 - scale_level));
	gfx->DrawShape(GFX_RECTANGLE, RGB(20, 200, 50), x + g_channel_width * 0.75 + 1, y + g_fadertracker_height / 2 + led_top + led_height - level_height, 3, level_height);

	if (channel->stereo) {
		gfx->DrawShape(GFX_RECTANGLE, RGB(50, 50, 50), x + g_channel_width * 0.75 + 5, y + g_fadertracker_height / 2 + led_top, 5, led_height);
		scale_level = meter_level2 / -76.0; // eigentlich -80?
		level_height = round(led_height * (1 - scale_level));
		gfx->DrawShape(GFX_RECTANGLE, RGB(20, 200, 50), x + g_channel_width * 0.75 + 1 + 5, y + g_fadertracker_height / 2 + led_top + led_height - level_height, 3, level_height);
	}

	if (gray) {
		gfx->DrawShape(GFX_RECTANGLE, RGB(20,20,20), _x, 0, _width, _height * 1.02, GFX_NONE, 0.7);
	}

	//selektion gesperrt (serverseitig überschrieben)
	if (g_btnSelectChannels->checked && (channel->IsOverriddenHide() || channel->IsOverriddenShow())) {
		gfx->DrawShape(GFX_RECTANGLE, RGB(255, 0, 0), _x, 0, _width, _height * 1.02, GFX_NONE, 0.1);

		gfx->SetColor(g_fntMain, WHITE);
		gfx->SetShadow(g_fntMain, BLACK, 1);
		sz = gfx->GetTextBlockSize(g_fntMain, "locked", GFX_CENTER);
		gfx->Write(g_fntMain, _x + _width / 2, 0 + (_height - sz.getY()) / 2, "locked", GFX_CENTER);
		gfx->SetShadow(g_fntMain, 0, 0);
	}

	//LABEL
	string name = channel->name;
	if (channel->stereo)
		name = channel->stereoname;

	stretch = Vector2D(_width * 1.1, g_fader_label_height);
	gfx->Draw(g_gsLabel[channel->label_gfx], _x - _width * 0.05, _y, NULL, GFX_NONE, 1.0, channel->label_rotation, NULL, &stretch);

	Vector2D max_size = Vector2D(_width , g_fader_label_height);

	do {
		sz = gfx->GetTextBlockSize(g_fntLabel, name, GFX_CENTER);
		if (sz.getX() > _width)
			name = name.substr(0, name.length() - 1);
	} while (sz.getX() > _width);

	gfx->Write(g_fntLabel, _x, _y + (g_fader_label_height - sz.getY()) / 2, name, GFX_CENTER, &max_size);
}

void Draw() {
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);
	float win_width = (float)w;
	float win_height = (float)h;
	Vector2D sz;
	Vector2D stretch;

	//background
	gfx->DrawShape(GFX_RECTANGLE, BLACK, 0, 0, win_width, win_height);

	int channels_per_page = CalculateChannelsPerPage();

	//bg right
	stretch = Vector2D(g_channel_offset_x, win_height);
	gfx->Draw(g_gsBgRight, g_channel_offset_x + g_channel_width * channels_per_page, 0, NULL, 
		GFX_NONE, 1.0, 0, NULL, &stretch);

	//bg left
	stretch = Vector2D(g_channel_offset_x, win_height);
	gfx->Draw(g_gsBgRight, 0, 0, NULL, GFX_HFLIP, 1.0, 0, NULL, &stretch);

	//draw buttons
	g_btnInfo->DrawButton(BTN_COLOR_GREEN);
	for (int n = 0; n < g_sends_count; n++) {
		g_btnSends[n].DrawButton(BTN_COLOR_BLUE);
	}
	g_btnMix->DrawButton(BTN_COLOR_BLUE);

	for (int n = 0; n < g_connection_btns; n++) {
		g_btnConnect[n].DrawButton(BTN_COLOR_GREEN);
	}
	g_btnChannelWidth->DrawButton(BTN_COLOR_GREEN);
	g_btnPageLeft->DrawButton(BTN_COLOR_GREEN);
	g_btnPageRight->DrawButton(BTN_COLOR_GREEN);
	g_btnSelectChannels->DrawButton(BTN_COLOR_GREEN);

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
				string text = (g_connection_btns == 0 ? "No servers found\n" : "");
				text += "Click to refresh the serverlist";
				gfx->Write(g_fntMain, win_width / 2, (win_height + sz.getY()) / 2, text, GFX_CENTER);
			}
		}
	}
	else {
		int count = 0;
		int offset = 0;
		
		//BG
		stretch = Vector2D(g_channel_width * channels_per_page, win_height);
		gfx->Draw(g_gsChannelBg, g_channel_offset_x, 0, NULL, 
			GFX_NONE, 1.0, 0, NULL, &stretch);

		for (int i = 0; i < MAX_UA_DEVICES; i++) {
			if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputsCount && g_ua_devices[i].inputs) {
				for (int n = 0; n < g_ua_devices[i].inputsCount; n++) {
					if (!g_ua_devices[i].IsChannelVisible(UA_INPUT, n, !g_btnSelectChannels->checked))
						continue;

					if (count >= g_page * channels_per_page && count < (g_page + 1) * channels_per_page) {
						float x = g_channel_offset_x + offset * g_channel_width;
						float y = g_channel_offset_y;

						//channel seperator
						gfx->DrawShape(GFX_RECTANGLE, BLACK, x + g_channel_width - SPACE_X / 2.0f, 0, SPACE_X, win_height, GFX_NONE, 0.7f);

						DrawChannel(ChannelIndex(i, n), x, y, g_channel_width, g_channel_height);

						offset++;
					}
					count++;
				}
			}
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

	gfx->Update();
}

#ifdef SIMULATION
void Connect(int connection_index)
{
	g_page = 0;

	if (g_ua_server_list[connection_index].length() == 0)
		return;

	g_ua_server_connected = g_ua_server_list[connection_index];

	g_ua_devices[0].ua_id = "0";
	g_ua_devices[0].AllocChannels(UA_INPUT, SIMULATION_CHANNEL_COUNT);
	for (int n = 0; n < SIMULATION_CHANNEL_COUNT; n++)
	{
		g_ua_devices[0].LoadChannels(UA_INPUT, n, to_string(n));
		g_ua_devices[0].inputs[n].name = "Analog " + to_string(n + 1);
		g_ua_devices[0].inputs[n].AllocSends(SIMULATION_SENDS_COUNT);
		for (int i = 0; i < SIMULATION_SENDS_COUNT; i++)
		{
			g_ua_devices[0].inputs[n].LoadSends(i, to_string(i));
			g_ua_devices[0].inputs[n].sends[i].name = "Send "+ to_string(i+1);
		}
	}

	g_btnSelectChannels->enabled=true;
	g_btnChannelWidth->enabled=true;
	g_btnPageLeft->enabled=true;
	g_btnPageRight->enabled=true;
	g_btnMix->enabled=true;

	SetRedrawWindow(true);
}
#endif

#ifndef SIMULATION
void Connect(int connection_index)
{
	g_page = 0;

	if (g_ua_server_list[connection_index].length() == 0)
		return;

	g_ua_server_connected = g_ua_server_list[connection_index];

	//connect to UA
	try
	{
		g_msg = "Connecting to " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT;
		Draw(); // force to draw

		g_tcpClient = new TCPClient(g_ua_server_list[connection_index].c_str(), UA_TCP_PORT, &UA_TCPClientProc);

		toLog("UA:  Connected on " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT);

		UA_TCPClientSend("get /devices");

		g_btnSelectChannels->enabled = true;
		g_btnChannelWidth->enabled = true;
		g_btnPageLeft->enabled = true;
		g_btnPageRight->enabled = true;
		g_btnMix->enabled = true;

		g_btnConnect[connection_index].checked = true;

		g_msg = "";

		SetNetworkTimeout();

		SetRedrawWindow(true);
	}
	catch (char *error)
	{
		g_msg = "Connection failed on " + g_ua_server_list[connection_index] + ":" + UA_TCP_PORT + ": " + string(error);

		toLog("UA:  " + g_msg);

		Disconnect();
	}
}
#endif

void Disconnect()
{
	if (g_timer_network_timeout != 0) {
		SDL_RemoveTimer(g_timer_network_timeout);
	}

	g_btnSelectChannels->enabled=false;
	g_btnChannelWidth->enabled=false;
	g_btnPageLeft->enabled=false;
	g_btnPageRight->enabled=false;
	g_btnMix->enabled=false;

	SaveServerSettings(g_ua_server_connected);
	g_ua_server_connected = "";

	if (g_tcpClient)
	{
		toLog("UA:  Disconnect from " + g_ua_server_list[0] + ":" + UA_TCP_PORT);

		g_tcpClient->Disconnect();
		delete g_tcpClient;
		g_tcpClient = NULL;
	}

	g_page = 0;

	g_sends_count = 0;
	CleanUpSendButtons();

	for (int n = 0; n < MAX_UA_DEVICES; n++)
	{
		if (g_ua_devices[n].ua_id.length())
		{
			g_ua_devices[n].Release();
		}
	}

	for (int n = 0; n < g_connection_btns; n++)
	{
		g_btnConnect[n].checked=false;
	}

	g_btnMix->checked=true;

	SetRedrawWindow(true);
}

string GetCheckedConnectionButton()
{
	for (int n = 0; n < g_connection_btns; n++)
	{
		if (g_btnConnect[n].checked)
		{
			return g_btnConnect[n].text;
		}
	}
	return "";
}

void OnTouchDown(Vector2D *mouse_pt, SDL_TouchFingerEvent *touchinput)
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

	ChannelIndex channelIndex = GetChannelIdByPosition(pos_pt);
	if (channelIndex.IsValid())
	{
		Channel *channel = GetChannelPtr(&channelIndex);
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

				//override als selected, wenn name mit ! beginnt
				if (channel->IsOverriddenShow())
					channel->selected_to_show = true;
				//override als nicht selected, wenn name mit # endet
				else if (channel->IsOverriddenHide())
					channel->selected_to_show = false;

				SetRedrawWindow(true);
			}
			else if (channel->IsTouchOnPan(&relative_pos_pt))
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
			else if (channel->IsTouchOnPan2(&relative_pos_pt))
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
			else if (channel->IsTouchOnMute(&relative_pos_pt))
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
						for (int i = 0; i < MAX_UA_DEVICES; i++)
						{
							if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
							{
								for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
								{
									if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
									{
										if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
										{
											g_ua_devices[i].inputs[n].PressMute(state);
											SetRedrawWindow(true);
										}
									}
								}
							}
						}
					}
					else
					{
						channel->PressMute();
						SetRedrawWindow(true);
					}
				}
				else
				{
					for (int v = 0; v < g_sends_count; v++)
					{
						if (g_btnSends[v].checked)
						{
							if (channel->fader_group)
							{
								int state = (int)!channel->sends[v].bypass;
								for (int i = 0; i < MAX_UA_DEVICES; i++)
								{
									if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
									{
										for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
										{
											if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
											{
												if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
												{
													g_ua_devices[i].inputs[n].sends[v].PressBypass(state);
													SetRedrawWindow(true);
												}
											}
										}
									}
								}
							}
							else
							{
								channel->sends[v].PressBypass();
								SetRedrawWindow(true);
							}
							break;
						}
					}
				}
			}
			else if (channel->IsTouchOnSolo(&relative_pos_pt))
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
						for (int i = 0; i < MAX_UA_DEVICES; i++)
						{
							if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
							{
								for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
								{
									if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
									{
										if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
										{
											g_ua_devices[i].inputs[n].PressSolo(state);
											SetRedrawWindow(true);
										}
									}
								}
							}
						}
					}
					else
					{
						channel->PressSolo();
						SetRedrawWindow(true);
					}
				}
			}
			else if (channel->IsTouchOnGroup1(&relative_pos_pt))
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

				SetRedrawWindow(true);
			}
			else if (channel->IsTouchOnGroup2(&relative_pos_pt))
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

				SetRedrawWindow(true);
			}
			else if (channel->IsTouchOnFader(&relative_pos_pt))
			{
			/*	string root = "unsubscribe /devices/" + channel->device->ua_id + "/inputs/" + channel->ua_id;
				string cmd = root + "/FaderLevelTapered";
				UA_TCPClientSend(cmd.c_str());*/

				if (channel->fader_group)
				{
					for (int i = 0; i < MAX_UA_DEVICES; i++)
					{
						if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
						{
							for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
							{
								if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
								{
									if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
									{
										if (touchinput)
										{
											g_ua_devices[i].inputs[n].touch_point.id = touchinput->touchId;
											g_ua_devices[i].inputs[n].touch_point.finger_id = touchinput->fingerId;
											g_ua_devices[i].inputs[n].touch_point.pt_start_x = pos_pt.getX();
											g_ua_devices[i].inputs[n].touch_point.pt_start_y = pos_pt.getY();
											g_ua_devices[i].inputs[n].touch_point.action = TOUCH_ACTION_LEVEL;
										}
										else if (mouse_pt)
										{
											g_ua_devices[i].inputs[n].touch_point.is_mouse = true;
											g_ua_devices[i].inputs[n].touch_point.pt_start_x = mouse_pt->getX();
											g_ua_devices[i].inputs[n].touch_point.pt_start_y = mouse_pt->getY();
											g_ua_devices[i].inputs[n].touch_point.action = TOUCH_ACTION_LEVEL;
										}
									}
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
}

void OnTouchDrag(Vector2D *mouse_pt, SDL_TouchFingerEvent *touchinput)
{
	ChannelIndex channelIndex = { -1, -1 };

	if (touchinput)
		channelIndex = GetChannelIdByTouchpointId(false, touchinput);
	else
		channelIndex = GetChannelIdByTouchpointId((bool)mouse_pt, NULL);

	if (channelIndex.IsValid())
	{
		Channel *channel = GetChannelPtr(&channelIndex);
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
				ChannelIndex hover_channelIndex = GetChannelIdByPosition(pos_pt);
				if (hover_channelIndex.IsValid())
				{
					Channel *hover_channel = GetChannelPtr(&hover_channelIndex);
					if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
					{
						hover_channel->selected_to_show = channel->selected_to_show;
						//override als selected, wenn name mit ! beginnt
						if (hover_channel->IsOverriddenShow())
							hover_channel->selected_to_show = true;
						//override als nicht selected, wenn name mit # endet
						else if (hover_channel->IsOverriddenHide())
							hover_channel->selected_to_show = false;
						SetRedrawWindow(true);
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_MUTE)
			{
				ChannelIndex hover_channelIndex = GetChannelIdByPosition(pos_pt);
				if (hover_channelIndex.IsValid())
				{
					Channel *hover_channel = GetChannelPtr(&hover_channelIndex);
					if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
					{
						Vector2D relative_pos_pt = pos_pt;
						relative_pos_pt.subtractX(g_channel_offset_x);
						relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
						relative_pos_pt.subtractY(g_channel_offset_y);

						if (hover_channel->IsTouchOnMute(&relative_pos_pt))
						{
							if (g_btnMix->checked)
							{
								if (hover_channel->fader_group)
								{
									int state = (int)channel->mute;
									for (int i = 0; i < MAX_UA_DEVICES; i++)
									{
										if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
										{
											for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
											{
												if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
												{
													if (g_ua_devices[i].inputs[n].fader_group == hover_channel->fader_group)
													{
														if (g_ua_devices[i].inputs[n].mute != (bool)state)
														{
															g_ua_devices[i].inputs[n].PressMute(state);
															SetRedrawWindow(true);
														}
													}
												}
											}
										}
									}
								}
								else
								{
									if (hover_channel->mute != channel->mute)
									{
										hover_channel->PressMute(channel->mute);
										SetRedrawWindow(true);
									}
								}
							}
							else
							{
								for (int v = 0; v < g_sends_count; v++)
								{
									if (g_btnSends[v].checked)
									{
										if (hover_channel->fader_group)
										{
											int state = (int)channel->sends[v].bypass;
											for (int i = 0; i < MAX_UA_DEVICES; i++)
											{
												if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
												{
													for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
													{
														if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
														{
															if (g_ua_devices[i].inputs[n].fader_group == hover_channel->fader_group)
															{
																if (g_ua_devices[i].inputs[n].sends[v].bypass != (bool)state)
																{
																	g_ua_devices[i].inputs[n].sends[v].PressBypass(state);
																	SetRedrawWindow(true);
																}
															}
														}
													}
												}
											}
										}
										else
										{
											if (hover_channel->sends[v].bypass != channel->sends[v].bypass)
											{
												hover_channel->sends[v].PressBypass(channel->sends[v].bypass);
												SetRedrawWindow(true);
											}
										}
										break;
									}
								}
							}
						}
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_SOLO)
			{
				ChannelIndex hover_channelIndex = GetChannelIdByPosition(pos_pt);
				if (hover_channelIndex.IsValid())
				{
					Channel *hover_channel = GetChannelPtr(&hover_channelIndex);
					if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
					{
						Vector2D relative_pos_pt = pos_pt;
						relative_pos_pt.subtractX(g_channel_offset_x);
						relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
						relative_pos_pt.subtractY(g_channel_offset_y);

						if (hover_channel->IsTouchOnSolo(&relative_pos_pt))
						{
							if (g_btnMix->checked)
							{
								if (hover_channel->fader_group)
								{
									int state = (int)channel->solo;
									for (int i = 0; i < MAX_UA_DEVICES; i++)
									{
										if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
										{
											for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
											{
												if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
												{
													if (g_ua_devices[i].inputs[n].fader_group == hover_channel->fader_group)
													{
														if (g_ua_devices[i].inputs[n].solo != (bool)state)
														{
															g_ua_devices[i].inputs[n].PressSolo(state);
															SetRedrawWindow(true);
														}
													}
												}
											}
										}
									}
								}
								else
								{
									if (hover_channel->solo != channel->solo)
									{
										hover_channel->PressSolo(channel->solo);
										SetRedrawWindow(true);
									}
								}
							}
						}
					}
				}
			}
			else if (channel->touch_point.action == TOUCH_ACTION_GROUP)
			{
				ChannelIndex hover_channelIndex = GetChannelIdByPosition(pos_pt);
				if (hover_channelIndex.IsValid())
				{
					Channel *hover_channel = GetChannelPtr(&hover_channelIndex);
					if (hover_channel && hover_channel->touch_point.id == 0 && !hover_channel->touch_point.is_mouse)
					{
						Vector2D relative_pos_pt = pos_pt;
						relative_pos_pt.subtractX(g_channel_offset_x);
						relative_pos_pt.setX((int)relative_pos_pt.getX() % (int)g_channel_width);
						relative_pos_pt.subtractY(g_channel_offset_y);

						if (hover_channel->IsTouchOnGroup1(&relative_pos_pt)
							|| hover_channel->IsTouchOnGroup2(&relative_pos_pt))
						{
						//	if (Button_GetCheck(g_hwndBtnMix) == BST_CHECKED)
							{
								if (hover_channel->fader_group != channel->fader_group)
								{
									hover_channel->fader_group=channel->fader_group;
									SetRedrawWindow(true);
								}
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
						channel->ChangePan(pan_move);
					}
					else
					{
						for (int n = 0; n < g_sends_count; n++)
						{
							if (g_btnSends[n].checked)
							{
								channel->sends[n].ChangePan(pan_move);
								break;
							}
						}
					}
					SetRedrawWindow(true);
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
						channel->ChangePan2(pan_move);
						SetRedrawWindow(true);
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
						for (int i = 0; i < MAX_UA_DEVICES; i++)
						{
							if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
							{
								for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
								{
									if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
									{
										if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
										{
											g_ua_devices[i].inputs[n].touch_point.pt_start_x = g_ua_devices[i].inputs[n].touch_point.pt_end_x;
											g_ua_devices[i].inputs[n].touch_point.pt_start_y = g_ua_devices[i].inputs[n].touch_point.pt_end_y;

											if (g_btnMix->checked)
											{
												g_ua_devices[i].inputs[n].ChangeLevel(fader_move);
												SetRedrawWindow(true);
											}
											else
											{
												for (int v = 0; v < g_sends_count; v++)
												{
													if (g_btnSends[v].checked)
													{
														g_ua_devices[i].inputs[n].sends[v].ChangeGain(fader_move);
														SetRedrawWindow(true);
														break;
													}
												}
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
							channel->ChangeLevel(fader_move);
							SetRedrawWindow(true);
						}
						else
						{
							for (int n = 0; n < g_sends_count; n++)
							{
								if (g_btnSends[n].checked)
								{
									channel->sends[n].ChangeGain(fader_move);
									SetRedrawWindow(true);
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}

void OnTouchUp(bool is_mouse, SDL_TouchFingerEvent *touchinput)
{
	ChannelIndex channelIndex = { -1, -1 };
		
	if (touchinput)
		channelIndex = GetChannelIdByTouchpointId(false, touchinput);
	else
		channelIndex = GetChannelIdByTouchpointId(is_mouse, 0);

	if (channelIndex.IsValid())
	{
		Channel *channel = GetChannelPtr(&channelIndex);
		if (channel)
		{
			if (channel->fader_group)
			{
				for (int i = 0; i < MAX_UA_DEVICES; i++)
				{
					if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
					{
						for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
						{
							if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
							{
								if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
								{
									if (g_ua_devices[i].inputs[n].touch_point.action == TOUCH_ACTION_LEVEL)
									{
										string cmd = "subscribe /devices/" + g_ua_devices[i].inputs[n].device->ua_id + "/inputs/" 
											+ g_ua_devices[i].inputs[n].ua_id + "/FaderLevelTapered/";
										UA_TCPClientSend(cmd.c_str());
									}
									if(touchinput)
									{
										g_ua_devices[i].inputs[n].touch_point.id = 0;
										g_ua_devices[i].inputs[n].touch_point.finger_id = 0;
									}
									if(is_mouse)
										g_ua_devices[i].inputs[n].touch_point.is_mouse = false;

									g_ua_devices[i].inputs[n].touch_point.action = TOUCH_ACTION_NONE;
								}
							}
						}
					}
				}
			}
			else
			{
				if (channel->touch_point.action == TOUCH_ACTION_LEVEL)
				{
					string cmd = "subscribe /devices/" + channel->device->ua_id + "/inputs/" + channel->ua_id + "/FaderLevelTapered/";
					UA_TCPClientSend(cmd.c_str());
				}
				if(touchinput)
				{
					channel->touch_point.id = 0;
					channel->touch_point.finger_id = 0;
				}
				if(is_mouse)
					channel->touch_point.is_mouse = false;
					
				channel->touch_point.action = TOUCH_ACTION_NONE;
			}
		}
	}
}

void CreateStaticButtons()
{
	g_btnInfo = new Button(ID_BTN_INFO, "Info",0,0,g_btn_width, g_btn_height,false, true);
	g_btnSelectChannels = new Button(ID_BTN_SELECT_CHANNELS, "Select\nChannels",0,0,g_btn_width, g_btn_height, false, false);
	g_btnChannelWidth = new Button(ID_BTN_CHANNELWIDTH, "View:\nRegular",0,0,g_btn_width, g_btn_height, false, false);
	g_btnPageLeft = new Button(ID_BTN_PAGELEFT, "Page Left",0,0,g_btn_width, g_btn_height,false, false);
	g_btnPageRight = new Button(ID_BTN_PAGELEFT, "Page Right",0,0,g_btn_width, g_btn_height,false, false);
	g_btnMix = new Button(ID_BTN_MIX, "MIX", 0, 0, g_btn_width, g_btn_height, true,false);
}

void CleanUpStaticButtons()
{
	if(g_btnSelectChannels)
	{
		delete g_btnSelectChannels;
		g_btnSelectChannels=NULL;
	}
	if(g_btnChannelWidth)
	{
		delete g_btnChannelWidth;
		g_btnChannelWidth=NULL;
	}
	if(g_btnPageLeft)
	{
		delete g_btnPageLeft;
		g_btnPageLeft=NULL;
	}
	if(g_btnPageRight)
	{
		delete g_btnPageRight;
		g_btnPageRight=NULL;
	}
	if(g_btnInfo)
	{
		delete g_btnInfo;
		g_btnInfo=NULL;
	}
	if(g_btnMix)
	{
		delete g_btnMix;
		g_btnMix=NULL;
	}
}

void CreateSendButtons(int sz)
{
	if (sz > 0)
	{
		float SPACE_X = 2.0f;
		float SPACE_Y = 2.0f;

		CleanUpSendButtons();

		g_btnSends = new Button[sz];
		g_sends_count = sz;
		for (int a = 0; a < g_sends_count; a++)
			g_btnSends[a].id = ID_BTN_SENDS + a;
		
		UpdateLayout();
		UpdateSubscriptions();
		SetRedrawWindow(true);
	}
}

void UpdateChannelWidthButton() {
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

void UpdateConnectButtons()
{
	int w,h;
	SDL_GetWindowSize(g_window,&w,&h);

	string connected_btn_text = GetCheckedConnectionButton();

	CleanUpConnectionButtons();

	int sz = 0;
	while (g_ua_server_list[sz].length())
	{
		sz++;

		if (sz >= UA_MAX_SERVER_LIST)
			break;
	}

	g_btnConnect = new Button[sz];

	float SPACE_X = 2.0f;
	float SPACE_Y = 2.0f;

	while (g_connection_btns < sz && g_ua_server_list[g_connection_btns].length())
	{
		string btn_text = "Connect to\n" + g_ua_server_list[g_connection_btns];
		g_btnConnect[g_connection_btns].id = ID_BTN_CONNECT + g_connection_btns;
		g_btnConnect[g_connection_btns].rc.x = w - g_channel_offset_x + round(SPACE_X + g_btn_width * 0.13f);
		g_btnConnect[g_connection_btns].rc.y = g_channel_offset_y + g_connection_btns * (g_btn_height + SPACE_Y);
		g_btnConnect[g_connection_btns].rc.w = g_btn_width;
		g_btnConnect[g_connection_btns].rc.h = g_btn_height;
		g_btnConnect[g_connection_btns].text = btn_text;

		if (btn_text == connected_btn_text)
		{
			g_btnConnect[g_connection_btns].checked = true;
		}

		g_connection_btns++;

		if (g_connection_btns >= UA_MAX_SERVER_LIST)
			break;
	}

	SetRedrawWindow(true);

	//wenn keine verbindung, verbinde automatisch mit dem ersten server
	if (g_connection_btns == 1 && connected_btn_text.length() == 0)
	{
		SDL_Event event;
		memset(&event, 0, sizeof(SDL_Event));
		event.type = SDL_USEREVENT;
		event.user.code = EVENT_CONNECT;
		SDL_PushEvent(&event);
	}
}

GFXSurface* LoadImage(string filename) {
	GFXSurface *gs = gfx->LoadGfx(getLocalPath("data/" + filename));
	if (!gs)
		return NULL;

	// usually GFXEngine keeps a copy of the image in RAM but here it's not needed, so delete it manually
	delete[] gs->bgra;
	gs->bgra = NULL;

	return gs;
}

bool LoadImages() {

	if(!(g_gsButtonYellow[0] = LoadImage("btn_yellow_off.gfx")))
		return false;
	if (!(g_gsButtonYellow[1] = LoadImage("btn_yellow_on.gfx")))
		return false;

	if (!(g_gsButtonRed[0] = LoadImage("btn_red_off.gfx")))
		return false;
	if (!(g_gsButtonRed[1] = LoadImage("btn_red_on.gfx")))
		return false;

	if (!(g_gsButtonGreen[0] = LoadImage("btn_green_off.gfx")))
		return false;
	if (!(g_gsButtonGreen[1] = LoadImage("btn_green_on.gfx")))
		return false;

	if (!(g_gsButtonBlue[0] = LoadImage("btn_blue_off.gfx")))
		return false;
	if (!(g_gsButtonBlue[1] = LoadImage("btn_blue_on.gfx")))
		return false;

	if (!(g_gsChannelBg = LoadImage("channelbg.gfx")))
		return false;

	if (!(g_gsBgRight = LoadImage("bg_right.gfx")))
		return false;

	if (!(g_gsFaderrail = LoadImage("faderrail.gfx")))
		return false;

	if (!(g_gsFader = LoadImage("fader.gfx")))
		return false;

	if (!(g_gsPan = LoadImage("poti.gfx")))
		return false;

	if (!(g_gsPanPointer = LoadImage("poti_pointer.gfx")))
		return false;

	for (int n = 0; n < 4; n++) {
		if (!(g_gsLabel[n] = LoadImage("label" + to_string(n + 1) + ".gfx")))
			return false;
	}

	return true;
}

void DeleteGfx()
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

string GetFileName(string postfix)
{
/*	char user[UNLEN + 1];
	DWORD sz = UNLEN + 1;
	GetUserName(user, &sz);

	char computer[MAX_COMPUTERNAME_LENGTH + 1];
	sz = MAX_COMPUTERNAME_LENGTH + 1;
	GetComputerName(computer, &sz);

	sprintf(filename, "%s@%s@%s.json", user, computer, postfix);*/

	return postfix + ".json";
}

bool LoadServerSettings(string server_name, Button *btnSend)
{
	if (server_name.length() == 0 || !btnSend)
		return false;

	bool result = true;
	string filename = GetFileName(server_name);

	//JSON
	try
	{
		dom::parser parser;
		dom::element element = parser.load(getLocalPath("data/" + filename));

		string_view selected_send = element["ua_server"]["selected_mix"];

		if (!g_settings.lock_to_mix.length() && btnSend->text == selected_send)
		{
			for (int i = 0; i < g_sends_count; i++)
			{
				if (g_btnSends[i].id == btnSend->id)
					continue;
				g_btnSends[i].checked = false;
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
			for (int i = 0; i < g_sends_count; i++) {
				g_btnSends[i].enabled = false;
				g_btnSends[i].checked = false;
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

bool LoadServerSettings(string server_name, UADevice *ua_dev) // l�d channel-select & group-setting f�r das jeweilige device
{
	if (server_name.length() == 0 || !ua_dev)
		return false;

	string filename = GetFileName(server_name);

	//JSON
	try
	{
		dom::parser parser;
		dom::element element = parser.load(getLocalPath("data/" + filename));

		for (int n = 0; n < ua_dev->inputsCount; n++)
		{
			if (ua_dev->ua_id.length() == 0 || !ua_dev->inputs)
				break;

			try
			{
				ua_dev->inputs[n].selected_to_show = element["ua_server"][ua_dev->ua_id][ua_dev->inputs[n].ua_id + " selected"];
				//override als selected, wenn name mit ! beginnt
				if (ua_dev->inputs[n].IsOverriddenShow())
					ua_dev->inputs[n].selected_to_show = true;
				//override als nicht selected, wenn name mit # endet
				else if (ua_dev->inputs[n].IsOverriddenHide())
					ua_dev->inputs[n].selected_to_show = false;
			}
			catch (simdjson_error e) {}
			try
			{
				int64_t value = element["ua_server"][ua_dev->ua_id][ua_dev->inputs[n].ua_id + " group"];
				ua_dev->inputs[n].fader_group = (int)value;
			}
			catch (simdjson_error e) {}
		}
	}
	catch (simdjson_error e)
	{
		return false;
	}
	return true;
}

bool SaveServerSettings(string server_name)
{
	if (server_name.length() == 0)
		return false;

	string filename = GetFileName(server_name);
	string path = getLocalPath("data/" + filename);

	FILE* fh = NULL;
	if (fopen_s(&fh, path.c_str(), "w") == 0)
	{
		fprintf(fh, "{\n");

		if (g_ua_server_connected.length())
		{
			fprintf(fh, "\"ua_server\": {\n");
			fprintf(fh, "\"name\": \"%s\",\n", g_ua_server_connected.c_str());

			fprintf(fh, "\"selected_mix\": ");
			
			if (g_btnMix->checked)
			{
				fprintf(fh, "\"MIX\",\n");
			}
			else
			{
				for (int n = 0; n < g_sends_count; n++)
				{
					if (g_btnSends[n].checked)
					{
						fprintf(fh, "\"%s\",\n", g_btnSends[n].text.c_str());
						break;
					}
				}
			}

			bool set_komma = false;
			for (int n = 0; n < MAX_UA_DEVICES; n++)
			{
				if (g_ua_devices[n].ua_id.length() == 0)
					continue;

				if(set_komma)
					fprintf(fh, ",\n");

				fprintf(fh, "\"%s\": {\n", g_ua_devices[n].ua_id.c_str());
				if (g_ua_devices[n].inputs)
				{
					bool set_komma2 = false;
					for (int i = 0; i < g_ua_devices[n].inputsCount; i++)
					{
						if (set_komma2)
							fprintf(fh, ",\n");

						fprintf(fh, "\"%s group\": %d\n,", g_ua_devices[n].inputs[i].ua_id.c_str(), g_ua_devices[n].inputs[i].fader_group);

						fprintf(fh, "\"%s selected\": ", g_ua_devices[n].inputs[i].ua_id.c_str());
						if (g_ua_devices[n].inputs[i].selected_to_show)
							fprintf(fh, "true");
						else
							fprintf(fh, "false");
						set_komma2 = true;
					}
				}
				fprintf(fh, "}");
				set_komma = true;
			}
			fprintf(fh, "}\n");
		}

		fprintf(fh, "}");
		fclose(fh);
	}
	else
		return false;

	return true;
}

void ConvertToDpiAware(float *fx, float *fy, bool round_values)
{
	float dpiX, dpiY;
	SDL_GetDisplayDPI(0, NULL , &dpiX, &dpiY);

	if (fx)
	{
		*fx = *fx * dpiX / 96;
		if (round_values)
			*fx = round(*fx);
	}
	if (fy)
	{
		*fy = *fy * dpiY / 96;
		if (round_values)
			*fy = round(*fy);
	}
}

bool Settings::load() {
	string filename = GetFileName("settings");

	//JSON
	try {
		dom::parser parser;
		dom::element element = parser.load(getLocalPath("data/" + filename));

		try {
			this->lock_settings = element["general"]["lock_settings"];
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

bool Settings::save()
{
	if (this->lock_settings)
		return true;

	string filename = GetFileName("settings");
	string path = getLocalPath("data/" + filename);

	FILE* fh = NULL;
	if (fopen_s(&fh, path.c_str(), "w") == 0)
	{
		fprintf(fh, "{\n");

		//STARTUP
		fprintf(fh, "\"general\": {\n");
		fprintf(fh, "\"lock_settings\": false,\n");
		fprintf(fh, "\"lock_to_mix\": \"%s\",\n", this->lock_to_mix.c_str());
		fprintf(fh, "\"extended_logging\": %s\n", this->extended_logging ? "true" : "false");
		fprintf(fh, "},\n");

		//WINDOW
		int x, y, w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		SDL_GetWindowPosition(g_window, &x, &y);

		Uint32 flags = SDL_GetWindowFlags(g_window);

		fprintf(fh, "\"window\": {\n");
		if(flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
			fprintf(fh, "\"fullscreen\": true,\n");
		else
			fprintf(fh, "\"fullscreen\": false,\n");
		if(flags & SDL_WINDOW_MAXIMIZED)
			fprintf(fh, "\"maximized\": true,\n");
		else
			fprintf(fh, "\"maximized\": false,\n");
		fprintf(fh, "\"x\": %d,\n", x);
		fprintf(fh, "\"y\": %d,\n", y);
		fprintf(fh, "\"width\": %d,\n", w);
		fprintf(fh, "\"height\": %d\n", h);
		fprintf(fh, "},\n");

		//VIEW
		fprintf(fh, "\"view\": {\n");
		fprintf(fh, "\"channel_width\": %d\n", this->channel_width);
		fprintf(fh, "},\n");

		//SERVERLIST
		fprintf(fh, "\"serverlist\": {\n");
		for (int n = 0; n < UA_MAX_SERVER_LIST; n++)
		{
			fprintf(fh, "\"%d\": \"%s\"", n + 1, this->serverlist[n].c_str());
			if(n == UA_MAX_SERVER_LIST - 1)
				fputs("\n", fh);
			else
				fputs(",\n", fh);
		}
		fprintf(fh, "}\n");

		fprintf(fh, "}");
		fclose(fh);
	}
	else
		return false;

	return true;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, char *lpCmdLine, int nCmdShow) {
	int argc = __argc;
	char** argv = __argv;
#else
int main(int argc, char* argv[]) {
#endif

	string path = string(SDL_GetBasePath());
	size_t l = path.find_last_of("\\/", path.length() - 2);
	path = path.substr(0, l + 1);
	initLocalPath(path);

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

	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);

#ifdef _WIN32
	SetProcessDPIAware();
#endif

	CreateStaticButtons();
	
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

	if (!g_settings.extended_logging) toLog("Extended logging is deactivated.\nYou can activate it in settings.json but keep in mind that it will slow down performance.");
	
	SDL_Delay(startup_delay);

	UpdateChannelWidthButton();

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
	catch (invalid_argument e)
	{
		toLog("InitGfx failed : " + string(SDL_GetError()));
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","InitGfx failed", NULL);
		return 1;
	}
	SDL_SetWindowMinimumSize(g_window, 320, 240);
	if (LoadImages())
	{
		UpdateLayout();

#ifdef _WIN32
		InitNetwork();
#endif
		g_ua_server_list_index = 0;
		for (int n = 0; n < UA_MAX_SERVER_LIST; n++)
		{
			if (g_settings.serverlist[n].length() > 0)
			{
				g_ua_server_list[g_ua_server_list_index] = g_settings.serverlist[n];
				g_ua_server_list_index++;
			}
		}
		if (g_ua_server_list_index) // serverliste wurde definiert, automatische Serversuche wird nicht ausgeführt
		{
			g_serverlist_defined = true;
			UpdateConnectButtons();
		}
		else // Serverliste wurde nicht definiert, automatische Suche wird ausgeführt
		{
			g_serverlist_defined = false;
			UA_GetServerList();
		}
		bool running=true;
		SDL_Event e;
		while(running)
		{
			if (SDL_WaitEventTimeout(&e, 33) > 0)
		//	while (SDL_PollEvent(&e) > 0)
			{
				switch (e.type)
				{
				case SDL_FINGERDOWN:
				{
					OnTouchDown(NULL, &e.tfinger);
					break;
				}
				case SDL_FINGERUP:
				{
					OnTouchUp(NULL, &e.tfinger);
					break;
				}
				case SDL_FINGERMOTION:
				{
					//only use last fingermotion event
					SDL_Event events[20];
					events[0] = e;
					SDL_PumpEvents();
					int ev_num = SDL_PeepEvents(&events[1], 19, SDL_GETEVENT, SDL_FINGERMOTION, SDL_FINGERMOTION) + 1;

					for (int n = ev_num - 1; n >= 0; n--)
					{
						bool drop_event = false;

						for (int i = n + 1; i < ev_num; i++)
						{
							if (events[n].tfinger.fingerId == events[i].tfinger.fingerId)
							{
								drop_event = true;
								break;
							}
						}

						if (!drop_event)
						{
							OnTouchDrag(NULL, &events[n].tfinger);
						}
					}
					break;
				}
				case SDL_MOUSEBUTTONDOWN:
				{
					if (e.button.button == SDL_BUTTON_LEFT) {
						bool handled = false;

						SDL_Point pt = { e.button.x, e.button.y };

						if(g_msg.length()) {
							g_msg = "";
							SetRedrawWindow(true);
						}
						if(g_btnInfo->checked)
							g_btnInfo->checked = false;
						else {
							//Click Send Buttons
							for (int sel = 0; sel < g_sends_count; sel++) {
								if (g_btnSends[sel].IsClicked(&pt)) {

									for (int n = 0; n < g_sends_count; n++) {
										g_btnSends[n].checked = false;
									}
									g_btnMix->checked = false;
									g_btnSends[sel].checked = true;

									UpdateSubscriptions();
									SetRedrawWindow(true);
									handled = true;
								}
							}
							//click Mix Buttons
							if (g_btnMix->IsClicked(&pt)) {

								for (int n = 0; n < g_sends_count; n++) {
									g_btnSends[n].checked = false;
								}
								g_btnMix->checked = true;

								UpdateSubscriptions();
								SetRedrawWindow(true);
								handled = true;
							}
							//Click Connect Buttons
							for (int sel = 0; sel < g_connection_btns; sel++) {
								if (g_btnConnect[sel].IsClicked(&pt)) {
									if (!g_btnConnect[sel].checked) {
										Disconnect();
										Connect(sel);
									}
									else {
										Disconnect();
									}
									handled = true;
								}
							}
							//Click Select Channels
							if (g_btnSelectChannels->IsClicked(&pt)) {
								g_btnSelectChannels->checked = !g_btnSelectChannels->checked;
								if (!g_btnSelectChannels->checked) {
									UpdateLayout();
								}
								UpdateMaxPages();
								UpdateSubscriptions();
								SetRedrawWindow(true);
								handled = true;
							}
							if (g_btnChannelWidth->IsClicked(&pt)) {

								if (g_settings.channel_width == 0)
									g_settings.channel_width = 2;

								g_settings.channel_width++;
								if (g_settings.channel_width > 3)
									g_settings.channel_width = 1;
								
								UpdateLayout();
								UpdateChannelWidthButton();
								UpdateMaxPages();
								UpdateSubscriptions();
								SetRedrawWindow(true);
								handled = true;
							}
							if (g_btnPageLeft->IsClicked(&pt)) {
								if (g_page > 0) {
									g_page--;
									UpdateSubscriptions();
									SetRedrawWindow(true);
								}
								handled = true;
							}
							if (g_btnPageRight->IsClicked(&pt)) {
								if (g_page < (GetAllChannelsCount(false) - 1) / CalculateChannelsPerPage()) {
									g_page++;
									UpdateSubscriptions();
									SetRedrawWindow(true);
								}
								handled = true;
							}
							if (g_btnInfo->IsClicked(&pt)) {
								g_btnInfo->checked = !g_btnInfo->checked;
								g_msg = INFO_TEXT;
								SetRedrawWindow(true);
								handled = true;
							}

							if (!handled) {
								SDL_Rect rc;
								rc.x = g_channel_offset_x;
								rc.y = 0;
								SDL_GetWindowSize(g_window, &rc.w, &rc.h);
								rc.w -= 2 * g_channel_offset_x;
								if (g_ua_server_connected.length() == 0 && SDL_PointInRect(&pt, &rc))//offline
								{
									if (!IS_UA_SERVER_REFRESHING)
									{
										UA_GetServerList();
									}
								}
								else
								{
									if (e.button.clicks == 2)
									{
										if (!g_btnSelectChannels->checked)
										{
											Vector2D cursor_pt = { (float)e.button.x, (float)e.button.y };

											ChannelIndex channelIndex = GetChannelIdByPosition(cursor_pt);
											if (channelIndex.IsValid())
											{
												handled = true;
												Channel *channel = GetChannelPtr(&channelIndex);
												if (channel)
												{
													Vector2D relative_cursor_pt = cursor_pt;
													relative_cursor_pt.subtractX(g_channel_offset_x);
													relative_cursor_pt.setX((int)relative_cursor_pt.getX() % (int)g_channel_width);
													relative_cursor_pt.subtractY(g_channel_offset_y);

													if (channel->IsTouchOnPan(&relative_cursor_pt))
													{
														if (g_btnMix->checked)
														{
															if (channel->stereo)
																channel->ChangePan(-1, true);
															else
																channel->ChangePan(0, true);
														}
														else
														{
															for (int n = 0; n < g_sends_count; n++)
															{
																if (g_btnSends[n].checked)
																{
																	channel->sends[n].ChangePan(0, true);
																	break;
																}
															}
														}
														SetRedrawWindow(true);
													}
													else if (channel->IsTouchOnPan2(&relative_cursor_pt))
													{
														if (g_btnMix->checked)
														{
															channel->ChangePan2(1, true);
														}
														SetRedrawWindow(true);
													}
													else if (channel->IsTouchOnFader(&relative_cursor_pt)) //level
													{
														if (g_btnMix->checked)
														{
															channel->ChangeLevel(UA_UNITY, true);
														}
														else
														{
															for (int n = 0; n < g_sends_count; n++)
															{
																if (g_btnSends[n].checked)
																{
																	channel->sends[n].ChangeGain(UA_UNITY, true);
																	break;
																}
															}
														}
														SetRedrawWindow(true);
													}
												}
											}
										}
										if (!handled)
										{
											SDL_Event event;
											event.type = SDL_KEYDOWN;
											event.key.keysym.sym = SDLK_f;
											SDL_PushEvent(&event);
										}
									}
									if (e.button.which != SDL_TOUCH_MOUSEID)
									{
										Vector2D pt = Vector2D(e.button.x, e.button.y);
										OnTouchDown(&pt, NULL);
									}
								}
							}
						}
					}
					break;
				}
				case SDL_MOUSEMOTION:
				{
					SDL_Event *p_event = &e;

					//only use last mousemotion event
					SDL_Event events[16];
					SDL_PumpEvents();
					int ev_num = SDL_PeepEvents(events, 16, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION);

					if (ev_num)
					{
						p_event = &events[ev_num - 1];
					}
					
					if (p_event->motion.which != SDL_TOUCH_MOUSEID)
					{
						if (p_event->motion.state & SDL_BUTTON_LMASK)
						{
							Vector2D pt = Vector2D(p_event->button.x, p_event->button.y);
							OnTouchDrag(&pt, NULL);
						}
					}
					break;
				}
				case SDL_MOUSEBUTTONUP:
				{
					if (e.button.which != SDL_TOUCH_MOUSEID)
					{
						if (e.button.button == SDL_BUTTON_LEFT)
						{
							OnTouchUp(true, NULL);
						}
					}
					break;
				}
				case SDL_MOUSEWHEEL:
				{
					if(g_msg.length()) {
						g_msg = "";
						g_btnInfo->checked = false;
						SetRedrawWindow(true);
					}

					int x, y;
					SDL_GetMouseState(&x, &y);
					Vector2D cursor_pt = { (float)x,(float)y };

					ChannelIndex channelIndex = GetChannelIdByPosition(cursor_pt);
					if (channelIndex.IsValid())
					{
						Channel *channel = GetChannelPtr(&channelIndex);
						if (channel)
						{
							Vector2D relative_cursor_pt = cursor_pt;
							relative_cursor_pt.subtractX(g_channel_offset_x);
							relative_cursor_pt.setX((int)relative_cursor_pt.getX() % (int)g_channel_width);
							relative_cursor_pt.subtractY(g_channel_offset_y);

							double move = (float)e.wheel.y / 50;

							if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
							{
								move = -move;
							}

							if (channel->IsTouchOnPan(&relative_cursor_pt))
							{
								if (g_btnMix->checked)
								{
									channel->ChangePan(move * 2);
								}
								else
								{
									for (int n = 0; n < g_sends_count; n++)
									{
										if (g_btnSends[n].checked)
										{
											channel->sends[n].ChangePan(move);
											break;
										}
									}
								}
								SetRedrawWindow(true);
							}
							else if (channel->IsTouchOnPan2(&relative_cursor_pt))
							{
								if (g_btnMix->checked)
								{
									channel->ChangePan2(move * 2);
									SetRedrawWindow(true);
								}
							}
							else if (channel->IsTouchOnFader(&relative_cursor_pt))
							{
								if (channel->fader_group)
								{
									for (int i = 0; i < MAX_UA_DEVICES; i++)
									{
										if (g_ua_devices[i].ua_id.length() && g_ua_devices[i].inputs)
										{
											for (int n = 0; n < g_ua_devices[i].inputsCount; n++)
											{
												if (g_ua_devices[i].inputs[n].ua_id.length() && !g_ua_devices[i].inputs[n].hidden && g_ua_devices[i].inputs[n].selected_to_show)
												{
													if (g_ua_devices[i].inputs[n].fader_group == channel->fader_group)
													{
														if (g_btnMix->checked)
														{
															g_ua_devices[i].inputs[n].ChangeLevel(move);
															SetRedrawWindow(true);
														}
														else
														{
															for (int v = 0; v < g_sends_count; v++)
															{
																if (g_btnSends[v].checked)
																{
																	g_ua_devices[i].inputs[n].sends[v].ChangeGain(move);
																	SetRedrawWindow(true);
																	break;
																}
															}
														}
													}
												}
											}
										}
									}
								}
								else
								{
									if (g_btnMix->checked)
									{
										channel->ChangeLevel(move);
										SetRedrawWindow(true);
									}
									else
									{
										for (int n = 0; n < g_sends_count; n++)
										{
											if (g_btnSends[n].checked)
											{
												channel->sends[n].ChangeGain(move);
												SetRedrawWindow(true);
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
				case SDL_KEYDOWN:
				{
					switch (e.key.keysym.sym)
					{
					case SDLK_ESCAPE:
					{
						SDL_SetWindowFullscreen(g_window, 0);
						break;
					}
					case SDLK_f:
					{
						int x, y;
						SDL_GetGlobalMouseState(&x, &y);
						if (SDL_GetWindowFlags(g_window) & SDL_WINDOW_FULLSCREEN_DESKTOP)
						{
							SDL_SetWindowFullscreen(g_window, 0);

							int x, y, w, h;
							SDL_GetWindowPosition(g_window, &x, &y);
							SDL_GetWindowSize(g_window, &w, &h);

							int t, l, b, r;
							SDL_GetWindowBordersSize(g_window, &t, &l, &b, &r);

							SDL_Rect rc;
							if (SDL_GetDisplayUsableBounds(0, &rc) == 0)
							{
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
						}
						else
							SDL_SetWindowFullscreen(g_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
						SDL_WarpMouseGlobal(x, y);
						break;
					}
					}
					break;
				}
				case SDL_WINDOWEVENT:
				{
					switch (e.window.event)
					{
					case SDL_WINDOWEVENT_EXPOSED:
					{
						SetRedrawWindow(true);
						break;
					}
					case SDL_WINDOWEVENT_SIZE_CHANGED:
					case SDL_WINDOWEVENT_RESIZED:
					case SDL_WINDOWEVENT_MAXIMIZED:
					case SDL_WINDOWEVENT_RESTORED:
					{
						gfx->Resize();
						UpdateLayout();
						UpdateMaxPages();
						UpdateSubscriptions();
						SetRedrawWindow(true);
						break;
					}
					}
					break;
				}
				case SDL_QUIT:
				case SDL_APP_TERMINATING:
				{
					running = false;
					break;
				}
				case SDL_USEREVENT:
				{
					if (e.user.code == EVENT_CONNECT) {
						Disconnect();
						Connect(0);
					}
					else if(e.user.code == EVENT_DISCONNECT) {
						Disconnect();
					}
					break;
				}
				default:
					continue;
				}
			}

			if(GetRedrawWindow()) {
				SetRedrawWindow(false);
				Draw();
			}
		}

		g_settings.save();

		Disconnect();

#ifdef _WIN32
		ReleaseNetwork();
#endif

		CleanUpConnectionButtons();

		CleanUpStaticButtons();
	}
	else
	{
		toLog("LoadImages failed");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR ,"Error","LoadImages failed", NULL);
	}

	gfx->DeleteFont(g_fntMain);
	gfx->DeleteFont(g_fntChannelBtn);
	gfx->DeleteFont(g_fntLabel);
	gfx->DeleteFont(g_fntFaderScale);
	gfx->DeleteFont(g_fntOffline);
	DeleteGfx();
	
	delete gfx;
	gfx = NULL;

	return 0;
}