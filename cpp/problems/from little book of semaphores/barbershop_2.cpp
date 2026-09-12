#include <chrono>
#include <deque>
#include <memory>
#include <print>
#include <semaphore>
#include <thread>

const int N_CHAIRS = 5;
int n_customers = 0;
// i'm this "INVERSE" state as too common, semas often are used with such
// partition states
std::binary_semaphore global_mtx{1};
std::counting_semaphore customer_ready{0};
std::binary_semaphore barber_done{0}, customer_done{0};
std::deque<std::shared_ptr<std::binary_semaphore>> queue;

void barber() {
    while (1) {
        // note the 2 way signalling, these alternate
        customer_ready.acquire();

        global_mtx.acquire();
        // rem: when using shared pts, copy by value
        // i have a habit of assigning by reference through auto&
        auto barber_ready = queue.front();
        queue.pop_front();
        global_mtx.release();

        barber_ready->release();

        std::println("barber on cut");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::println("barber done with cut");
        // does this order matter? YES, and will depend on how customer does it
        // else both can be waiting on the next line
        barber_done.release();
        customer_done.acquire();
    }
}

void customer(int i) {
    while (1) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds((i + 1) * 250));  // diff arrival gaps

        std::println("customer {} arrived", i);
        auto barber_ready = std::make_shared<std::binary_semaphore>(0);

        global_mtx.acquire();
        bool free_chair = n_customers < N_CHAIRS;
        if (!free_chair) {
            std::println("customer {} walked out", i);
            global_mtx.release();
            continue;
        } else {
            n_customers++;
            queue.push_back(barber_ready);
            global_mtx.release();
        }

        customer_ready.release();
        barber_ready->acquire();
        std::println("customer {} sat for cut", i);
        // again, alternates
        barber_done.acquire();
        customer_done.release();

        std::println("customer {} done with cut", i);

        global_mtx.acquire();
        n_customers--;
        global_mtx.release();
    }
}

int main() {
    std::vector<std::thread> threads;
    std::thread barber_thread(barber);
    for (int i = 0; i < 6; i++) threads.emplace_back(customer, i);
    barber_thread.join();
}