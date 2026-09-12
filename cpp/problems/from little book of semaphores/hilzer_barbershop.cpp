#include <chrono>
#include <deque>
#include <memory>
#include <print>
#include <semaphore>
#include <thread>
#include <vector>

const int MAX_CUSTOMERS = 20;  // fire code limit
const int SOFA_CAPACITY = 4;
const int N_BARBERS = 3;

int n_customers = 0;

// mutexes
std::binary_semaphore global_mtx{1};  // protects n_customers and standing_queue
std::binary_semaphore sofa_mtx{1};    // protects sofa_queue
std::binary_semaphore register_mtx{1};  // 1 cash register

// multiplex for the 4 sofa seats
std::counting_semaphore<SOFA_CAPACITY> sofa_sema{SOFA_CAPACITY};

// signals
std::counting_semaphore<> customer_standing{0};
std::counting_semaphore<> customer_on_sofa{0};
std::binary_semaphore payment_ready{0};
std::binary_semaphore receipt_ready{0};

// FIFO queues for store per-customer semaphores
std::deque<std::shared_ptr<std::binary_semaphore>> standing_queue;
std::deque<std::shared_ptr<std::binary_semaphore>> sofa_queue;

void barber(int id) {
    while (1) {
        // TODO: wait for customer from sofa, pop, and signal chair ready
        // TODO: cutHair()
        // TODO: acceptPayment() rendezvous at cash register
    }
}

// Optional: Downey's usher to keep sofa full while barbers cut hair
void usher(int id) {
    while (1) {
        // TODO: wait for standing customer, wait on sofa_sema, pop and signal
        // to sit
    }
}

void customer(int id) {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds((id + 1) * 150));
        std::println("customer {} arrived", id);

        // 1. enterShop() - check MAX_CUSTOMERS, balk if full, add to
        // standing_queue

        // 2. sitOnSofa() - wait for sofa spot, pop from standing, sit, add to
        // sofa_queue

        // 3. getHairCut() - wait for barber chair, leave sofa, get haircut

        // 4. pay() - rendezvous with a barber at the cash register

        // 5. exitShop() - decrement n_customers
    }
}

int main() {
    std::vector<std::thread> threads;

    for (int i = 0; i < N_BARBERS; i++) threads.emplace_back(barber, i);
    for (int i = 0; i < SOFA_CAPACITY; i++) threads.emplace_back(usher, i);
    for (int i = 0; i < 10; i++) threads.emplace_back(customer, i);

    threads[0].join();  // wait forever
}
