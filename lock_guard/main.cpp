#include <atomic>
#include <mutex>
#include <thread>
#include <iostream>

class LockGuard {
public:
	explicit LockGuard(std::mutex& m) : m_(m) {
		m_.lock();
	}
	~LockGuard() {
		m_.unlock();
	}
	LockGuard(const LockGuard& other) = delete;
	LockGuard& operator=(const LockGuard& other) = delete;
	LockGuard(LockGuard&& other) = delete;
	LockGuard& operator=(LockGuard&& other) = delete;
private:
	// can't rebind to another mutex
	std::mutex& m_;
};

std::mutex m;
int counter = 0;

// counter += 1 is 3 steps
// atomic_count += 1 compiles down to one indivisible CPU instruction
std::atomic<int> atomic_counter(0);

void work() {
    for (int i = 0; i < 100000; i++) {
        // version A (racy):  counter++;
        // version B (guarded):
        { 
        	LockGuard lg(m); counter++; 
        }
        atomic_counter += 1;
    }
}

int main() {

    std::thread a(work);
    std::thread b(work);
    a.join(); b.join();
    std::cout << counter << "\n";   // want: 200000
    std::cout << atomic_counter << "\n";   // want: 200000

	return 0;

}