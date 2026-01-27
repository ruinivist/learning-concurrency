#include <latch>

std::latch wait_for(3);

void worker() {
    // phase 1 of work

    // arrive and wait is same as this
    // wait_for.count_down(); count down
    // wait_for.wait(); wait

    wait_for.arrive_and_wait();

    // phase 2 of work
}