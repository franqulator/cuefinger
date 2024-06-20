/*
This file is part of Cuefinger 1

Cuefinger 1 gives you the possibility to remote control Universal Audio's
Console Application via Network (TCP).
Copyright � 2024 Frank Brempel

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
#include "gfx2d_sdl.h"

string GetComputerNameByIP(const string &ip) {

	struct addrinfo *ai=NULL;

	if(getaddrinfo(ip.c_str(), NULL, NULL, &ai) == 0) {
		if (ai) {
			char computername[NI_MAXHOST];
			int result = getnameinfo(ai->ai_addr, ai->ai_addrlen, computername, NI_MAXHOST, NULL, 0, NI_NOFQDN);
			freeaddrinfo(ai);

			if(result == 0) {
				return string(computername);
			}
		}
	}
	return ip;
}

bool GetClientIPs(void(*MessageCallback)(string ip))
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
				if (inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN)) {
					MessageCallback(string(addressBuffer));
				}
				else {
					return false;
				}
			}
		/*	else if (ifa->ifa_addr->sa_family == AF_INET6)
			{
				tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
				char addressBuffer[INET6_ADDRSTRLEN];
				inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
				
				MessageCallback(addressBuffer);
			}*/
		}
		if (ifAddrStruct) {
			freeifaddrs(ifAddrStruct);
		}

		return true;
	}
	return false;
}


int SDLCALL TCPClient::receiveThread(void *param)
{
	TCPClient* tcpClient = (TCPClient*)param;

	tcpClient->receiveThreadIsRunning = true;

	char buffer[TCP_BUFFER_SIZE];
	string completeMessage = "";

	while(tcpClient->socketConnect) {
		ssize_t bytes = read(tcpClient->socketConnect, buffer, TCP_BUFFER_SIZE);

		if(bytes > 0) {
			size_t i = 0;
			while (i < (size_t)bytes) {
				size_t len = strnlen(&buffer[i], TCP_BUFFER_SIZE - i);
				completeMessage += string(&buffer[i], len);

				if (i + len >= (size_t)bytes) {
					//message nicht komplett, warte auf weitere pakete
				}
				else {
					//message komplett
					if (tcpClient->MessageCallback) {
						tcpClient->MessageCallback(MSG_TEXT, completeMessage);
					}
					completeMessage = "";
				}

				//suche nach weiteren messages im stream
				i += strnlen(&buffer[i], TCP_BUFFER_SIZE - i) + 1;
			}
		}
		else if(bytes == -1) {
			if (tcpClient->MessageCallback) {
				tcpClient->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
			}
			break;
		}
	}
	
	tcpClient->receiveThreadIsRunning=false;

	return 0;
}

TCPClient::TCPClient(const string &host, const string &port, void (*MessageCallback)(int,const string&), int timeout)
{

    if(host.empty()) {
        throw invalid_argument("Host string is empty");
    }
	if (port.empty()) {
		throw invalid_argument("Port string is empty");
	}

	this->socketConnect = 0;
	this->receiveThreadHandle = NULL;
	this->receiveThreadIsRunning = false;
	this->MessageCallback = MessageCallback;

	struct addrinfo hints, *result = NULL;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

	//resolve host name to ip if necessary
	if(getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0) {
		throw invalid_argument("Error on resolving hostname on " + host + ":" + port);
	}
	if (!result) {
		throw invalid_argument("Error on resolving hostname (result == NULL) on " + host + ":" + port);
	}

	this->socketConnect = socket(result->ai_family, result->ai_socktype | SOCK_NONBLOCK, result->ai_protocol);
	if(this->socketConnect == -1) {
		freeaddrinfo(result);
		throw invalid_argument("Error on creating socket");
	}
	if(connect(this->socketConnect,result->ai_addr,result->ai_addrlen) == -1) {
		if (errno != EINPROGRESS) {
			close(this->socketConnect);
			this->socketConnect = 0;
			freeaddrinfo(result);
			throw invalid_argument("Error on connecting to " + host + ":" + port);
		}
	}
	freeaddrinfo(result);

	struct pollfd fds[1];
	fds[0].fd = this->socketConnect;
	fds[0].events = POLLOUT;
	int res = poll(fds, 1, timeout);
	if (res < 1) {
		shutdown(this->socketConnect, SHUT_RDWR);
		close(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on poll (" + to_string(res) + ") " + host + ":" + port);
	}

	// set to blocking
	int arg;
	if ((arg = fcntl(this->socketConnect, F_GETFL, NULL)) < 0) {
		shutdown(this->socketConnect, SHUT_RDWR);
		close(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on fcntl");
	}
	arg &= (~O_NONBLOCK);
	if (fcntl(this->socketConnect, F_SETFL, arg) < 0) {
		shutdown(this->socketConnect, SHUT_RDWR);
		close(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on fcntl");
	}

/*	int opt_val;
	socklen_t len = sizeof(opt_val);
	if (getsockopt(this->socketConnect, SOL_SOCKET, SO_ERROR, &opt_val, &len) != 0) {
		close(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on getsockopt on " + host + ":" + port);
	}
	if (opt_val != 0) {
		close(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on getsockopt on " + host + ":" + port);
	}*/

	if (this->MessageCallback) {
		this->receiveThreadHandle = SDL_CreateThread(receiveThread, "ReceiveThread",(void*)this);
		if (this->receiveThreadHandle) {
			this->MessageCallback(MSG_CLIENT_CONNECTED, "");
		}
		else {
			shutdown(this->socketConnect, SHUT_RDWR);
			close(this->socketConnect);
			this->socketConnect = 0;
			throw invalid_argument("Error on creating thread");
		}
	}
}

TCPClient::~TCPClient() {
	this->MessageCallback = NULL;

	if(this->socketConnect) {
		shutdown(this->socketConnect, SHUT_RDWR);
		close(this->socketConnect);
		this->socketConnect=0;
		if (this->MessageCallback) {
			this->MessageCallback(MSG_CLIENT_DISCONNECTED, "");
		}
	}

	unsigned long long timeout = GetTickCount64();
	while (this->receiveThreadIsRunning) { // wait until thread is terminated
		Sleep(250);
		if (GetTickCount64() - timeout > 3000) { // call terminate thread after 3 sec
			SDL_DetachThread(this->receiveThreadHandle);
			this->receiveThreadIsRunning = false;
			this->receiveThreadHandle = NULL;
		}
	}
}

int TCPClient::Receive(string &msg, int timeout) {
	char buffer[TCP_BUFFER_SIZE];
	msg = "";
	
	// set to non-blocking
	int arg;
	if ((arg = fcntl(this->socketConnect, F_GETFL, NULL)) < 0) {
		return -1;
	}
	arg |= O_NONBLOCK;
	if (fcntl(this->socketConnect, F_SETFL, arg) < 0) {
		return -1;
	}

	int result = 0;
	while (this->socketConnect && !result) {

		struct pollfd fds[1];
		fds[0].fd = this->socketConnect;
		fds[0].events = POLLIN;
		if (poll(fds, 1, timeout) < 1) {
			result = -1;
			break;
		}

		ssize_t bytes = read(this->socketConnect, buffer, TCP_BUFFER_SIZE);

		if (bytes > 0) {
			size_t i = 0;
			while (i < (size_t)bytes) {
				size_t len = strnlen(&buffer[i], TCP_BUFFER_SIZE - i);
				msg += string(&buffer[i], len);

				if (i + len >= (size_t)bytes) {
					//message nicht komplett, warte auf weitere pakete
				}
				else {
					//message komplett
					result = bytes;
					break;
				}

				//suche nach weiteren messages im stream
				i += strnlen(&buffer[i], TCP_BUFFER_SIZE - i) + 1;
			}
		}
		else if (bytes == -1) {
			result = -1;
		}
	}
	// set to blocking
	if ((arg = fcntl(this->socketConnect, F_GETFL, NULL)) < 0) {
		return -1;
	}
	arg &= (~O_NONBLOCK);
	if (fcntl(this->socketConnect, F_SETFL, arg) < 0) {
		return -1;
	}
	return result;
}

bool TCPClient::Send(const string &data) {
	if(this->socketConnect) {
		size_t p=0;
		while(p < data.length() + 1)
		{
			size_t len = TCP_BUFFER_SIZE;
			if(len > data.length() + 1  - p)
			{
				len = data.length() + 1 - p;
			}
			ssize_t lenSent = write(this->socketConnect, data.substr(p).c_str(), len);
			if(lenSent == -1) {

				if (this->MessageCallback) {
					this->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
				}
				return false;
			}
			
			p += lenSent;
		}
	}
	return true;
}