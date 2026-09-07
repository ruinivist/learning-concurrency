#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <random>
#include <ranges>
#include <thread>
#include <vector>

std::mutex mtx;
std::condition_variable agent_cv;
std::array<std::condition_variable, 3> pusher_cv{};
std::array<std::condition_variable, 3> smoker_wake_cv{};
std::array<int, 3> table{};
std::array<int, 3> push_pending{};
bool agent_can_place = true;

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

        // as a cheap trick we generate what to exclude
        int exclude = rand_int();
        std::this_thread::sleep_for(std::chrono::seconds(1));
        {
            std::unique_lock<std::mutex> lock(mtx);
            // this is wrong as the table remains empty betwen the agent
            // notifying pushers AND the smoker consuming it
            // need another state
            // agent_cv.wait(lock, [&] {
            //     return std::ranges::all_of(table,
            //                                [](int elem) { return elem == 0;
            //                                });
            // });
            agent_cv.wait(lock, [&] { return agent_can_place; });
            agent_can_place = false;
            for (int i = 0; i < 3; i++)
                if (i != exclude) push_pending[i]++;
        }
        for (int i = 0; i < 3; i++)
            if (i != exclude) pusher_cv[i].notify_one();
    }
}

// this pushed places {item} on the table and wakes up relevant smoker
void pusher(int item) {
    while (true) {
        int notif = -1;
        {
            std::unique_lock<std::mutex> lock(mtx);
            // this conditionless wait is wrong
            // cvs can wake up spuriously
            // pusher_cv[item].wait(lock);
            pusher_cv[item].wait(lock, [&] { return push_pending[item]; });
            push_pending[item]--;
            table[item]++;
            std::println("pusher puts {}", item);
            int on_table =
                std::ranges::count_if(table, [](int x) { return x > 0; });
            if (on_table == 2) {
                // more tricks
                notif = 3;
                for (int i = 0; i < 3; i++)
                    if (table[i]) notif -= i;
            }
        }

        if (notif != -1) {
            smoker_wake_cv[notif].notify_one();
            std::println("pusher {} notified {}", item, notif);
        } else {
            std::println("pusher {} did not notify", item);
        }
    }
}

void smoker(int have) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            smoker_wake_cv[have].wait(lock, [&] {
                return std::ranges::all_of(
                    table | std::views::enumerate, [&](auto pack) {
                        auto [index, value] = pack;
                        return value > 0 || index == have;
                    });
            });

            table.fill(0);
            agent_can_place = true;
            std::println("Smoker {} smokes", have);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        agent_cv.notify_one();
    }
}

int main() {
    std::thread ag(agent);
    // you CANNOT have a thread that's joinable going out of scope, dtor
    // will terminate()
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) threads.emplace_back(pusher, i);
    for (int i = 0; i < 3; i++) threads.emplace_back(smoker, i);
    ag.join();  // forever wait

    return 0;
}