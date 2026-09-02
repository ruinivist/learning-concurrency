#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <random>
#include <thread>
#include <vector>

class Barrier {
   private:
    int total_, current_, gen_ctr_ = 0;

    std::mutex mtx_;
    std::condition_variable cv_;

   public:
    Barrier(int total, int current = 0) : total_(total), current_(current) {};

    void arrive_and_wait() {
        std::unique_lock<std::mutex> lock(mtx_);
        int orig_gen = gen_ctr_;
        current_++;
        bool move_on = current_ == total_;
        if (move_on) {
            gen_ctr_++;
            current_ = 0;
            cv_.notify_all();
        } else {
            cv_.wait(lock, [&] { return orig_gen < gen_ctr_; });
        }
    }
};

int random_int(int low, int high) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(low, high);
    return dist(gen);
}

void player(int id, Barrier& barrier, int rounds) {
    for (int round = 1; round <= rounds; ++round) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(random_int(20, 100)));
        std::println("Player {} arrived (round {})", id, round);

        barrier.arrive_and_wait();

        std::println("--> Player {} starting round {} work", id, round);
    }
}

int main() {
    const int NUM_PLAYERS = 4;
    const int ROUNDS = 3;

    Barrier barrier(NUM_PLAYERS);
    std::vector<std::thread> players;
    players.reserve(NUM_PLAYERS);

    std::println("Starting game lobby with {} players across {} rounds:\n",
                 NUM_PLAYERS, ROUNDS);

    for (int i = 1; i <= NUM_PLAYERS; ++i) {
        players.emplace_back(player, i, std::ref(barrier), ROUNDS);
    }

    for (auto& t : players) {
        t.join();
    }

    std::println("\nAll rounds completed successfully.");
    return 0;
}