#include <atomic>
#include <chrono>
#include <mutex>
#include <print>
#include <thread>
#include <vector>

class Spinlock {
   private:
    std::atomic_flag flag_ = ATOMIC_FLAG_INIT;

   public:
    void lock() {
        // test and test and set ( rev )
        while (true) {
            // need to read -> lock -> acuire
            // !flag_ ... => old value was unset and we've set
            if (!flag_.test_and_set(std::memory_order_acquire)) {
                break;
            }

            // cheap reads, no ordering needed
            while (flag_.test(std::memory_order_relaxed)) {
                // spin
            }
        }
        // returns on crit section
    }

    // cpp's template duck typing, this'll throw on compile if no
    // unclock and lock are defined
    void unlock() {
        // I've written so release lock
        flag_.clear(std::memory_order_release);
    }
};

Spinlock lock;
int ctr = 0;

void worker(int i) {
    while (true) {
        std::lock_guard<Spinlock> guard(lock);
        std::println("Worker {} inc counter to {}", i, ctr++);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) threads.emplace_back(worker, i);

    threads[0].join();  // never stops so this is fine
}