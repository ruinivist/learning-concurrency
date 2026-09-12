#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <thread>

const int N_CHAIRS = 5;
std::condition_variable chairs_free_cv, barber_chair_free_cv;
std::mutex mtx, barber_chair_mtx;
bool barber_chair_free = true;
int n_customers = 0;

void barber() {
    std::println("barber on haircut");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void customer(int i) {
    while (1) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds((i + 1) * 250));  // diff arrival gaps

        std::println("customer {} arrived", i);
        bool wait = false;
        {
            std::lock_guard<std::mutex> lock(mtx);
            wait = n_customers < N_CHAIRS;
            if (wait) n_customers++;
        }

        if (!wait) {
            std::println("customer {} walked out", i);

            continue;
        }

        // now wait for the barber chair
        {
            // this is again a mutex + counter patterns, semas are ideal here
            // but let's just do it this way in this solution
            std::unique_lock<std::mutex> lock(barber_chair_mtx);
            barber_chair_free_cv.wait(lock, [&] { return barber_chair_free; });
            barber_chair_free = false;
            std::println("customer {} sat for cut", i);
            barber();
            std::println("customer {} done with cut", i);
            barber_chair_free = true;
        }

        {
            // this is LITERALLY a sema
            std::lock_guard<std::mutex> lock(mtx);
            n_customers--;
        }

        barber_chair_free_cv.notify_one();
    }
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 6; i++) {
        threads.emplace_back(customer, i);
    }
    threads[0].join();
}