
#include <atomic>
#include <chrono>
#include <print>
#include <thread>

const int M = 10;
std::atomic<int> meals{M};
static_assert(std::atomic<int>::is_always_lock_free);

void chef() {
    while (1) {
        // this is how you do a atomic wait for a certain value
        int cur_m = meals.load(std::memory_order_relaxed);

        while (cur_m > 0) {
            // this will WAIT till notified, and won't automatically
            // wake up when value changes
            meals.wait(cur_m, std::memory_order_relaxed);
            cur_m = meals.load(std::memory_order_relaxed);
        }

        // meals is 0
        std::println("chef is filling...");
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        meals.store(M, std::memory_order_relaxed);
        meals.notify_all();
    }
}

void diner(int i) {
    while (true) {
        while (true) {
            int cur_m = meals.load(std::memory_order_relaxed);

            // bad state
            if (cur_m == 0) {
                meals.wait(0, std::memory_order_relaxed);
                continue;
            }

            // cas try to decrement
            if (meals.compare_exchange_weak(cur_m, cur_m - 1,
                                            std::memory_order_relaxed)) {
                if (cur_m == 1) {
                    // all as I can't directly target chef here
                    meals.notify_all();
                }

                std::println("diner {} is eating", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                break;
            }
        }
    }
};

int main() {
    std::vector<std::thread> threads;
    std::thread chef_t(chef);
    for (std::size_t i = 0; i < 5; i++) threads.emplace_back(diner, i);
    chef_t.join();
}