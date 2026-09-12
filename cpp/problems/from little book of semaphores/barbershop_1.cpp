#include <chrono>
#include <print>
#include <semaphore>
#include <thread>

const int N_CHAIRS = 5;
int n_customers = 0;
// i'm this "INVERSE" state as too common, semas often are used with such
// partition states
std::binary_semaphore done_haircut{0}, barber_chair{1}, global_mtx{1},
    haircut_request{0};

void barber() {
    while (1) {
        haircut_request.acquire();
        std::println("barber on cut");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::println("barber done with cut");
        done_haircut.release();
    }
}

void customer(int i) {
    while (1) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds((i + 1) * 250));  // diff arrival gaps

        std::println("customer {} arrived", i);

        global_mtx.acquire();
        bool free_chair = n_customers < N_CHAIRS;
        if (!free_chair) {
            std::println("customer {} walked out", i);
            global_mtx.release();
            continue;
        } else {
            n_customers++;
            global_mtx.release();
        }

        barber_chair.acquire();  // wait for the sticker
        std::println("customer {} sat for cut", i);
        haircut_request.release();  // ask the barber for a cut
        done_haircut.acquire();     // wait for it to be done
        std::println("customer {} done with cut", i);

        // I could've done this after the barber chair release, technically this
        // just hold the barber chair possibly longer; doing it later frees it
        // for another already waiting customer but then has a bried window
        // where chairs are free but a new customer will see a full
        global_mtx.acquire();
        n_customers--;
        global_mtx.release();

        barber_chair.release();
    }
}

int main() {
    std::vector<std::thread> threads;
    std::thread barber_thread(barber);
    for (int i = 0; i < 6; i++) threads.emplace_back(customer, i);
    barber_thread.join();
}