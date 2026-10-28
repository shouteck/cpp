#include <atomic>

const int N = 512;

int buf[N];

alignas(64) std::atomic<int> head = 0;
alignas(64) std::atomic<int> tail = 0;

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

int main() {

	return 0;

}
