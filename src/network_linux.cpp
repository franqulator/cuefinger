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

string TCPClient::getComputerNameByIP(const string &ip) {

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

bool TCPClient::getClientIPs(vector<string>& ips)
{
	struct ifaddrs * ifAddrStruct=NULL;
    struct ifaddrs * ifa=NULL;
    void * tmpAddrPtr=NULL;
	bool result = true;

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
					ips.push_back(string(addressBuffer));
				}
				else {
					result = false;
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

		return result;
	}
	return false;
}


bool TCPClient::lookUpServers(const string& ipMask, int start, int end, const string& port, int timeout, vector<string>& servers) {

	int range = end - start;

	if (range < 1) {
		return false;
	}

	struct pollfd* fds = new struct pollfd[range];
	if (!fds) {
		return false;
	}

	memset(fds, 0, sizeof(struct pollfd) * range);

	for (int n = 0; n < range; n++) {
		string ip = ipMask + to_string(n + start);
		try {
			int sock = connectNonBlock(ip, port);
			if (sock) {
				fds[n].fd = sock;
				fds[n].events = POLLOUT;
			}
			else {
				fds[n].fd = -1;
			}
		}
		catch (invalid_argument&) {
		}
	}

	unsigned long long time = GetTickCount64();
	int n = 0;
	while (n < 254 && time + timeout > GetTickCount64()) {
		int res = poll(fds, range, timeout / 10);
		if (res < 1) {
			break;
		}
		else {
			for (int i = 0; i < range; i++) {
				if (fds[i].revents == POLLOUT) {
					close(fds[i].fd);
					fds[i].fd = -1;
					servers.push_back(ipMask + to_string(i + start));
					n++;
				}
				else if (fds[i].revents == POLLERR || fds[i].revents == POLLHUP || fds[i].revents == POLLNVAL) {
					close(fds[i].fd);
					fds[i].fd = -1;
					n++;
				}
			}
		}
	}
	if (fds) {
		delete[] fds;
		fds = NULL;
	}
	return true;
}

TCPClient::TCPClient() {
	this->MessageCallback = NULL;
	this->receiveThreadHandle = NULL;
	this->receiveThreadIsRunning = false;
	this->sock = 0;
}

TCPClient::TCPClient(const string &host, const string &port, void (*MessageCallback)(int,const string&), int timeout) : TCPClient() {

	this->MessageCallback = MessageCallback;

	this->sock = this->connectNonBlock(host, port); // throws exception

	struct pollfd fds[1];
	memset(fds, 0, sizeof(struct pollfd));
	fds[0].fd = this->sock;
	fds[0].events = POLLOUT;
	int res = poll(fds, 1, timeout);
	if (res < 1 || fds[0].revents != POLLOUT) {
		close(this->sock);
		this->sock = 0;
		throw invalid_argument("Error on poll (" + to_string(res) + ") " + host + ":" + port);
	}

	setBlock(this->sock, true);

	if (this->MessageCallback) {
		this->receiveThreadHandle = SDL_CreateThread(receiveThread, "ReceiveThread",(void*)this);
		if (this->receiveThreadHandle) {
			this->MessageCallback(MSG_CLIENT_CONNECTED, "");
		}
		else {
			shutdown(this->sock, SHUT_RDWR);
			close(this->sock);
			this->sock = 0;
			throw invalid_argument("Error on creating thread");
		}
	}
}

TCPClient::~TCPClient() {

	if(this->sock) {
		if (this->MessageCallback) {
			this->MessageCallback(MSG_CLIENT_DISCONNECTED, "");
		}
		this->MessageCallback = NULL;
		shutdown(this->sock, SHUT_RDWR);
		close(this->sock);
		this->sock=0;
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


bool TCPClient::setBlock(int sock, bool block) {
	// set to blocking
	int arg;
	if ((arg = fcntl(sock, F_GETFL, NULL)) < 0) {
		return false;
	}
	if (block) {
		arg &= (~O_NONBLOCK);
	}
	else {
		arg |= O_NONBLOCK;
	}
	if (fcntl(sock, F_SETFL, arg) < 0) {
		return false;
	}
	return true;
}

int TCPClient::connectNonBlock(const string& host, const string& port) {
	int sock = 0;
	struct addrinfo hints, * result;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	//resolve host name to ip if necessary
	if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0) {
		throw invalid_argument("Error on resolving hostname on " + host + ":" + port);
	}
	if (!result) {
		throw invalid_argument("Error on resolving hostname (result == NULL) on " + host + ":" + port);
	}

	sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (sock == -1) {
		freeaddrinfo(result);
		throw invalid_argument("Error on creating sock");
	}

	if (!setBlock(sock, false)) {
		freeaddrinfo(result);
		throw invalid_argument("Error on setBlock");
	}

	if (connect(sock, result->ai_addr, (int)result->ai_addrlen) == -1) {
		if (errno != EINPROGRESS) {
			freeaddrinfo(result);
			throw invalid_argument("Error on connecting to " + host + ":" + port);
		}
	}
	freeaddrinfo(result);

	/*	int opt_val;
	socklen_t len = sizeof(opt_val);
	if (getsockopt(this->sock, SOL_SOCKET, SO_ERROR, (char*)&opt_val, &len) != 0) {
		closesocket(this->sock);
		this->sock = 0;
		throw invalid_argument("Error on getsockopt on " + host + ":" + port);
	}
	if (opt_val != 0) {
		closesocket(this->sock);
		this->sock = 0;
		throw invalid_argument("Error on getsockopt on " + host + ":" + port);
	}*/

	return sock;
}

int SDLCALL TCPClient::receiveThread(void* param)
{
	TCPClient* tcpClient = (TCPClient*)param;
	tcpClient->receiveThreadIsRunning = true;

	char buffer[TCP_BUFFER_SIZE];
	string completeMessage;

	while (tcpClient->sock) {
		ssize_t bytes = read(tcpClient->sock, buffer, TCP_BUFFER_SIZE);

		if (bytes > 0) {
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
		else/* if (bytes == -1)*/ {
			if (tcpClient->MessageCallback) {
				tcpClient->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
			}
			break;
		}
	}

	tcpClient->receiveThreadIsRunning = false;

	return 0;
}

int TCPClient::receive(string &msg, int timeout) {
	char buffer[TCP_BUFFER_SIZE];
	msg = "";
	
	setBlock(this->sock, false);

	int result = 0;
	while (this->sock && !result) {

		struct pollfd fds[1];
		fds[0].fd = this->sock;
		fds[0].events = POLLIN;
		int res = poll(fds, 1, timeout);
		if (res < 1 || fds[0].revents != POLLIN) {
			result = -1;
			break;
		}

		ssize_t bytes = read(this->sock, buffer, TCP_BUFFER_SIZE);

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
					result = (int)bytes;
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
	
	setBlock(this->sock, true);

	return result;
}

bool TCPClient::send(const string &data) {
	if(this->sock) {
		size_t p=0;
		while(p < data.length() + 1)
		{
			size_t len = TCP_BUFFER_SIZE;
			if(len > data.length() + 1  - p)
			{
				len = data.length() + 1 - p;
			}
			ssize_t lenSent = write(this->sock, data.substr(p).c_str(), len);
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