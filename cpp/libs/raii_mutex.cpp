#include <mutex>

std::mutex mtx;
int ctr = 0;

void lock_immediately() {
    std::lock_guard<std::mutex> lock(mtx);
    ctr++;
}

// JUST USE THIS ONE BY DEFAULT
void lock_flexible() {
    std::unique_lock<std::mutex> lock(mtx, std::defer_lock);
    // some code
    lock.lock();
    ctr++;
    // lock is released when going out of scope
}

std::mutex mtx2;
void multi_lock() {
    std::scoped_lock lock(mtx, mtx2);
    ctr++;
}
