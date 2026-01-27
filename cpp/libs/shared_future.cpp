#include <future>

int calc(std::shared_future<int> sf, int val) { return sf.get() + val; }

int main() {
    std::promise<int> p;
    std::shared_future<int> f = p.get_future().share();

    // can be copied to threads

    p.set_value(10);  // broadcast to all threads
}