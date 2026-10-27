#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>
#include <optional>
#include <condition_variable>
#include <queue>

#include "lock_guard.h"

using Clock = std::chrono::steady_clock;

struct TaskQueue {
	std::queue<SOCKET> tasks;
	std::mutex m;
	std::condition_variable cv;
	void push(SOCKET s) {
		{
			LockGuard lg(m); tasks.push(s);
		}
		cv.notify_one();
	}
	SOCKET take() {
		std::unique_lock<std::mutex> lk(m);
		cv.wait(lk, [&] { 
			return !tasks.empty();
		});
		SOCKET s = tasks.front(); tasks.pop();
		return s;
	}
};

struct Entry {
	std::string value;
	// monotonic time = only moves forward at a constant rate
	// the right clock for durations and deadlines	
	// optional - bundles the value and flag into one object
	std::optional<Clock::time_point> deadline;
};

struct Db {
	std::unordered_map<std::string, Entry> store;
	std::mutex m;
	std::condition_variable cv;
};

std::string handle(Db& db, const std::string& line) {
	LockGuard lg(db.m);
	std::istringstream iss(line);
	std::string cmd, key, value; iss >> cmd >> key >> value;
	if (cmd.empty()) return "";
	if (cmd == "SET") {
		db.store[key] = Entry{value, {}}; return "OK"; 
	}
	else if (cmd == "GET") {
		auto it = db.store.find(key);
		if (it == db.store.end()) return "(nil)";
		if (it->second.deadline && Clock::now() >= *it->second.deadline) {
			db.store.erase(it);
			return "(nil)";
		}
		return it->second.value;
	}
	else if (cmd == "DEL") {
		return "(integer) " + std::to_string(db.store.erase(key));
	}
	else if (cmd == "EXIT") {
		return "bye";
	}
	else if (cmd == "EXPIRE") {
		auto it = db.store.find(key);
		if (it == db.store.end()) return "(integer) 0";
		it->second.deadline = Clock::now() + std::chrono::seconds(std::stoi(value));
		db.cv.notify_one();
		return "(integer) 1";
	}
	else if (cmd == "TTL") {
		auto it = db.store.find(key);
		if (it == db.store.end()) return "(integer) -2";
		if (!it->second.deadline) return "(integer) -1";
		auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
			*it->second.deadline - Clock::now()).count();
		return "(integer) " + std::to_string(remaining);
	}
	else {
		return "ERR unknown command";
	}

}

void serve(SOCKET client, Db& db) {

	std::string acc;
	// char buf[512];
	char buf[4096];

	int n = recv(client, buf, sizeof(buf), 0);
	
	if (n <= 0) {
		closesocket(client); return;
	}

	std::string request(buf, n);
	std::cout << request << std::endl;

	std::istringstream iss(request);
	std::string method, path, version;
	iss >> method >> path >> version; // "GET" "/hero" "HTTP/1.1"

	std::string key = path.substr(1);

    std::string body, status;

    if (method == "GET") body = handle(db, "GET " + key);
    if (method == "DELETE") body = handle(db, "DEL " + key);
	size_t hdr_end = request.find("\r\n\r\n");
	std::string req_body = request.substr(hdr_end + 4);
    if (method == "PUT") body = handle(db, "SET " + key + " " + req_body);
    if (method != "GET" && method != "DELETE" && method != "PUT") {
    	body = "method not supported"; status = "501 Not Implemented";
    }

    if (status.empty()) {
    	if (body == "(nil)") status = "404 Not Found";
    	else if (body == "ERR unknown command") status = "400 Bad Request";
    	else status = "200 OK";
    }

    std::string resp =
        "HTTP/1.1 " + status + "\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "\r\n" + body;
 
    send(client, resp.data(), (int)resp.size(), 0);	

	/*

	while (true) {
		// kernel keeps a per-socket mailbox
		// bytes arriving from the client pile up in a queue inside the OS
		// recv drains it
		// give me whatever sitting there up to sizeof(buf) bytes
		int n = recv(client, buf, sizeof(buf), 0);
		// n == 0 - client hung up
		// n < 0 - SOCKET_ERROR
		if (n <= 0) break;
		acc.append(buf, n);
		
		bool hangup = false;
		while (true) {
			size_t pos = acc.find('\n');
			if (pos == std::string::npos) break;
			std::string line = acc.substr(0, pos);
			acc.erase(0, pos + 1);
			std::string reply = handle(db, line);
			reply += '\n';
			// kernel outbound queue
			send(client, reply.data(), (int)reply.size(), 0);
			if (reply == "bye\n") {
				hangup = true; break;
			}			
		}

		if (hangup) break;

	}
	*/

	closesocket(client);

}

void sweeper(Db& db) {

	std::unique_lock<std::mutex> lk(db.m);

	while (true) {

		std::optional<Clock::time_point> earliest;
		for (auto& [key, e]: db.store) {
			if (e.deadline && (!earliest || *e.deadline < *earliest))
				earliest = e.deadline;
		}

		// cv - primitive for
		// "sleep until something may have changed, without burning CPU"
		// so we know of lock_guard but
		// the needed moves are release inside wait's execution
		// and reacquire before it returns
		if (!earliest) db.cv.wait(lk);
		else db.cv.wait_for(lk, *earliest - Clock::now());

		for (auto it = db.store.begin(); it != db.store.end(); ) {
			if (it->second.deadline && *it->second.deadline <= Clock::now())
				it = db.store.erase(it); // erase returns the next iterator
			else it++;
		}

	}

}

void worker(Db& db, TaskQueue& tasks) {
	while (true) {
		SOCKET client = tasks.take();
		serve(client, db);
	}
}

int main() {

	Db db;

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
	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, 0);
	if (listenSock == INVALID_SOCKET) {
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

	// register this phone number with this phone
	// sockaddr* - generic footprint
	// think of it as minimum shape, not a maximum size
	if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		std::cout << "bind failed: " << WSAGetLastError() << std::endl;
		closesocket(listenSock);
		WSACleanup(); return 1;
	}

	// opening for business
	// tells OS route incoming calls for this number to me and start queueing them
	// three intuition to hold
	// 1. listen converts the socket from a phone to a server
	// it is a one-way role change, it is a receptionist desk
	// 2. listen doesn't answer calls, it only starts collecting them
	// 3. SOMAXCONN - waiting room size, how many callers may sit queued before getting refused
	if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
		std::cout << "listen failed: " << WSAGetLastError() << std::endl;
		closesocket(listenSock);
		WSACleanup(); return 1;		
	}

	std::thread(sweeper, std::ref(db)).detach();

	TaskQueue tasks;
	for (int i = 0; i < 4; i++) 
		std::thread(worker, std::ref(db), std::ref(tasks)).detach();

	while (true) {
		// answering the call
		// it is a blocking wait
		SOCKET client = accept(listenSock, NULL, NULL);
		if (client == INVALID_SOCKET) {
			std::cout << "accept failed: " << WSAGetLastError() << std::endl;
			continue;			
		}
		// std::ref is required because of std::thread
		// server parameter is unordered_map&
		// but std::thread doesn't call serve directly
		// it copies its arguments into internal storage first
		// then invokes serve with those stored copies	

		// detach() - allows main to continue working
		// std::thread(serve, client, std::ref(db)).detach();

		tasks.push(client);

	}

	closesocket(listenSock);
	WSACleanup();

	return 0;

}