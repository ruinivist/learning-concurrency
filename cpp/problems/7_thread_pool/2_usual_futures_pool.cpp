#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

template <std::size_t N>
class ThreadPool {
    static_assert(N > 0);

   private:
    using Task = std::move_only_function<void()>;
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

    // internal wrapper, handles stuff post convertion to no arg void tasks
    void submit_(Task task) {
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

    template <class F, class... Args>
    auto submit(F&& function, Args&&... arguments) {
        auto job = [stored_function = std::forward<F>(function),
                    ... stored_arguments = std::forward<Args>(
                        arguments)]() mutable -> decltype(auto) {
            return std::invoke(std::move(stored_function),
                               std::move(stored_arguments)...);
        };

        using Result = std::invoke_result_t<decltype(job)>;

        std::packaged_task<Result()> task(std::move(job));
        auto future = task.get_future();

        submit_(std::move(task));
        return future;
    }
};

int main() {
    {
        ThreadPool<3> pool;
        auto result = pool.submit([](int a) { return a * a; }, 10);
        assert(result.get() == 100);

        int completed = 0;
        auto void_result = pool.submit([&completed] { ++completed; });
        void_result.get();
        assert(completed == 1);

        auto failed = pool.submit(
            []() -> int { throw std::runtime_error("task failed"); });
        try {
            failed.get();
            assert(false);
        } catch (const std::runtime_error&) {
        }

        auto pointer = std::make_unique<int>(42);
        auto moved =
            pool.submit([value = std::move(pointer)] { return *value; });
        assert(moved.get() == 42);
        assert(!pointer);

        int value = 10;
        auto modified =
            pool.submit([](int& number) { ++number; }, std::ref(value));
        modified.get();
        assert(value == 11);

        const int constant = 7;
        auto read = pool.submit([](const int& number) { return number; },
                                std::cref(constant));
        assert(read.get() == 7);
    }
}
