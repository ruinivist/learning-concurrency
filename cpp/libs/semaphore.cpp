#include <semaphore>

// UPTO 5 ACCESS AT A TIME
// initial count 3 max count 5
std::counting_semaphore<5> sem(3);

void access_resource() {
    sem.acquire();  // decrease count
    // critical section
    sem.release();  // increase count
}