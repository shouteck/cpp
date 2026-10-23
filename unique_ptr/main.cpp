// this is to solve the problem of manually deleting a pointer from a caller

// class template
// a class-shaped blueprint that isn't a real class until you say which type it is for
template<class T>
class UniquePtr {
public:
	// explicit
	// dont translate here, if they want a UniquePtr, they must write UniquePtr

	// if no p is supplied, it defaults to nullptr
	explicit UniquePtr(T* p = nullptr) {
		ptr_ = p;
	}
	~UniquePtr() {
		delete ptr_;
	}
	// caller gets to mutate the pointee
	T* get() const {
		return ptr_;
	}
	T& operator*() const {
		return *ptr_;
	}
	T* operator->() const {
		return ptr_;
	}
	// if (p)
	explicit operator bool() const {
		return ptr_ != nullptr;
	}
	// forbids copying
	// two UniquePtr objects holding ptr_ to the same int
	// each believing this is mine to delete
	// double free can happen
	UniquePtr(const UniquePtr& other) = delete;
	UniquePtr& operator=(const UniquePtr& other) = delete;
	UniquePtr(UniquePtr&& other) noexcept {
		ptr_ = other.ptr_;
		other.ptr_ = nullptr;
	}
	UniquePtr& operator=(UniquePtr&& other) noexcept {
		if (this == &other) return *this;
		delete ptr_;
		ptr_ = other.ptr_;
		other.ptr_ = nullptr;
		return *this;
	}
	void reset(T* p = nullptr) {
		delete ptr_;
		ptr_ = p;
	}
	T* release() {
		T* tmp = ptr_;
		ptr_ = nullptr;
		return tmp;
	}
private:
	T* ptr_;
};

int main() {

	return 0;

}