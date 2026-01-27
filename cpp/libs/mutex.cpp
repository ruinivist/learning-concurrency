#include <mutex>

std::mutex mtx;
int ctr = 0;

void critical_section() {
    mtx.lock();
    ctr++;
    mtx.unlock();
}

int main() {
    critical_section();  // Example usage
    return 0;
}