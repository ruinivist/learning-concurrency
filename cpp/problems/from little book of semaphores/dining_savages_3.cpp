#include <barrier>
#include <chrono>
#include <print>
#include <semaphore>
#include <thread>
#include <vector>

constexpr int M = 10;
std::counting_semaphore<M> servings{M};
std::barrier empty_pot{M + 1};  // M servings consumed + one waiting chef

void chef() {
    while (true) {
        empty_pot.arrive_and_wait();
        std::println("chef is filling...");
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        servings.release(M);
    }
}

void diner(int id) {
    while (true) {
        servings.acquire();
        std::println("diner {} is eating", id);
        static_cast<void>(empty_pot.arrive());
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main() {
    std::thread chef_thread(chef);
    std::vector<std::thread> diners;

    for (int id = 0; id < 5; ++id) diners.emplace_back(diner, id);

    chef_thread.join();
}
