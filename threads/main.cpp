#include <thread>
#include <iostream>

int main() {

	// experiment 1 - interleaving
	/*
	std::thread a([]{ 
		for (int i = 0; i < 100000; i++) std::cout << "A"; 
	});
	std::thread b([]{ 
		for (int i = 0; i < 100000; i++) std::cout << "B"; 
	});
	a.join(); b.join();

	std::cout << std::endl;
	*/

	// experiment 2 - shared data corrupts silently
	// counter++ is 3 steps, read, add, write
	// both threads read 5, compute 6, and write 6, one increment evaporates
	
	int counter = 0;
	std::thread a([&]{ 
		for (int i = 0; i < 100000; i++) counter++; 
	});
	std::thread b([&]{ 
		for (int i = 0; i < 100000; i++) counter++; 
	});
	a.join(); b.join();

	std::cout << counter << std::endl;

	return 0;

}