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
#include "network_linux.h"

void GetComputerNameByIP(char *ip, char *computername)
{
	struct addrinfo *ai=NULL;

	if(getaddrinfo(ip, NULL, NULL, &ai) == 0)
	{	
		if(getnameinfo(ai->ai_addr, ai->ai_addrlen, computername, NI_MAXHOST, NULL, 0, NI_NOFQDN) < 0)
			strcpy(computername, ip);

		if(ai)
		{
			freeaddrinfo(ai);
		}
	}
}

bool GetClientIPs(void(*MessageCallback)(char *_ip))
{
	struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;

    if(getifaddrs(&ifAddrStruct) == 0)
	{
		for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next)
		{
			if (!ifa->ifa_addr) {
				continue;
			}
			if (ifa->ifa_addr->sa_family == AF_INET)
			{
				tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
				char addressBuffer[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);

				MessageCallback(addressBuffer);
			}
		/*	else if (ifa->ifa_addr->sa_family == AF_INET6)
			{
				tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
				char addressBuffer[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
				
				MessageCallback(addressBuffer);
			}*/
		}
		if (ifAddrStruct!=NULL)
			freeifaddrs(ifAddrStruct);

		return true;
	}
	return false;
}


static int receiveThread(void *param)
{
	TCPClient *tcpClient=(TCPClient*)param;

	tcpClient->receiveThreadIsRunning=true;

	char buffer[TCP_BUFFER_SIZE];
	string complete_message = "";

	while(tcpClient->socketConnect)
	{
		ssize_t bytes=read(tcpClient->socketConnect,
			buffer,TCP_BUFFER_SIZE);

		if(bytes>0)
		{
			size_t i = 0;
			while (i < bytes)
			{
				size_t msg_len = strnlen(&buffer[i], TCP_BUFFER_SIZE - i);

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
		else if(bytes == -1)
		{
		/*	bool conlost=(bytes==0);
			if(bytes < 0)
			{
			//	int err=WSAGetLastError();
			//	if(err!=WSAEINTR)
				{
					conlost=true;
				}
			}
			if(conlost)
			{*/
				if (tcpClient->MessageCallback)
				{
					tcpClient->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
				}
				break;
		//	}
		}
	}
	
	tcpClient->receiveThreadIsRunning=false;

	return 0;
}

TCPClient::TCPClient(const char *_ip_or_hostname, const char *_port, void (*MessageCallback)(int,string))
{
	struct addrinfo hints, *result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

	//resolve host name to ip if necessary
	if(getaddrinfo(_ip_or_hostname, _port, &hints, &result) < 0)
	{
		char msg[256];
		sprintf(msg,"Error on resolving hostname to %s:%s",_ip_or_hostname, _port);
		throw msg;
	}
	
	this->socketConnect=socket(result->ai_family,result->ai_socktype, result->ai_protocol);
	if(this->socketConnect < 0)
	{
		throw "Error on creating socket";
	}

	if(connect(this->socketConnect,result->ai_addr,result->ai_addrlen) < 0)
	{
		close(this->socketConnect);
		this->socketConnect=0;
		char msg[256];
		sprintf(msg,"Error on connecting to %s:%s",_ip_or_hostname, _port);
		throw msg;
	}

	if(result)
	{
		freeaddrinfo(result);
	}

	this->MessageCallback=MessageCallback;

	if (this->MessageCallback)
	{
		this->receiveThreadHandle = SDL_CreateThread(receiveThread, "ReceiveThread",(void*)this);
		this->MessageCallback(MSG_CLIENT_CONNECTED, "");
	}
}

TCPClient::~TCPClient()
{
}

void TCPClient::Disconnect()
{
	if(this->receiveThreadIsRunning)
	{
		SDL_DetachThread(this->receiveThreadHandle);
	}
	this->receiveThreadIsRunning=false;
	
	if(this->socketConnect)
	{
		shutdown(this->socketConnect, SHUT_RDWR);
		close(this->socketConnect);
		this->socketConnect=0;
		if (this->MessageCallback)
		{
			this->MessageCallback(MSG_CLIENT_DISCONNECTED, "");
		}
	}
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
			if(curlen>len-p)
			{
				curlen=len-p;
			}
			int len_sent=write(this->socketConnect,&txt[p],curlen);
			if(len_sent == -1)
			{
//				int err = WSAGetLastError();
//				if (err != WSAEINTR)
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