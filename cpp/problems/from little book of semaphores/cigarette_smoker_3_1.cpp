#include <algorithm>
#include <array>
#include <chrono>
#include <mutex>
#include <print>
#include <random>
#include <semaphore>
#include <thread>
#include <vector>

using sema = std::binary_semaphore;
std::mutex mtx;
sema agent_sema{1};
std::array<sema, 3> pusher_sema{sema(0), sema(0), sema(0)};
std::array<sema, 3> smoker_sema{sema(0), sema(0), sema(0)};
std::array<int, 3> table{};

int rand_int() {
    std::mt19937 mt{std::random_device{}()};
    std::uniform_int_distribution<int> gen(0, 2);
    return gen(mt);
}

void agent() {
    while (true) {
        /*
        agent will put two RANDOM items on the table
        and wait for a signal FROM a smoker
        */

        int exclude = rand_int();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        agent_sema.acquire();  // use ticker => 0
        for (int i = 0; i < 3; i++)
            if (i != exclude) pusher_sema[i].release();
    }
}

// this pushed places {item} on the table and wakes up relevant smoker
void pusher(int item) {
    while (true) {
        int notif = -1;
        pusher_sema[item].acquire();  // use ticket
        {
            std::unique_lock<std::mutex> lock(mtx);
            table[item]++;
            std::println("pusher puts {}", item);
            int on_table =
                std::ranges::count_if(table, [](int x) { return x > 0; });
            if (on_table == 2) {
                notif = 3;
                for (int i = 0; i < 3; i++)
                    if (table[i]) notif -= i;
            }
        }

        if (notif != -1) {
            smoker_sema[notif].release();  // give ticket
            std::println("pusher {} notified {}", item, notif);
        } else {
            std::println("pusher {} did not notify", item);
        }
    }
}

void smoker(int have) {
    while (true) {
        smoker_sema[have].acquire();
        {
            std::lock_guard<std::mutex> lock(mtx);
            table.fill(0);
            std::println("Smoker {} smokes", have);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        agent_sema.release();
    }
}

int main() {
    std::thread ag(agent);
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) threads.emplace_back(pusher, i);
    for (int i = 0; i < 3; i++) threads.emplace_back(smoker, i);
    ag.join();  // forever wait

    return 0;
}