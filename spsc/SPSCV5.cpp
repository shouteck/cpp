#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <queue>
#include <thread>
#include <cassert>

const int N = 512;
const int M = 1 << 20;

// ---------- lock-free SPSC (V4) ----------
int buf[N];
alignas(64) std::atomic<int> head{0};
alignas(64) std::atomic<int> tail{0};

bool push(int x) {
	if ((tail + 1) % N == head) return 0;
	buf[tail] = x;
	tail.store((tail + 1) % N, std::memory_order_release);
	return 1;
}

bool pop(int& x) {
	if (head == tail.load(std::memory_order_acquire)) return 0;
	x = buf[head];
	head.store((head + 1) % N, std::memory_order_release);
	return 1;
}

double bench_lockfree() {
	head = 0; tail = 0;
	auto t0 = std::chrono::steady_clock::now();
	std::thread prod([]{
		for (int i = 0; i < M; ++i)
			while (!push(i)) {}          // full -> spin
	});
	std::thread cons([]{
		for (int expected = 0; expected < M; ) {
			int x;
			if (pop(x)) { assert(x == expected); ++expected; }  // sequence check
		}
	});
	prod.join(); cons.join();
	return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now() - t0).count() / M;
}

// ---------- mutex + CV queue (bounded — same contract) ----------
std::queue<int> q;
std::mutex m;
std::condition_variable cv_not_empty, cv_not_full;

bool lpush(int x) {
	std::unique_lock<std::mutex> lk(m);
	cv_not_full.wait(lk, []{ return q.size() < N; });   // wait for room
	q.push(x);
	lk.unlock();
	cv_not_empty.notify_one();
	return 1;
}

bool lpop(int& x) {
	std::unique_lock<std::mutex> lk(m);
	cv_not_empty.wait(lk, []{ return !q.empty(); });     // wait for item
	x = q.front(); q.pop();
	lk.unlock();
	cv_not_full.notify_one();
	return 1;
}

double bench_locked() {
	auto t0 = std::chrono::steady_clock::now();
	std::thread prod([]{ for (int i = 0; i < M; ++i) lpush(i); });
	std::thread cons([]{
		for (int expected = 0; expected < M; ) {
			int x;
			lpop(x);
			assert(x == expected);          // sequence check
			++expected;
		}
	});
	prod.join(); cons.join();
	return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now() - t0).count() / M;
}

int main() {
	printf("lock-free SPSC : %8.1f ns/item\n", bench_lockfree());
	printf("mutex + cv     : %8.1f ns/item\n", bench_locked());
	return 0;
}
