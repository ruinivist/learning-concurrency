
#include <chrono>
#include <print>
#include <semaphore>
#include <thread>

const int M = 10;
std::binary_semaphore mtx{1};
// same as prod consumer
int meals = M;
std::binary_semaphore empty{0}, full{0};

void chef() {
    while (1) {
        empty.acquire();
        meals = M;
        std::println("chef is cooking...");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        full.release();
    }
}

void diner(int i) {
    while (true) {
        mtx.acquire();
        std::println("diner {} is eating", i);
        if (--meals == 0) {
            empty.release();
            full.acquire();
        }
        mtx.release();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

int main() {
    std::vector<std::thread> threads;
    std::thread chef_t(chef);
    for (std::size_t i = 0; i < 5; i++) threads.emplace_back(diner, i);
    chef_t.join();
}