#include <atomic>
#include <mutex>

class SpinLock {
   public:
    void lock() {
        while (true) {
            // lock aquire attemp, assuming non-contention bias
            if (!flag_.test_and_set(std::memory_order_acquire)) {
                return;
            }

            // Second "test":
            // Lock is currently held, so spin using READS only.
            while (flag_.test(std::memory_order_relaxed)) {
                // spin
            }

            // flag looked free, so loop back and try
            // test_and_set() again.
        }
    }

    void unlock() { flag_.clear(std::memory_order_release); }

   private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;
};

SpinLock lock;
int counter = 0;

void worker() {
    /*
    cpp has compile time duck typing on templates so the lock_guard is just a
    RAII guard which needs lock and unclock to exit on the template param
    */
    std::lock_guard<SpinLock> guard(lock);
    ++counter;
}