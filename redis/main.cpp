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

#include "lock_guard.h"

using Clock = std::chrono::steady_clock;

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

void serve_http(SOCKET client, Db& db, const std::string& request) {

	// most threads spend most life blocked inside recv - sleeping
	// but here's the thing, the OS already knows which sockets have bytes ready
	// instead of N threads each asking "anything for me" repeatedly
	// one thread asks the OS "which of all my sockets are ready right now"

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

void worker(SOCKET listenSock, Db& db) {

	// previously, the queue's job was dealing clients to workers
	// in this model, accept moved inside the workers
	// every worker select watches the shared listener directly

    std::unordered_map<SOCKET, std::string> sessions;
    u_long mode = 1; char buf[512];

    while (true) {

    	// set of sockets to watch
        fd_set readfds;
        // clear sockets
        // if you skipped FD_ZERO, dead sockets may linger
        FD_ZERO(&readfds);
        // add listenSock to the watch list
        FD_SET(listenSock, &readfds);
        // add this worker's own clients to the watch list
        for (auto& [s, acc] : sessions) FD_SET(s, &readfds);

        // sleep till anyone's ready
        select(0, &readfds, NULL, NULL, NULL);

        // listener ready, a knock pending
        if (FD_ISSET(listenSock, &readfds)) {
            SOCKET c = accept(listenSock, NULL, NULL);
            if (c != INVALID_SOCKET) {
                ioctlsocket(c, FIONBIO, &mode);
                sessions[c] = "";
            }
        }

        // walk into MY section — service whoever's ready
        for (auto it = sessions.begin(); it != sessions.end(); ) {
            SOCKET s = it->first;
            std::string& acc = it->second;

            if (!FD_ISSET(s, &readfds)) { ++it; continue; }

            int n = recv(s, buf, sizeof(buf), 0);
            // hangup → close + remove
            if (n <= 0) {
                closesocket(s);
                it = sessions.erase(it);
                continue;
            }

            acc.append(buf, n);

            // a complete request = headers ending \r\n\r\n + Content-Length body bytes
            // consume one request's worth, keep the tail — keep-alive:
            // the connection persists, the next request may already be in acc
            while (true) {
                size_t hdr_end = acc.find("\r\n\r\n");
                if (hdr_end == std::string::npos) break;   // headers incomplete

                // body length declared by Content-Length header (0 when absent)
                size_t body_len = 0;
                size_t cl_pos = acc.find("Content-Length: ");
                if (cl_pos != std::string::npos && cl_pos < hdr_end)
                    body_len = (size_t)std::stoul(acc.substr(cl_pos + 16));

                size_t req_len = hdr_end + 4 + body_len;
                if (acc.size() < req_len) break;           // body incomplete

                std::string request = acc.substr(0, req_len);
                acc.erase(0, req_len);                     // keep the tail
                serve_http(s, db, request);
            }
            ++it;
        }
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
	u_long mode = 1;
	ioctlsocket(listenSock, FIONBIO, &mode);
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

	// N workers each run their own select loop — shared listener, racing accepts
	for (int i = 0; i < 4; i++)
		std::thread(worker, listenSock, std::ref(db)).detach();
	worker(listenSock, db);   // main is worker #0 — don't waste the thread

	closesocket(listenSock);
	WSACleanup();

	return 0;

}