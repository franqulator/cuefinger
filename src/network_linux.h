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
#ifndef _NETWORK_
#define _NETWORK_

#include <SDL2/SDL.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>

using namespace std;

#define MAX_CLIENTS	16
#define TCP_BUFFER_SIZE 512

#define MSG_CLIENT_CONNECTED		1
#define MSG_CLIENT_DISCONNECTED		2
#define MSG_CLIENT_CONNECTION_LOST	3
#define MSG_TEXT					4

bool GetClientIPs(void(*MessageCallback)(char *));
void GetComputerNameByIP(char *ip, char *computername);

static int receiveThread(void *param);

class TCPClient
{
public:
	int socketConnect;
	SDL_Thread *receiveThreadHandle;
	bool receiveThreadIsRunning;
	TCPClient(const char *ip_or_hostname, const char *port, void (*MessageCallback)(int,string));
	~TCPClient();
	void Send(const char *txt);
	char* GetServerIP(char *ip);
	void (*MessageCallback)(int,string);
	void Disconnect();
};

#endif