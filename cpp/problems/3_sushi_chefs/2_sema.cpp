#include <chrono>
#include <mutex>
#include <print>
#include <queue>
#include <random>
#include <semaphore>
#include <thread>

// ==== bounded blocking queue ====

template <typename T>
class BlockingQueue {
   private:
    // note that while it does give me signatlling, the queue is not locked
    // on a critical section so I still need a mutex
    std::mutex mtx_;
    std::queue<T> q_;
    // this needs to be HIGH enough and needs to be defined COMPILE TIME
    // though I this I read is mostly used for optimisations as it signals
    // that ATLEAST this size is to be supported, you can specify it high
    // enough for "practical" purposes; it's not taking up any extra space
    std::counting_semaphore<1000> free_, taken_;

   public:
    const std::size_t size;
    BlockingQueue(std::size_t size) : size(size), free_(size), taken_(0) {}

    void push(T elem) {
        // ignoring copy here
        // wait for free to acuire ( take ) a slot
        free_.acquire();
        {
            std::unique_lock<std::mutex> lck(mtx_);
            q_.push(elem);
        }
        taken_.release();
    }

    T pop() {
        T elem;  // needs T to be default constructible
        taken_.acquire();
        {
            std::unique_lock<std::mutex> lck(mtx_);
            elem = q_.front();
            q_.pop();
        }
        free_.release();
        return elem;
    }
};

// ==== prod cons wiring ====

const int N_PROD = 4;
const int N_CONS = 8;
BlockingQueue<int> q(4);
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
