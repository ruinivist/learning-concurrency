#include <mutex>
#include <shared_mutex>

std::shared_mutex rw_lock;

void reader() {
    std::shared_lock lock(rw_lock);
    // reading section
}

void writer() {
    std::unique_lock lock(rw_lock);
    // writing section
}