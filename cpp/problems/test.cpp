/*
pinger and ponger 2 thread

2 funcs
a shared count read under lock
cv to notify when to ping or pong
*/

#include <condition_variable>
#include <mutex>
#include <print>

std::mutex mtx;
std::condition_variable cv;
enum turn_t { PINGER, PONGER };
turn_t turn = PINGER;
int count = 0;
const int STOP_AT = 10;

void pinger() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock);
            cv.wait(lock, [] { return turn == PINGER || count == STOP_AT; });
            if (count == 10) {
                cv.notify_one();
                break;
            }
            // only pong increases counts
            std::println("ping {}", count);
            turn = turn == PINGER ? PONGER : PINGER;
        }
        // release lock and then notify
        cv.notify_one();
    }
}

void ponger() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] { return turn == PONGER || count == STOP_AT; });
            if (count == 10) {
                cv.notify_one();
                break;
            }
            std::println("pong {}", count++);
            turn = turn == PINGER ? PONGER : PINGER;
        }
        // release lock and then notify
        cv.notify_one();
    }
}

int main() {
    std::thread pinger_thread(pinger), ponger_thread(ponger);
    pinger_thread.join();
    ponger_thread.join();

    return 0;
}
