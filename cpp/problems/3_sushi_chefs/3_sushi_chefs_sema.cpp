/*
Bounded Blocking Queue

P chefs and C customers.
The conveyor belt has N slots.
Chef only makes when a slot is empty.
Customer keeps on sitting until sushi arrives.

Simulate via threads untile each consumer has eaten thrice.

Uses: cv, mutex
*/

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <queue>
#include <random>
#include <thread>

template <typename T>
class BlockingQueue {
   private:
    std::queue<T> queue;
    std::mutex mtx;

    // upper cap, what's unused is optimised away at compile time
    std::counting_semaphore<1000> empty_slots;
    std::counting_semaphore<1000> full_slots;

   public:
    const int N;
    BlockingQueue(int N) : empty_slots(N), full_slots(0), N(N) {}

    void push(T elem) {
        // 1. Wait for space, blocking decrement
        empty_slots.acquire();

        {
            std::lock_guard<std::mutex> lck(mtx);
            queue.push(std::move(elem));
        }

        // 2. Signal that a slot if filled
        full_slots.release();
    }

    T take() {
        // 1. Wait for food (Decrements full_slots. Blocks if 0).
        full_slots.acquire();

        T elem;
        {
            std::lock_guard<std::mutex> lck(mtx);
            elem = std::move(queue.front());
            queue.pop();
        }

        // 2. Signal that space is ready (Increments empty_slots).
        empty_slots.release();
        return elem;
    }
};

const int N = 5, P = 2, C = 3;
const int EAT_COUNT = 3;
int total_produces = C * EAT_COUNT;  // thrice
std::mutex chef_mutex;
BlockingQueue<std::string> queue(N);

int random_int(int low, int high) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(low, high);
    return dist(gen);
}

void chef(int chef_index) {
    while (true) {
        {
            std::unique_lock<std::mutex> lck(chef_mutex);
            if (total_produces > 0)
                total_produces--;
            else
                break;
        }
        int time = random_int(1, 2);
        std::printf("Chef %d is cooking for %d seconds...\n", chef_index + 1,
                    time);
        std::this_thread::sleep_for(std::chrono::seconds(time));
        queue.push("dish");
    }
}

std::vector<int> eat_count(C);
void customer(int customer_index) {
    while (eat_count[customer_index] < EAT_COUNT) {
        int time = random_int(3, 5);
        queue.take();
        std::printf("Customer %d is eating for %d seconds...\n",
                    customer_index + 1, time);
        eat_count[customer_index]++;
    }
}

int main() {
    std::vector<std::thread> chefs(P), customers(C);
    for (int i = 0; i < P; i++) {
        chefs[i] = std::thread(chef, i);
    }
    for (int i = 0; i < C; i++) {
        customers[i] = std::thread(customer, i);
    }

    for (auto& t : chefs) t.join();
    for (auto& t : customers) t.join();
}