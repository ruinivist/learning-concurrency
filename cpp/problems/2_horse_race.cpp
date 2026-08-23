/*
3 Horses in 3 threads
each updates it's position
when they all have make a move, update screen

use: barrier
*/

#include <array>
#include <barrier>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

void clear() { std::system("clear"); }
void wait_second() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

const int horses = 3;
const int finish = 25;
std::array<int, horses> positions;

inline int winner() {
    for (int i = 0; i < horses; i++)
        if (positions[i] == finish) return i;
    return -1;
}

void print_track() {
    clear();

    for (int i = 0; i < horses; i++) {
        auto line = std::string(positions[i], '=');
        std::printf("Horse %2d : %2d : %s\n", i + 1, positions[i],
                    line.c_str());
    }

    int winner_index = winner();
    if (winner_index != -1) {
        std::cout << "Horse " << winner_index + 1 << " won!\n";
    } else {
        wait_second();
    }
}

std::mutex mtx;
std::barrier barrier(horses, print_track);

void horse_jumps(int horse_index) {
    while (winner() == -1) {
        {
            std::unique_lock<std::mutex> lck;
            int cur_pos = positions[horse_index];
            int delta = rand() % 4 + 1;
            positions[horse_index] = std::min(finish, cur_pos + delta);
        }
        barrier.arrive_and_wait();
    }

    // arrive and not wait, needed as if one reaches finish
    // other will stay waiting
    barrier.arrive_and_drop();
}

int main() {
    std::array<std::thread, horses> threads;
    for (int i = 0; i < horses; i++) {
        threads[i] = std::thread(horse_jumps, i);
    }

    for (auto& thread : threads) thread.join();
}