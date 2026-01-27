#include <future>

int calc(int x) { return x * x; }

int main() {
    std::future<int> fut = std::async(calc, 10);
    // or
    std::future<int> fut2 = std::async(std::launch::deferred, calc, 10);
    // std::launch::async or std::launch::deferred
    // default is both

    // deferred runs on get, async is immediate on a new thread
    int result = fut.get();  // waits
}