#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>

int main() {

	WSADATA wsa;
	// MAKEWORD(2, 2) - the version you're requesting
	// wsa - an out-parameter: you hand the library an empty form, it fills it in
	// != 0 check - returns 0 on success
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		std::cout << "WSAStartup failed" << std::endl;
		return 1;
	}

	// buying a phone, device exists
	// but it has no number, isn't plugged in, can't call anyone, it just is
	// AF_INET - which addressing scheme do we speak? (IPv4, IPv6)
	// SOCK_STREAM - what conversation style? (TCP/UDP)
	// 0 - default
	SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == INVALID_SOCKET) {
		std::cout << "socket failed: " << WSAGetLastError() << std::endl;
		WSACleanup(); return 1;
	}
	std::cout << "socket created" << std::endl;

	// {} - value-init - ALL fields zeroed
	// server - describes itself
	// client - describes the server	
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(6379);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	// dialing the number
	// the call that turns your phone into live line
	if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		std::cout << "connect failed: " << WSAGetLastError() << std::endl;
		closesocket(sock);
		WSACleanup(); return 1;
	}		

	std::string acc;
	char buf[512];
	std::string line;

	while (std::getline(std::cin, line)) {

		std::string out = line + "\n";
		send(sock, out.data(), (int)out.size(), 0);

		bool hangup = false;
		while (true) {
			int n = recv(sock, buf, sizeof(buf), 0);
			if (n <= 0) {
				hangup = true; break;
			}
			acc.append(buf, n);
			size_t pos = acc.find('\n');
			if (pos == std::string::npos) continue;
			std::string reply = acc.substr(0, pos);
			acc.erase(0, pos + 1);
			std::cout << reply << std::endl;
			if (reply == "bye") {
				hangup = true; break;
			}
			break;
		}
		if (hangup) break;

	}

	closesocket(sock); WSACleanup();

	return 0;

}