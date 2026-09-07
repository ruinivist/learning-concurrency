/*
so I need 4 threads
one will be the agent
then 3 smokers
let's number those ingredients as ints
0, 1 and 2
so missing one is naturaly 3 - (sum of what I have)
*/

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
std::array<int, 3> table{};  // {} will init all to 0
std::condition_variable smokers_picks_cv, agent_places_cv;

int rand_int() {
    // only one thread is calling it, no need to make thread local
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
            agent_places_cv.wait(lock, [&] {
                return std::ranges::all_of(
                    table, [](int item_cnt) { return item_cnt == 0; });
            });
            for (int i = 0; i < 3; i++) {
                if (i != exclude) {
                    std::println("Agent places {}", i);
                    table[i]++;
                }
            }
        }

        // signal everyone
        smokers_picks_cv.notify_all();
    }
}

void smoker(int have) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            smokers_picks_cv.wait(lock, [&] {
                return std::ranges::all_of(
                    table | std::views::enumerate, [&](auto pack) {
                        auto [index, value] = pack;
                        return value > 0 || index == have;
                    });
            });

            table.fill(0);
            std::println("Smoker {} smokes", have);
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        agent_places_cv.notify_one();
    }
}

int main() {
    std::thread ag(agent);
    // you CANNOT have a thread that's joinable going out of scope, dtor will
    // terminate()
    std::vector<std::thread> smokers;
    for (int i = 0; i < 3; i++) smokers.emplace_back(smoker, i);
    ag.join();  // forever wait

    return 0;
}