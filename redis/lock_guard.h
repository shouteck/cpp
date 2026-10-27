// headers = shareable declarations
// a class definition in a .h can be included in any .cpp

// it prevents transitive double-inclusion
#pragma once
#include <mutex>

class LockGuard {
public:
    explicit LockGuard(std::mutex& m) : m_(m) { m_.lock(); }
    ~LockGuard() { m_.unlock(); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    LockGuard(LockGuard&&) = delete;
    LockGuard& operator=(LockGuard&&) = delete;
private:
    std::mutex& m_;
};