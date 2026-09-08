#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <mutex>
#include <print>
#include <random>
#include <semaphore>
#include <thread>
#include <vector>

// I can omit the max count here, in which case it's impl defined
using sema = std::counting_semaphore<>;
std::mutex mtx;
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
        // agent can just keep on looping independent of what anyone else is
        // doing
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        int exclude = rand_int();
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
            // pusher modifies the table under a mutex
            std::unique_lock<std::mutex> lock(mtx);
            table[item]++;
            std::println("pusher puts {}", item);
            int on_table =
                std::ranges::count_if(table, [](int x) { return x > 0; });
            // as soon as the count becomes 2, and it'll be so all ops are
            // serialised under a mutex so FOR SOME pusher it'll be 2
            // my point being no pusher seems an on_table count jump by MORE
            // than 1
            if (on_table == 2) {
                notif = 3;
                for (int i = 0; i < 3; i++) {
                    if (table[i]) {
                        notif -= i;
                        table[i]--;
                    }
                }
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
        // at THIS stage, can it happen that I consume so that vals go
        // negative? if I do not check for table values being > 0 when I
        // consume
        // YES! because if two pushes push say a,b and assume we are in a
        // state where agent ingredients are in abundance, BOTH will issue a
        // ticket to smokers, previously this would've just been ONE ticket
        // issued only by the 2nd pusher
        // I don't think I can tie break based on WHICH pusher is allowed to
        // issue a ticket unless I modify agent code to make it so that only
        // the larger ingredient one will notify
        // BIG NOTE !!! => THIS ^ ABOVE IS WRONG as well
        // - (0,2) and 2 runs first does not notify as not enough
        // ingredients even if notify = true was passed from agent
        // - there wold also be reaces based on "do_i_notify" for a fast
        // agent case as say multiple pusher work got accumulated and I need
        // to have that same order of do I notify

        // letting pushers reserve too is the ONLY way, smokers do not update
        // counts and can be lockless
        std::println("Smoker {} smokes", have);
        std::this_thread::sleep_for(std::chrono::seconds(1));
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