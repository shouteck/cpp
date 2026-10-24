#include <thread>
#include <iostream>

class JoiningThread {
public:
	// a named variable is presumed still-in-use
	// std::move(t) is the signature "I certify that im done with t"
	explicit JoiningThread(std::thread t) : t_(std::move(t)) {
	}
	// join the thread before t_ own destructor runs
	~JoiningThread() {
		if (t_.joinable()) t_.join();
	}
private:
	std::thread t_;
};

int main() {

    {
        JoiningThread jt(std::thread([] {
            for (int i = 0; i < 5; i++)
                std::cout << "worker " << i << "\n";
        }));
        std::cout << "main continues\n";
    }   // jt dies → join() → worker finishes → clean exit
 
    return 0;
}