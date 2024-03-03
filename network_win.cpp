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

#include "network_win.h"

#pragma warning(disable : 4996)

bool InitNetwork()
{
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) == SOCKET_ERROR)
		return false;

	return true;
}

void ReleaseNetwork()
{
	WSACleanup();
}

void GetComputerNameByIP(const char *ip, char *computername)
{
	struct sockaddr_in sa;
	inet_pton(AF_INET, ip, &(sa.sin_addr));

	struct sockaddr_in saGNI;
	char hostname[NI_MAXHOST];
	u_short port = 27015;
	saGNI.sin_family = AF_INET;
	saGNI.sin_addr.s_addr = sa.sin_addr.s_addr;
	saGNI.sin_port = htons(port);

	DWORD dwRetval = getnameinfo((struct sockaddr *) &saGNI,
		sizeof(struct sockaddr),
		hostname,
		NI_MAXHOST, NULL, 0, NI_NOFQDN);

	strcpy(computername, hostname);
}

bool GetClientIPs(void(*MessageCallback)(char *_ip))
{
	WSADATA wsadata;

	if (WSAStartup(MAKEWORD(2, 2), &wsadata) == SOCKET_ERROR)
	{
		throw "Error on loading wsock2.dll";
	}

	char hostname[256];
	if (gethostname(hostname, 256) == SOCKET_ERROR)
		return false;

	hostent* h = gethostbyname(hostname);
	if (!h)
		return false;

	int i = 0;
	char ip[256];
	SOCKADDR_IN addr;

	while (h->h_addr_list[i])
	{
		addr.sin_addr.s_addr = inet_addr(h->h_addr_list[0]);
		strcpy_s(ip, 64, inet_ntoa(*(in_addr*)(h->h_addr_list[0])));

		MessageCallback(ip);

		i++;
	}

	return true;
}

TCPClient::TCPClient(const char *_ip_or_hostname, const char *_port, void (__cdecl *MessageCallback)(int,string))
{
	this->socketConnect = 0;

	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//resolve host name to ip if necessary
	if (getaddrinfo(_ip_or_hostname, _port, &hints, &result) < 0 || !result)
	{
		char msg[256];
		sprintf(msg, "Error on resolving hostname to %s:%s", _ip_or_hostname, _port);
		throw msg;
	}

	this->socketConnect = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (this->socketConnect < 0)
	{
		throw "Error on creating socket";
	}

	if (connect(this->socketConnect, result->ai_addr, result->ai_addrlen) < 0)
	{
		closesocket(this->socketConnect);
		this->socketConnect = 0;
		char msg[256];
		sprintf(msg, "Error on connecting to %s:%s", _ip_or_hostname, _port);
		throw msg;
	}

	if (result)
	{
		freeaddrinfo(result);
	}

	this->MessageCallback=MessageCallback;

	if (this->MessageCallback)
	{
		this->receiveThreadHandle = CreateThread(NULL, 0, this->receiveThread, (void*)this, NULL, NULL);
		this->MessageCallback(MSG_CLIENT_CONNECTED, "");
	}
}

TCPClient::~TCPClient()
{
}

void TCPClient::Disconnect()
{
	if (this->receiveThreadIsRunning)
	{
		TerminateThread(this->receiveThreadHandle, -1);
		this->receiveThreadIsRunning = false;
	}

	if(this->socketConnect)
	{
		shutdown(this->socketConnect,SD_BOTH);
		closesocket(this->socketConnect);
		this->socketConnect=NULL;
		if (this->MessageCallback)
		{
			this->MessageCallback(MSG_CLIENT_DISCONNECTED, "");
		}
	}
}

DWORD WINAPI TCPClient::receiveThread(void *param)
{
	TCPClient *tcpClient=(TCPClient*)param;

	tcpClient->receiveThreadIsRunning=true;

	char buffer[TCP_BUFFER_SIZE];
	string complete_message = "";

	while(tcpClient->socketConnect)
	{
		int bytes=recv(tcpClient->socketConnect,
			buffer,TCP_BUFFER_SIZE,NULL);

		if(bytes>0)
		{
			int i = 0;
			while (i < bytes)
			{
				int msg_len = strnlen(&buffer[i], TCP_BUFFER_SIZE - i);

				complete_message += string(&buffer[i], msg_len);

				if (i + msg_len >= bytes)
				{
					//message nicht komplett, warte auf weitere pakete
				}
				else
				{
					//message komplett
					if (tcpClient->MessageCallback)
					{
						tcpClient->MessageCallback(MSG_TEXT, complete_message);
					}
					complete_message = "";
				}

				//suche nach weiteren messages im stream
				i += strnlen(&buffer[i], TCP_BUFFER_SIZE - i) + 1;
			}			
		}
		else
		{
			bool conlost=(bytes==0);
			if(bytes==SOCKET_ERROR)
			{
				int err=WSAGetLastError();
				if(err!=WSAEINTR)
				{
					conlost=true;
				}
			}
			if(conlost)
			{
				if (tcpClient->MessageCallback)
				{
					tcpClient->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
				}
				break;
			}
		}
	}
	
	tcpClient->receiveThreadIsRunning=false;

	return 0;
}

void TCPClient::Send(const char *txt)
{
	if(this->socketConnect)
	{
		size_t len=strlen(txt)+1;
		size_t p=0;
		while(p < len)
		{
			int curlen=TCP_BUFFER_SIZE;
			if(curlen > len-p)
			{
				curlen = len-p;
			}
			int len_sent = send(this->socketConnect, &txt[p] ,curlen,0);
			if(len_sent == SOCKET_ERROR)
			{
				int err = WSAGetLastError();
				if (err != WSAEINTR)
				{
					if (this->MessageCallback)
					{
						this->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
					}
					return;
				}
			}
			p+=len_sent;
		}
	}
}

char* TCPClient::GetServerIP(char *ip) //ip = pointer to buffer that receives the server's ip
{
	if(!this->socketConnect)
		return NULL;
	
	sockaddr_in addr;
	int len=sizeof(sockaddr_in);
	getpeername(this->socketConnect,(sockaddr*)&addr,&len);

	ip=inet_ntoa ( addr.sin_addr );

	return ip;
}
