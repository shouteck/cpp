#include <iostream>

template<class T>
class SharedPtr {
public:
	explicit SharedPtr(T* p = nullptr) {
		ptr_ = p;
		if (p) count_ = new int(1);
		else count_ = nullptr;
	}
	~SharedPtr() {
		if (!count_) return;
		*count_ -= 1;
		if (*count_ == 0) {
			delete ptr_;
			delete count_;
		}
	}
	SharedPtr(const SharedPtr& other) {
		ptr_ = other.ptr_;
		count_ = other.count_;
		if (count_) *count_ += 1;
	}
	SharedPtr& operator=(const SharedPtr& other) {
		if (this == &other) return *this;
		if (count_) {
			*count_ -= 1;
			if (*count_ == 0) {
				delete ptr_;
				delete count_;
			}
		}
		ptr_ = other.ptr_;
		count_ = other.count_;
		if (count_) *count_ += 1;
		return *this;
	}
	SharedPtr(SharedPtr&& other) noexcept {
		ptr_ = other.ptr_;
		count_ = other.count_;
		other.ptr_ = nullptr;
		other.count_ = nullptr;
	}
	SharedPtr& operator=(SharedPtr&& other) noexcept {
		if (this == &other) return *this;
		if (count_) {
			*count_ -= 1;
			if (*count_ == 0) {
				delete ptr_;
				delete count_;
			}
		}		
		ptr_ = other.ptr_;
		count_ = other.count_;
		other.ptr_ = nullptr;
		other.count_ = nullptr;
		return *this;
	}
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
	int use_count() const {
		if (!count_) return 0;
		else return *count_;
	}
private:
	T* ptr_;
	int* count_;
};

int main() {
    SharedPtr<int> a(new int(42));
    // a.use_count() → 1
    std::cout << a.use_count() << std::endl;
    {
        SharedPtr<int> b(a);      // join → both print 2
        std::cout << a.use_count() << std::endl;
        std::cout << b.use_count() << std::endl;
        SharedPtr<int> c = a;     // join → 3
        std::cout << c.use_count() << std::endl;
    }                             // b, c die → a prints 1 again
    std::cout << a.use_count() << std::endl;
    // a's scope ends → count hits 0 → int + ledger freed
    return 0;
}