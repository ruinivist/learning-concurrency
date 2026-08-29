/*
nothing has fundamentally changed except by using
a fixed size dequeue, we are avoiding allocations
*/

#include <array>
#include <cstddef>
#include <mutex>
#include <print>
#include <random>
#include <semaphore>
#include <thread>

template <typename T, std::size_t N>
class BlockingRingQ {
   private:
    std::array<T, N> array_;
    std::mutex mtx_;
    int head_ = 0, tail_ = 0;
    std::counting_semaphore<N> free_{N}, taken_{0};

   public:
    BlockingRingQ() = default;
    // this'll delete any other ctors

    void push(T elem) {
        free_.acquire();
        {
            std::unique_lock<std::mutex> lock(mtx_);
            // already copied in arg so let's just move
            array_[tail_++] = std::move(elem);
            if (tail_ == N) tail_ = 0;
        }
        taken_.release();
    }

    T pop() {
        // optionals will help avoid the default constructible
        // need here but the array_ itself needs that req so fine
        // here as well
        T elem;
        taken_.acquire();
        {
            std::unique_lock<std::mutex> lock(mtx_);
            elem = std::move(array_[head_++]);
            if (head_ == N) head_ = 0;
        }
        free_.release();
        return elem;
    }
};

// ==== prod cons wiring ====

const int N_PROD = 4;
const int N_CONS = 8;
BlockingRingQ<int, 4> q;
const int low = 1, high = 5;

int make_dish() {
    thread_local std::mt19937 gen{std::random_device{}()};
    // read is always fine across threads if things are not changing
    // don't fall into the trap of jamming a sync everywhere
    thread_local std::uniform_int_distribution<int> dist(low, high);
    return dist(gen);
}

void producer(int chef) {
    while (1) {
        int dish = make_dish();
        std::println("Chef {} making dish {}", chef, dish);
        std::this_thread::sleep_for(
            std::chrono::seconds(dish));  // dish takes dish seconds of wait
        q.push(dish);
        std::println("Chef {} made dish {}", chef, dish);
    }
}

void consumer(int cust) {
    while (1) {
        int dish = q.pop();
        std::println("Customer {} ate dish {}", cust, dish);
    }
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < N_PROD; i++) threads.emplace_back(producer, i + 1);
    for (int i = 0; i < N_CONS; i++) threads.emplace_back(consumer, i + 1);

    // never stopping so we can just wait on first thread ig
    threads[0].join();

    return 0;
}
