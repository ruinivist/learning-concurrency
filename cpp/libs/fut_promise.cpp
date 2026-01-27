#include <future>
#include <thread>

void foo(std::promise<int> p) {
    try {
        p.set_value(42);
    } catch (...) {
        p.set_exception(std::current_exception());
    }
}

int main() {
    std::promise<int> p;
    std::future<int> f = p.get_future();

    std::thread t(foo, std::move(p));
}