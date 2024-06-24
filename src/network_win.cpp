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

#include "network_win.h"

#pragma warning(disable : 4996)

bool TCPClient::initNetwork() {
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) == SOCKET_ERROR) {
		return false;
	}
	return true;
}

void TCPClient::releaseNetwork() {
	WSACleanup();
}

string TCPClient::getComputerNameByIP(const string &ip) {

	struct sockaddr_in sa;
	if (inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1) {
		return ip;
	}

	struct sockaddr_in saGNI;
	char hostname[NI_MAXHOST];
	u_short port = 27015;
	saGNI.sin_family = AF_INET;
	saGNI.sin_addr.s_addr = sa.sin_addr.s_addr;
	saGNI.sin_port = htons(port);

	if (getnameinfo((struct sockaddr*)&saGNI, sizeof(struct sockaddr), hostname, NI_MAXHOST, NULL, 0, NI_NOFQDN) != 0) {
		return ip;
	}

	return string(hostname);
}

bool TCPClient::getClientIPs(vector<string>& ips) {

	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) == SOCKET_ERROR) {
		return false;
	}

	char hostname[NI_MAXHOST];
	if (gethostname(hostname, NI_MAXHOST) == SOCKET_ERROR) {
		return false;
	}

	hostent* h = gethostbyname(hostname);
	if (!h) {
		return false;
	}

	bool result = true;
	for (int i = 0; h->h_addr_list[i]; i++) {
		char* ip = inet_ntoa(*(in_addr*)(h->h_addr_list[i]));
		if (!ip) {
			result = false;
		}
		else {
			ips.push_back(string(ip));
		}
	}

	return result;
}

bool TCPClient::lookUpServers(const string& ipMask, int start, int end, const string& port, int timeout, vector<string> &servers) {

	int range = end - start;

	if (range < 1) {
		return false;
	}

	struct pollfd *fds = new struct pollfd[range];
	if (!fds) {
		return false;
	}

	memset(fds, 0, sizeof(struct pollfd) * range);

	for (int n = 0; n < range; n++) {
		string ip = ipMask + to_string(n + start);
		try {
			SOCKET sock = connectNonBlock(ip, port);
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
	while (n < 254 && time + timeout > GetTickCount64() ) {
		int res = WSAPoll(fds, range, timeout / 10);
		if (res < 1) {
			break;
		}
		else {
			for (int i = 0; i < range; i++) {
				if (fds[i].revents == POLLOUT) {
					closesocket(fds[i].fd);
					fds[i].fd = -1;
					servers.push_back(ipMask + to_string(i + start));
					n++;
				}
				else if (fds[i].revents == POLLERR || fds[i].revents == POLLHUP || fds[i].revents == POLLNVAL) {
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

TCPClient::TCPClient(const string &host, const string &port, void (__cdecl *MessageCallback)(int,const string&), int timeout) : TCPClient() {

	this->MessageCallback = MessageCallback;

	this->sock = this->connectNonBlock(host, port); // throws exception

	struct pollfd fds[1];
	memset(fds, 0, sizeof(struct pollfd));
	fds[0].fd = this->sock;
	fds[0].events = POLLOUT;
	int res = WSAPoll(fds, 1, timeout);
	if (res < 1 || fds[0].revents != POLLOUT) {
		shutdown(this->sock, SD_BOTH);
		closesocket(this->sock);
		this->sock = 0;
		throw invalid_argument("Error on poll (" + to_string(res) + ") " + host + ":" + port);
	}

	if (!setBlock(this->sock, true)) {
		shutdown(this->sock, SD_BOTH);
		closesocket(this->sock);
		this->sock = 0;
		throw invalid_argument("Error on setBlock");
	}

	if (this->MessageCallback) {
		this->receiveThreadHandle = CreateThread(NULL, 0, this->receiveThread, (void*)this, NULL, NULL);
		if (this->receiveThreadHandle) {
			this->MessageCallback(MSG_CLIENT_CONNECTED, "");
		}
		else {
			shutdown(this->sock, SD_BOTH);
			closesocket(this->sock);
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
		shutdown(this->sock, SD_BOTH);
		closesocket(this->sock);
		this->sock = NULL;
	}

	unsigned long long timeout = GetTickCount64();
	while (this->receiveThreadIsRunning) { // wait until thread is terminated
		Sleep(250);
		if (GetTickCount64() - timeout > 3000) { // call terminate thread after 3 sec
			TerminateThread(this->receiveThreadHandle, -1);
			this->receiveThreadIsRunning = false;
			this->receiveThreadHandle = NULL;
		}
	}
}

bool TCPClient::setBlock(SOCKET sock, bool block) {
	u_long mode = block ? 0 : 1;  // set non-blocking mode
	if (ioctlsocket(sock, FIONBIO, &mode) != 0) {
		return false;
	}
	return true;
}

SOCKET TCPClient::connectNonBlock(const string &host, const string &port) {
	SOCKET sock = 0;
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
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
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

DWORD WINAPI TCPClient::receiveThread(void *param)
{
	TCPClient *tcpClient=(TCPClient*)param;
	tcpClient->receiveThreadIsRunning = true;

	char buffer[TCP_BUFFER_SIZE];
	string completeMessage;

	while(tcpClient->sock) {
		int bytes = recv(tcpClient->sock, buffer, TCP_BUFFER_SIZE, 0);

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
		else {
			if (tcpClient->MessageCallback) {
				tcpClient->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
			}
			break;
		}
	}

	tcpClient->receiveThreadIsRunning = false;
	
	return 0;
}

int TCPClient::receive(string& msg, int timeout) {
	char buffer[TCP_BUFFER_SIZE];
	msg = "";

	setBlock(this->sock, false);

	int result = 0;
	while (this->sock && !result) {

		struct pollfd fds[1];
		fds[0].fd = this->sock;
		fds[0].events = POLLIN;
		int res = WSAPoll(fds, 1, timeout);
		if (res < 1 || fds[0].revents != POLLIN) {
			result = -1;
			break;
		}

		int bytes = recv(this->sock, buffer, TCP_BUFFER_SIZE, 0);

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
		else {
			result = bytes;
			break;
		}
	}

	setBlock(this->sock, true);

	return result;
}

bool TCPClient::send(const string &data) {
	if(this->sock) {
		size_t p = 0;
		while(p < data.length() + 1) {
			int len = TCP_BUFFER_SIZE;
			if(len > (int)(data.length() + 1 - p)) {
				len = (int)(data.length() + 1 - p);
			}
			int lenSent = ::send(this->sock, data.substr(p).c_str() ,len,0);
			if(lenSent == SOCKET_ERROR) {
				int err = WSAGetLastError();
				if (err != WSAEINTR) {
					if (this->MessageCallback) {
						this->MessageCallback(MSG_CLIENT_CONNECTION_LOST, "");
					}
					return false;
				}
			}
			p += (size_t)lenSent;
		}
	}
	return true;
}
