#include <cassert>
#include <chrono>
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
#include <vector>

class ThreadPool {
   private:
    using Task = std::move_only_function<void()>;
    std::vector<std::thread> threads_;
    std::deque<Task> tasks_;
    std::mutex mtx_;
    bool stopping_ = false;
    std::condition_variable stop_or_dq_task_cv_, stop_or_enq_task_cv_;

    void worker() {
        while (true) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                // I initially wrote
                // stopping_ && tasks_.empty() || !tasks_.empty();
                // a && !b || b is same as a || b
                // case on b to make it obvious
                stop_or_dq_task_cv_.wait(
                    lock, [&] { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) return;
                // otherwise you pick a task
                // dq so we pick them in order
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }

            stop_or_enq_task_cv_.notify_one();
            task();
        }
    }

    // internal wrapper, handles stuff post convertion to no arg void tasks
    void submit_(Task task) {
        // now we must block and continue either when stopping or we have space
        {
            std::unique_lock<std::mutex> lock(mtx_);
            // who notified this? a stop or a pop
            stop_or_enq_task_cv_.wait(
                lock, [&] { return stopping_ || tasks_.size() < capacity; });
            // for where some thread is blocked on a submit and we begin to stop
            if (stopping_) {
                throw std::runtime_error("can't queue when stopping");
            }
            tasks_.emplace_back(std::move(task));
        }
        // if all are working they'll pick it up some is done
        // otherwise I must wake up
        stop_or_dq_task_cv_.notify_one();
    }

   public:
    const std::size_t capacity;

    explicit ThreadPool(std::size_t thread_count, std::size_t capacity)
        : capacity(capacity) {
        assert(thread_count > 0);
        assert(capacity > 0);
        // we spawn n threads
        threads_.reserve(thread_count);
        for (std::size_t i = 0; i < thread_count; i++) {
            // pointer to member syntax, note how first is the
            // ptr to member and second is the obect
            threads_.emplace_back(&ThreadPool::worker, this);
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
        stop_or_dq_task_cv_.notify_all();
        stop_or_enq_task_cv_.notify_all();
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
        ThreadPool pool(1, 1);
        std::promise<void> worker_started, release_worker;
        auto release = release_worker.get_future().share();

        // queue up a blocking task
        auto running = pool.submit([&worker_started, release] {
            worker_started.set_value();
            release.wait();
        });
        // as worker started is done this must proceed immediately
        worker_started.get_future().wait();

        auto queued = pool.submit([] {});
        // this'll block as release is not completed yet
        auto blocked_submit = std::async(
            std::launch::async, [&pool] { return pool.submit([] {}); });

        assert(blocked_submit.wait_for(std::chrono::milliseconds(10)) ==
               std::future_status::timeout);

        // free the release
        release_worker.set_value();
        // wait on the older running one to complete and then the blocked one
        running.get();
        queued.get();
        // async returns one fut and then my pool returns anotehr
        blocked_submit.get().get();
    }
}
