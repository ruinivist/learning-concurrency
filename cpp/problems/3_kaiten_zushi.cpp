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
    std::condition_variable producer_cv, consumer_cv;

   public:
    const int N;
    BlockingQueue(int N) : N(N) {}

    void push(T elem) {
        {
            std::unique_lock<std::mutex> lck(mtx);
            producer_cv.wait(lck, [&] {
                return (int)queue.size() < N;
            });  // wait untile size < N
            queue.push(std::move(elem));
        }
        consumer_cv.notify_one();  // one customer
    }

    T take() {
        T elem;
        {
            std::unique_lock<std::mutex> lck(mtx);
            consumer_cv.wait(lck, [&] { return (int)queue.size() > 0; });
            elem = queue.front();
            queue.pop();
        }
        producer_cv.notify_one();
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