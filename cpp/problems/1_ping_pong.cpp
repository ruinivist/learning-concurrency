/*
Two threads.
A prints ping
B prints pong
in alternation N times.

Use: mutex and condition_variable.
*/

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

std::mutex mtx;
std::condition_variable cv;
int turn = 0;
int N = 10;

void pinger() {
    while (true) {
        {
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait(lck, [] { return turn == 0 || N == 0; });

            if (N == 0) {
                cv.notify_one();  // REMEMBER
                break;
            }

            N--;

            std::cout << "ping\n";
            turn = !turn;
        }

        cv.notify_one();
    }
}

void ponger() {
    while (true) {
        {
            std::unique_lock<std::mutex> lck(mtx);
            cv.wait(lck, [] { return turn == 1 || N == 0; });

            if (N == 0) {
                cv.notify_one();
                break;
            }
            N--;

            std::cout << "pong\n";
            turn = !turn;
        }

        cv.notify_one();
    }
}

int main() {
    std::thread ping(pinger), pong(ponger);
    ping.join();
    pong.join();
}