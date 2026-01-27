#include <future>

int calc(int x) { return x * x; }

int main() {
    std::packaged_task<int(int)> task(calc);

    std::future<int> fut = task.get_future();

    std::thread t(std::move(task), 10);

    int result = fut.get();  // waits

    // calc runs immediatedly on new thread
    // waits on .get()
    t.join();  // still need join
}