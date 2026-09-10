
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <thread>

const int M = 10;
std::mutex mtx;
// rem: my naming convention is what the "continue" condition is
std::condition_variable not_empty_cv, empty_cv;
int meals = 0;

void chef() {
    while (1) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            empty_cv.wait(lock, [&] { return meals == 0; });
            std::println("chef is filling...");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            meals = M;
        }
        not_empty_cv.notify_all();
    }
}

void diner(int i) {
    while (true) {
        bool notify_chef = false;
        {
            std::unique_lock<std::mutex> lock(mtx);
            not_empty_cv.wait(lock, [&] { return meals > 0; });
            notify_chef = meals == 1;
            meals--;
        }
        if (notify_chef) empty_cv.notify_one();

        std::println("diner {} is eating", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

int main() {
    std::vector<std::thread> threads;
    std::thread chef_t(chef);
    for (std::size_t i = 0; i < 5; i++) threads.emplace_back(diner, i);
    chef_t.join();
}