#include <condition_variable>
#include <mutex>

bool ready = false;
std::condition_variable cv;
std::mutex mtx;

void wait_for_condition() {
    std::unique_lock<std::mutex> lock(mtx);
    while (!ready) {
        cv.wait(lock);
    }
}

void wait_modern_syntax() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready; });
}