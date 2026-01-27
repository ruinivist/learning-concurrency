#include <mutex>

std::once_flag isInit;

void init() {
    // some hard init
}

void worker() {
    // read as "ensure"
    std::call_once(isInit, init);
}