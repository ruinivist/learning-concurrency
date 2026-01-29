/*
N threads ( players ) start at the same time
each take some random time, print player i joined
when player i is done.

print game start when all is done
reset again

idea: rendezvous

use: mutex, condition variable, generation counters
*/

#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

class Barrier {
    int total_;
    int current_ = 0;
    int generation_counter_ = 0;

    std::mutex mtx;
    std::condition_variable cv;

   public:
    Barrier(int total) : total_(total) {}

    void arrive_and_wait(int player_id) {
        {
            std::unique_lock<std::mutex> lck(mtx);
            int this_generaion = generation_counter_;
            current_++;
            std::printf("Player %d arrived\n", player_id + 1);

            if (current_ == total_) {
                std::printf("Game found\n");
                current_ = 0;
                generation_counter_++;
                cv.notify_all();
            } else {
                cv.wait(lck, [&]() {
                    return this_generaion != generation_counter_;
                });
            }
        }

        cv.notify_all();
    }
};

int random_int(int low, int high) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(low, high);
    return dist(gen);
}

const int N = 10;
Barrier barrier(N);

void player(int player_id) {
    while (true) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(random_int(0, 2000)));
        barrier.arrive_and_wait(player_id);
    }
}

int main() {
    std::vector<std::thread> players;
    for (int i = 0; i < N; i++) players.emplace_back(player, i);

    for (auto& t : players) t.join();
}