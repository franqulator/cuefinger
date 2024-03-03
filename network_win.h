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

#pragma once
#include <ws2tcpip.h>
#include <windows.h>
#include <exception>
#include <string>

#pragma comment(lib,"Ws2_32.lib")

using namespace std;

#define MAX_CLIENTS	16
#define TCP_BUFFER_SIZE 512

#define MSG_CLIENT_CONNECTED		1
#define MSG_CLIENT_DISCONNECTED		2
#define MSG_CLIENT_CONNECTION_LOST	3
#define MSG_TEXT					4

bool InitNetwork();
void ReleaseNetwork();
bool GetClientIPs(void(*MessageCallback)(char *_ip));
void GetComputerNameByIP(const char *ip, char *computername);

class TCPClient
{
private:
	SOCKET socketConnect;
	HANDLE receiveThreadHandle;
	bool receiveThreadIsRunning;
public:
	TCPClient(const char *ip_or_hostname, const char *port, void (__cdecl *MessageCallback)(int,string));
	~TCPClient();
	void Send(const char *txt);
	char* GetServerIP(char *ip);
	void Disconnect();
private:
	static DWORD WINAPI receiveThread(void *param);
	void(*MessageCallback)(int msg, string text);
};