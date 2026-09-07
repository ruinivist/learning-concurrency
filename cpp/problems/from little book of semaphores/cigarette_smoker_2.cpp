/*
each item remembers the generation in which the agent placed it
a smoker observes one item, then waits for the other item from the SAME
generation before consuming anything
*/

#include <array>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <random>
#include <thread>
#include <vector>

std::mutex mtx;
std::array<std::condition_variable, 3> item_cvs;
std::condition_variable agent_places_cv;
std::array<int, 3> item_generation{};
int generation = 0;
bool agent_can_place = true;

int rand_int() {
    std::mt19937 mt{std::random_device{}()};
    std::uniform_int_distribution<int> gen(0, 2);
    return gen(mt);
}

void agent() {
    while (true) {
        int exclude = rand_int();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        {
            std::unique_lock<std::mutex> lock(mtx);
            agent_places_cv.wait(lock, [] { return agent_can_place; });

            agent_can_place = false;
            generation++;

            for (int i = 0; i < 3; i++) {
                if (i != exclude) {
                    item_generation[i] = generation;
                    std::println("Agent places {} in generation {}", i,
                                 generation);
                }
            }
        }

        for (int i = 0; i < 3; i++) {
            if (i != exclude) item_cvs[i].notify_all();
        }
    }
}

void smoker(int have) {
    // the two items this smoker needs, in index order
    int item_a = have == 0 ? 1 : 0;
    int item_b = have == 2 ? 1 : 2;
    int seen_generation = 0;

    while (true) {
        int candidate_generation;

        {
            std::unique_lock<std::mutex> lock(mtx);

            item_cvs[item_a].wait(lock, [&] {
                return item_generation[item_a] > seen_generation;
            });
            candidate_generation = item_generation[item_a];

            item_cvs[item_b].wait(lock, [&] {
                return item_generation[item_b] >= candidate_generation;
            });

            seen_generation = candidate_generation;
            if (item_generation[item_b] != candidate_generation) continue;

            // neither item was consumed until both generations matched
            assert(candidate_generation == generation);
            assert(!agent_can_place);
            agent_can_place = true;
        }

        std::println("Smoker {} smokes generation {}", have,
                     candidate_generation);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        agent_places_cv.notify_one();
    }
}

int main() {
    std::thread ag(agent);
    std::vector<std::thread> smokers;
    for (int i = 0; i < 3; i++) smokers.emplace_back(smoker, i);

    ag.join();
}
