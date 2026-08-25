#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <queue>
#include <random>
#include <thread>

// ==== bounded blocking queue ====

template <typename T>
class BlockingQueue {
   private:
    std::queue<T> q_;
    std::mutex mtx_;
    // naming is who notifies: prod_cv => prod notifies
    std::condition_variable prod_cv_, cons_cv_;

   public:
    const std::size_t size;
    BlockingQueue(std::size_t size) : size(size) {}

    void push(T elem) {
        // ignoring copy here
        {
            std::unique_lock<std::mutex> lck(mtx_);
            // push needs to wait till a consumer notifiers of a pop
            cons_cv_.wait(lck, [&] { return q_.size() < size; });
            q_.push(elem);
        }
        prod_cv_.notify_one();
    }

    T pop() {
        T elem;  // needs T to be default constructible
        {
            std::unique_lock<std::mutex> lck(mtx_);
            // pop needs to wait till a producer notifiers of a push
            prod_cv_.wait(lck, [&] { return !q_.empty(); });
            elem = q_.front();
            q_.pop();
        }
        cons_cv_.notify_one();
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
