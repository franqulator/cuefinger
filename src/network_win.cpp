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

bool InitNetwork() {
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) == SOCKET_ERROR) {
		return false;
	}
	return true;
}

void ReleaseNetwork() {
	WSACleanup();
}

string GetComputerNameByIP(const string &ip) {

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

bool GetClientIPs(void(*MessageCallback)(string ip)) {

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
			MessageCallback(string(ip));
		}
	}

	return result;
}

TCPClient::TCPClient(const string &host, const string &port, void (__cdecl *MessageCallback)(int,const string&), int timeout) {

	if (host.empty()) {
		throw invalid_argument("Host string is empty");
	}
	if (port.empty()) {
		throw invalid_argument("Port string is empty");
	}

	this->socketConnect = 0;
	this->receiveThreadHandle = NULL;
	this->receiveThreadIsRunning = false;
	this->MessageCallback = MessageCallback;

	struct addrinfo hints, *result;
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

	this->socketConnect = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (this->socketConnect == -1) {
		freeaddrinfo(result);
		throw invalid_argument("Error on creating socket");
	}

	u_long mode = 1;  // set non-blocking socket
	if (ioctlsocket(this->socketConnect, FIONBIO, &mode) != 0) {
		throw invalid_argument("Error on ioctlsocket");
	}

	if (connect(this->socketConnect, result->ai_addr, (int)result->ai_addrlen) == -1) {
		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			closesocket(this->socketConnect);
			this->socketConnect = 0;
			freeaddrinfo(result);
			throw invalid_argument("Error on connecting to " + host + ":" + port);
		}
	}
	freeaddrinfo(result);

	struct pollfd fds[1];
	fds[0].fd = this->socketConnect;
	fds[0].events = POLLOUT;
	int res = WSAPoll(fds, 1, timeout);
	if (res < 1) {
		shutdown(this->socketConnect, SD_BOTH);
		closesocket(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on poll (" + to_string(res) + ") " + host + ":" + port);
	}

	mode = 0;  // set blocking socket
	if (ioctlsocket(this->socketConnect, FIONBIO, &mode) != 0) {
		shutdown(this->socketConnect, SD_BOTH);
		closesocket(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on ioctlsocket");
	}
/*
	bool opt_val;
	socklen_t len = sizeof(opt_val);
	if(getsockopt(this->socketConnect, SOL_SOCKET, SO_ACCEPTCONN, (char*)&opt_val, &len) != 0) {
		closesocket(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on getsockopt on " + host + ":" + port);
	}
	if (!opt_val) {
		closesocket(this->socketConnect);
		this->socketConnect = 0;
		throw invalid_argument("Error on getsockopt on " + host + ":" + port);
	}*/

	if (this->MessageCallback) {
		this->receiveThreadHandle = CreateThread(NULL, 0, this->receiveThread, (void*)this, NULL, NULL);
		if (this->receiveThreadHandle) {
			this->MessageCallback(MSG_CLIENT_CONNECTED, "");
		}
		else {
			shutdown(this->socketConnect, SD_BOTH);
			closesocket(this->socketConnect);
			this->socketConnect = 0;
			throw invalid_argument("Error on creating thread");
		}
	}
}

TCPClient::~TCPClient() {
	this->MessageCallback = NULL;
	if(this->socketConnect) {
		shutdown(this->socketConnect,SD_BOTH);
		closesocket(this->socketConnect);
		this->socketConnect=NULL;
		if (this->MessageCallback) {
			this->MessageCallback(MSG_CLIENT_DISCONNECTED, "");
		}
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

DWORD WINAPI TCPClient::receiveThread(void *param)
{
	TCPClient *tcpClient=(TCPClient*)param;
	tcpClient->receiveThreadIsRunning = true;

	char buffer[TCP_BUFFER_SIZE];
	string completeMessage = "";

	while(tcpClient->socketConnect) {
		int bytes = recv(tcpClient->socketConnect, buffer, TCP_BUFFER_SIZE, 0);

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
		}
	}
	
	tcpClient->receiveThreadIsRunning=false;

	return 0;
}

int TCPClient::Receive(string& msg, int timeout) {
	char buffer[TCP_BUFFER_SIZE];
	msg = "";

	u_long mode = 1;  // set non-blocking socket
	if (ioctlsocket(this->socketConnect, FIONBIO, &mode) != 0) {
		return -1;
	}

	int result = 0;
	while (this->socketConnect && !result) {

		struct pollfd fds[1];
		fds[0].fd = this->socketConnect;
		fds[0].events = POLLIN;
		if (WSAPoll(fds, 1, timeout) < 1) {
			result = -1;
			break;
		}

		int bytes = recv(this->socketConnect, buffer, TCP_BUFFER_SIZE, 0);

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

	mode = 0;  // set blocking socket
	if (ioctlsocket(this->socketConnect, FIONBIO, &mode) != 0) {
		return -1;
	}
	return result;
}

bool TCPClient::Send(const string &data) {
	if(this->socketConnect) {
		size_t p = 0;
		while(p < data.length() + 1) {
			int len = TCP_BUFFER_SIZE;
			if(len > (int)(data.length() + 1 - p)) {
				len = (int)(data.length() + 1 - p);
			}
			int lenSent = send(this->socketConnect, data.substr(p).c_str() ,len,0);
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
