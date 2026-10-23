#include <iostream>
#include <cassert>

class MaramVector {
public:
	MaramVector() {
		data_ = nullptr;
		size_ = 0;
		capacity_ = 0;
	}
	~MaramVector() {
		// 1. run each element's destructor
		// 2. free the whole memory block
		delete[] data_;
	}
	// this is answering what should MaramVector b = a; mean?

	// why the parameter is a reference
	// passing by value requires making a copy of the parameter
	// causing infinite recursion

	// const because you only read other 
	MaramVector(const MaramVector& other) {
		data_ = nullptr;
		size_ = 0;
		capacity_ = 0;		
		reserve(other.capacity_);
		for (int i = 0; i < other.size_; i++) data_[i] = other.data_[i];
		size_ = other.size_;
	}
	MaramVector& operator=(const MaramVector& other) {
		if (this == &other) return *this;
		if (capacity_ >= other.size_) {
			for (int i = 0; i < other.size_; i++) data_[i] = other.data_[i];
			size_ = other.size_;
		}
		else {
			int* newHome = new int[other.size_];
			for (int i = 0; i < other.size_; i++) newHome[i] = other.data_[i];
			delete[] data_;
			data_ = newHome;
			size_ = other.size_;
			capacity_ = other.size_;
		}
		return *this;
	}
	// noexcept = this function never throws, if it somehow does, terminate the program
	// if move throws halfway, the damage is done
	// data_ was stolen, elements pilfered, source is a husk, target isn't finished

	// &&
	// Before C++11 existed
	// T& - the writable handle, refuses temporaries
	// const T& - read-only handle, binds to everything
	// for decades, a temporary can be seen not touched
	// however in move semantics
	// stealing requires writing the source
	MaramVector(MaramVector&& other) noexcept {
		data_ = other.data_;
		size_ = other.size_;
		capacity_ = other.capacity_;
		other.data_ = nullptr;
		other.size_ = 0;
		other.capacity_ = 0;
	}
	MaramVector& operator=(MaramVector&& other) noexcept {
		if (this == &other) return *this;
		delete[] data_;
		data_ = other.data_;
		size_ = other.size_;
		capacity_ = other.capacity_;
		other.data_ = nullptr;
		other.size_ = 0;
		other.capacity_ = 0;
		return *this;
	}	
	// trailing const constrains the function's own body
	int size() const {
		return size_;
	}
	int capacity() const {
		return capacity_;
	}
	bool empty() const {
		return size_ == 0;
	}
	// front const constrains the caller's side
	const int* data() const {
		return data_;
	}
	// it doesn't have a trailing const
	// a const MaramVector could call it and receive int*
	int* data() {
		return data_;
	}
	const int& operator[](int index) const {
		return data_[index];
	}
	// reference & is used here so that you could do mv[0] = 5
	int& operator[](int index) {
		return data_[index];
	}	
	void reserve(int n) {
		if (n <= capacity_) return;
		// secure new home
		int* newHome = new int[n];
		// move furniture
		for (int i = 0; i < size_; i++) newHome[i] = data_[i];
		// release the old lease
		delete[] data_;

		data_ = newHome;
		capacity_ = n;
	}
	void push_back(int x) {
		if (size_ == capacity_) {
			if (capacity_ == 0) reserve(1);
			else reserve(2 * capacity_);
		}
		data_[size_] = x;
		size_ += 1;		
	}
	void pop_back() {
		if (size_ == 0) return;
		size_ -= 1;
	}
	void clear() {
		size_ = 0;
	}
	void resize(int n) {
		if (size_ == n) return;
		else if (size_ > n) size_ = n;
		else {
			if (capacity_ < n) reserve(n);
			for (int i = size_; i < n; i++) data_[i] = 0;
			size_ = n;
		}
	}
private:
	// a pointer to the heap array that holds the elements
	int* data_;
	// elements stored right now
	int size_;
	// elements the current array can hold
	// this is introduced to solve computation time spent on copying over elements
	// naive case: 0 + 1 + 2 + ... + n = (n(n + 1)) / 2
	int capacity_;
};

int main() {

	MaramVector mv;

	return 0;

}