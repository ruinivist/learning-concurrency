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
    bool try_push(T elem) {
        // READ needed vars at start, that's easier and less error prone for
        // impl this can be relaxes because push owns tail
        int tail = tail_.load(std::memory_order_relaxed);
        // head used by cons so need sync
        int head = head_.load(std::memory_order_acquire);

        int next_tail = (tail + 1) % N;
        if (next_tail == head) return false;

        // if we reach here, it's not full. single producer so not possible for
        // something else to modify
        // a pop only makes it more empty

        // array_[tail_] is same as array_[tail_.load()] as it gets converted
        // implicitly but rather just use the created var
        array_[tail] = std::move(elem);
        // note that we only publish AFTER modifying the data

        // release lock / publish
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    std::optional<T> try_pop() {
        // only cons reads head, so relaxed
        int head = head_.load(std::memory_order_relaxed);
        // prod reads to need sync
        int tail = tail_.load(std::memory_order_acquire);

        // rem: [open,close) semantics
        if (head == tail) return std::nullopt;

        // modify data
        T ret = std::move(array_[head]);

        int next_head = (head + 1) % N;
        // then publish
        head_.store(next_head, std::memory_order_release);
        return ret;
    }
};

int main() {
    SPSC<int, 4> queue;

    std::thread producer([&] {
        for (int i = 0; i < 20;) {
            if (queue.try_push(i)) {
                std::print("prod put {}", ++i);
            } else {
                // tell sched to relinquish control to another thread
                // "yielding" is key in cooperative algos
                // yield is a HINT mostly, these two threads are pretty much
                // spin-waiting
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        for (int i = 0; i < 20;) {
            if (auto value = queue.try_pop()) {
                std::println("cons got {}", *value);
                ++i;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();
}
