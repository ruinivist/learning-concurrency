#include <chrono>
#include <cstddef>
#include <functional>
#include <print>
#include <stop_token>
#include <thread>
#include <vector>

class ThreadPool {
   private:
    using Task = std::function<void()>;
    // this order below matters so and this is what makes the dtor to be
    // not needed here, fine for this here but in general such ordering
    // is fragile and I would rather use a dtor
    std::vector<Task> tasks_;
    std::stop_source global_stop_;
    std::vector<std::jthread> threads_;
    // ^ threads -> global stop -> tasks ( destroyed in this order )
    // the jthreads dtor will do an equivalent of stop all

    void worker(std::stop_token local_stop, std::stop_token global_stop,
                int index) {
        while (true) {
            if (local_stop.stop_requested()) {
                std::println("Worker {} local stop requested", index);
                return;
            }
            if (global_stop.stop_requested()) {
                std::println("Worker {} global stop requested", index);
                return;
            }

            tasks_[index]();
        }
    }

   public:
    const std::size_t size;
    ThreadPool(std::vector<std::function<void()>> tasks)
        : tasks_(tasks), size(tasks.size()) {
        threads_.reserve(size);
        for (std::size_t i = 0; i < size; i++) {
            // jthread does not correctly resolve if you pass in stop token and
            // use the pointer to member function syntax so this
            threads_.emplace_back(std::bind_front(&ThreadPool::worker, this),
                                  global_stop_.get_token(), i);
        }
    };

    void stop_one(int index) {
        threads_[index].request_stop();
        threads_[index].join();
    }

    void stop_all() {
        global_stop_.request_stop();
        for (auto& t : threads_) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    // do I need a dtor?
    // jthread will request stop on EACH of the individual threads but print
    // would be a local stop requested
};

int main() {
    // both are separate, you don't need atomics at all
    int ctr1 = 0, ctr2 = 0;
    ThreadPool tp(
        {[&ctr1] {
             ctr1++;
             std::this_thread::sleep_for(std::chrono::milliseconds(250));
         },
         [&ctr2] {
             ctr2++;
             std::this_thread::sleep_for(std::chrono::milliseconds(250));
         }});

    std::this_thread::sleep_for(std::chrono::seconds(1));
    // the stops do a join instead otherwise the data may still be changing
    // while we read it, as stop is only requested, not completed
    tp.stop_one(0);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    tp.stop_all();
    std::println("Counters are {} and {}", ctr1, ctr2);
}
