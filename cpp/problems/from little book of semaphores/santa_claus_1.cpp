#include <barrier>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <semaphore>
#include <thread>

bool elves_ready = false, deer_ready = false;
std::mutex mtx;
std::condition_variable santa_wake;
std::counting_semaphore deers_helped{0}, elves_helped{0}, elf_help_req{3};

// you don't need an & for global scope, in fact, you can't
// put an & here as it's "non-local lambda"
std::barrier elf_sync{3, [] {
                          // u need a lock because there's santa thread that's
                          // using this as well
                          std::unique_lock<std::mutex> lock(mtx);
                          elves_ready = true;
                          santa_wake.notify_one();
                      }};
std::barrier elf_leave_sync{3};
std::barrier deer_sync{9, [] {
                           std::unique_lock<std::mutex> lock(mtx);
                           deer_ready = true;
                           santa_wake.notify_one();
                       }};
// this callback is executed by one of the "participating threads" ( so not main
// here ) and only ONE, so no need to worry of a mutex

void deer(int i) {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200 * i));
        std::println("deer {} arrived", i);
        deer_sync.arrive_and_wait();
        // all 9 ready, wait for santa's signal to continue
        deers_helped.acquire();
        std::println("deer {} got help", i);
    }
}

void elf(int i) {
    while (1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200 * i));
        std::println("elf {} arrived", i);
        // if more than 3 elves, they will be blocked here
        elf_help_req.acquire();
        elf_sync.arrive_and_wait();
        elves_helped.acquire();
        std::println("elf {} got help", i);
        elf_leave_sync.arrive_and_wait();
        // we don't release ANY of the help req till all 3 get help
        elf_help_req.release();
    }
}

void santa() {
    while (1) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            // for the case of both being true and this thread handling notifies
            // at the same time, I was thinking will it just check the condition
            // once and then sleep forever waiting for another notify this ^ is
            // not a problem as predicate is checked on a wait call and on
            // notifies so it loops again and hits wait, it WILL check cond
            // again before sleeping, that condition would be true again
            santa_wake.wait(lock, [&] { return elves_ready || deer_ready; });

            if (deer_ready) {
                std::println("helping deers");
                deers_helped.release(9);
                deer_ready = false;
            } else {
                std::println("helping elves");
                elves_helped.release(3);
                elves_ready = false;
            }
        }
    }
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 9; i++) threads.emplace_back(deer, i + 1);
    for (int i = 0; i < 5; i++) threads.emplace_back(elf, i + 1);
    threads.emplace_back(santa);
    threads.back().join();
}