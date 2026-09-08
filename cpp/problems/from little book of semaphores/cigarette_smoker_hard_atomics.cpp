#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <random>
#include <thread>
#include <vector>

using counter = std::atomic<std::uint64_t>;

struct Scoreboard {
    std::uint32_t item;  // 0, 1, 2, or 3 when empty
    std::uint32_t count;
};

static_assert(std::atomic<Scoreboard>::is_always_lock_free);
static_assert(counter::is_always_lock_free);

std::atomic<Scoreboard> scoreboard{Scoreboard{3, 0}};
// These counters carry permits only; there is no separate payload to publish.
// Relaxed operations preserve atomic counting without acquire/release ordering.
std::array<counter, 3> pusher_pending{};
std::array<counter, 3> smoker_ready{};

int rand_int() {
    std::mt19937 mt{std::random_device{}()};
    std::uniform_int_distribution<int> gen(0, 2);
    return gen(mt);
}

void post(counter& permits) {
    permits.fetch_add(1, std::memory_order_relaxed);
    permits.notify_one();
}

void take(counter& permits) {
    // note this CAS as a try and then the later while loop as a pattern
    // that's easier to reason about
    auto try_take = [&] {
        // read
        auto current = permits.load(std::memory_order_relaxed);

        // if it's not 0, keep on trying cas till it's still not 0
        while (current != 0) {
            // Success claims one permit. Failure refreshes current for retry.
            if (permits.compare_exchange_weak(current, current - 1,
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {
                return true;
            }
        }

        return false;
    };

    while (!try_take()) {
        permits.wait(0, std::memory_order_relaxed);
    }
}

// Atomically stores an unmatched item or reserves a completed pair.
// The whole state is in one atomic: relaxed CAS still updates it indivisibly.
int place(std::uint32_t arriving) {
    auto old = scoreboard.load(std::memory_order_relaxed);

    while (true) {
        auto next = old;
        int notify = -1;

        if (old.count == 0) {
            next = {arriving, 1};
        } else if (old.item == arriving) {
            next.count++;
        } else {
            notify =
                3 - static_cast<int>(old.item) - static_cast<int>(arriving);
            next.count--;
            if (next.count == 0) next.item = 3;
        }

        if (scoreboard.compare_exchange_weak(old, next,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
            return notify;
        }
    }
}

void agent() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        int exclude = rand_int();
        for (int item = 0; item < 3; item++) {
            if (item != exclude) post(pusher_pending[item]);
        }
    }
}

void pusher(int item) {
    while (true) {
        take(pusher_pending[item]);

        int notify = place(item);
        if (notify != -1) {
            post(smoker_ready[notify]);
        }
    }
}

void smoker(int have) {
    while (true) {
        take(smoker_ready[have]);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

int main() {
    std::thread ag(agent);
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) threads.emplace_back(pusher, i);
    for (int i = 0; i < 3; i++) threads.emplace_back(smoker, i);
    ag.join();  // forever wait
}
