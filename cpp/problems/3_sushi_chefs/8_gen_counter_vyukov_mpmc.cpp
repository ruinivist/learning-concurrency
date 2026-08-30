#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>
#include <print>
#include <random>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

template <typename T, std::size_t N>
class MPMC {
    // there's no point ( or less so ) if it's not lock free from
    // arch
    static_assert(N >= 2);
    static_assert(std::atomic<std::size_t>::is_always_lock_free);
    static_assert(std::is_default_constructible_v<T>);
    // so that I don't need to handle throws on my moves
    static_assert(std::is_nothrow_move_assignable_v<T>);
    static_assert(std::is_nothrow_move_constructible_v<T>);

   private:
    struct Slot {
        std::atomic<std::size_t> seq;
        T data;
    };
    std::atomic<std::size_t> enq_pos_{0}, dq_pos_{0};
    std::array<Slot, N> buff_;

   public:
    MPMC() {
        // seq num on slot initially = pos
        for (std::size_t i = 0; i < N; i++) {
            buff_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    bool try_enq(T data) {
        // retry on a CAS failure ( as some other thread made progress )
        // it's only on seq not same as enq_pos that would mean that
        // some consumer HAS NOT yet read the data for which we want to
        // enq at AKA queue is full
        while (true) {
            // need changes by other prods
            std::size_t enq_pos = enq_pos_.load(std::memory_order_relaxed);
            std::size_t slot_pos = enq_pos % N;

            Slot& slot = buff_[slot_pos];
            // need changes by other cons on same slot
            // Why acquire? I initially assumed just relaxed would be
            // enough since I don't need to use data here, I'll anways
            // overwrite it but in general you ALWAYS need acq + rel semantics
            // ( separately, not talking about std::memory_order_acq_rel here )
            // both ways ( if you need it one way )
            // CASE: acquire here sync with a consumer's release op
            // so that a producer overwrite is NOT re-ordered to be done before
            // a consumer read => note that in the timeline the seq numbers
            // would still increase, there's no staleness there at all
            std::size_t seq = slot.seq.load(std::memory_order_acquire);

            // enq pos is leading but the slot hasn't been READ yet => full
            if (seq < enq_pos) {
                return false;
            }
            // the seq generation is ahead of enq pos retry
            if (seq > enq_pos) {
                continue;
            }

            // try CAS, CAS(address, expected, new), enq pos is the addr
            bool success = enq_pos_.compare_exchange_weak(
                enq_pos, enq_pos + 1, std::memory_order_relaxed);

            if (!success) continue;

            // write data
            slot.data = std::move(data);

            // publish seq number to be seq + 1 for consumers
            slot.seq.store(seq + 1, std::memory_order_release);

            return true;
        }
    }

    std::optional<T> try_dq() {
        while (true) {
            std::size_t dq_pos = dq_pos_.load(std::memory_order_relaxed);
            std::size_t slot_pos = dq_pos % N;
            Slot& slot = buff_[slot_pos];
            std::size_t seq = slot.seq.load(std::memory_order_acquire);

            // dq pos is stale
            if (seq > dq_pos + 1) continue;
            // empty
            if (seq < dq_pos + 1) return std::nullopt;

            // try cas
            // my bias in thinking that seq is changed here, it's dp pos
            // you just claim by letting other see the new value
            bool success = dq_pos_.compare_exchange_weak(
                dq_pos, dq_pos + 1, std::memory_order_relaxed);
            if (!success) continue;

            T elem = std::move(slot.data);

            // set seq num to pos + cap
            std::size_t new_seq = dq_pos + N;
            slot.seq.store(new_seq, std::memory_order_release);

            return elem;
        }
    }

    static constexpr std::size_t capacity() { return N; }
};

// ==== prod cons wiring ====

const int N_PROD = 4;
const int N_CONS = 8;
MPMC<int, 4> q;
std::atomic<unsigned> dishes_changed{0}, space_changed{0};

const int low = 1;
const int high = 5;

int make_dish() {
    thread_local std::mt19937 gen{std::random_device{}()};
    thread_local std::uniform_int_distribution<int> dist(low, high);
    return dist(gen);
}

void producer(int chef) {
    while (true) {
        int dish = make_dish();

        std::println("Chef {} making dish {}", chef, dish);

        // takes dish seconds to make.
        std::this_thread::sleep_for(std::chrono::seconds(dish));

        // version at start
        auto version = space_changed.load();
        // try and fail first
        while (!q.try_enq(dish)) {
            // wait till change
            space_changed.wait(version);
            // u need a reload because even if the space
            // changed, the next enq might fail as some other
            // prod succeeded
            version = space_changed.load();
        }
        ++dishes_changed;
        dishes_changed.notify_one();

        std::println("Chef {} made dish {}", chef, dish);
    }
}

void consumer(int cust) {
    while (true) {
        auto version = dishes_changed.load();
        // try first
        auto dish = q.try_dq();

        // else loop on wait
        while (!dish) {
            dishes_changed.wait(version);
            version = dishes_changed.load();
            dish = q.try_dq();
        }
        ++space_changed;
        space_changed.notify_one();

        std::println("Customer {} ate dish {}", cust, *dish);
    }
}

int main() {
    std::vector<std::thread> threads;

    for (int i = 0; i < N_PROD; ++i) threads.emplace_back(producer, i + 1);
    for (int i = 0; i < N_CONS; ++i) threads.emplace_back(consumer, i + 1);

    // wait forever
    threads[0].join();
}
