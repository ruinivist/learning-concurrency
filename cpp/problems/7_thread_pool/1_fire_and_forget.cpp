#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>

template <std::size_t N>
class ThreadPool {
    static_assert(N > 0);

   private:
    using Task = std::function<void()>;
    std::array<std::thread, N> threads_;
    std::deque<Task> tasks_;
    std::mutex mtx_;
    bool stopping_ = false;
    std::condition_variable stop_or_task_cv_;

    void worker() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                // I initially wrote
                // stopping_ && tasks_.empty() || !tasks_.empty();
                // a && !b || b is same as a || b
                // case on b to make it obvious
                stop_or_task_cv_.wait(
                    lock, [&] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                // otherwise you pick a task
                // dq so we pick them in order
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }

            task();
        }
    }

   public:
    ThreadPool() {
        // we spawn n threads
        for (std::size_t i = 0; i < N; i++) {
            // pointer to member syntax, note how first is the
            // ptr to member and second is the obect
            threads_[i] = std::thread(&ThreadPool::worker, this);
        }
    };

    ~ThreadPool() {
        // you set stopping true, notify all and then wait on
        // an all join
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopping_ = true;
            // do I notify while holding the lock or AFTER? AFTER
        }
        stop_or_task_cv_.notify_all();
        // I added a second cv here on a cv wait for tasks empty
        // workers will naturally exit and join would succeed, you don't
        // need a cv
        for (auto& t : threads_) t.join();
    }

    void put(Task task) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (stopping_) {
                throw std::runtime_error("can't queue when stopping");
            }
            tasks_.emplace_back(std::move(task));
        }
        // if all are working they'll pick it up some is done
        // otherwise I must wake up
        stop_or_task_cv_.notify_one();
    }
};

int main() {
    std::atomic<int> completed = 0;
    {
        ThreadPool<3> pool;
        for (int i = 0; i < 10; ++i)
            pool.put(
                [&] { completed.fetch_add(1, std::memory_order_relaxed); });
    }
    assert(completed.load(std::memory_order_relaxed) == 10);
}
