#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <print>
#include <thread>

template <typename T, std::size_t N>
class SPSC {
    static_assert(N >= 2, "need atleast 2 size");
    // think of cases 0 and 1, modulo issues there

   private:
    std::array<T, N> array_;
    // align in diff cache lines
    alignas(64) std::atomic<int> head_{0}, tail_{0};

   public:
    void push(T elem) {
        int tail = tail_.load(std::memory_order_relaxed);
        int head = head_.load(std::memory_order_acquire);

        int next_tail = (tail + 1) % N;
        if (next_tail == head) {
            // full
            // wait till value is different from old, this can
            // only mean that now we can write to tail
            head_.wait(head, std::memory_order_acquire);
        }

        array_[tail] = std::move(elem);
        tail_.store(next_tail, std::memory_order_release);
        // notify if waiting in pop
        tail_.notify_one();
    }

    T pop() {
        int tail = tail_.load(std::memory_order_acquire);
        int head = head_.load(std::memory_order_relaxed);

        if (head == tail) {
            // empty
            // if tail changes, something was pushed and we continue
            tail_.wait(tail, std::memory_order_acquire);
        }

        T elem = std::move(array_[head]);
        head = (head + 1) % N;
        head_.store(head, std::memory_order_release);
        head_.notify_one();
        return elem;
    }
};

int main() {
    SPSC<int, 4> queue;

    std::thread producer([&] {
        for (int i = 0; i < 20; i++) {
            queue.push(i);
            std::println("prod put {}", i);
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < 20; i++) {
            int value = queue.pop();
            std::println("cons got {}", value);
        }
    });

    producer.join();
    consumer.join();
}
