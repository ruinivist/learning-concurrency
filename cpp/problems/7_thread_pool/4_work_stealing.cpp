#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class ThreadPool {
   private:
    using Task = std::move_only_function<void()>;

    struct WorkerState {
        std::mutex local_mutex_;
        std::thread thread_;
        std::deque<Task> local_tasks_;
        ThreadPool* owner_;
    };

    inline static thread_local WorkerState* current_worker_ = nullptr;

    std::vector<WorkerState> threads_;
    std::deque<Task> global_tasks_;
    std::mutex global_mtx_;
    bool stopping_ = false;
    std::condition_variable stop_or_task_cv_;
    std::atomic<std::size_t> all_queued_tasks_{0};

    void worker(std::size_t index) {
        auto& wk = threads_[index];
        current_worker_ = &wk;
        // preference is local -> steal -> global
        while (true) {
            std::optional<Task> task;
            // I still need to lock mutex even for my local queue to handle
            // stealers
            // 1. check own
            {
                std::lock_guard<std::mutex> lock(wk.local_mutex_);
                if (!wk.local_tasks_.empty()) {
                    task = std::move(wk.local_tasks_.back());
                    wk.local_tasks_.pop_back();
                    --all_queued_tasks_;
                    // all tasks has changed but we don't need to notify as a cv
                    // would only continue when it INCREASES from 0 or not block
                    // at all
                }
            }
            // 2. steal
            if (!task) {
                task = try_steal(index);
            }
            // 3. global
            if (!task) {
                std::unique_lock<std::mutex> lock(global_mtx_);
                if (!global_tasks_.empty()) {
                    task = std::move(global_tasks_.front());
                    global_tasks_.pop_front();
                    --all_queued_tasks_;
                } else {
                    // 4. now we wait
                    // this would look cleaner if I unlcoekd the global mutex
                    // and made an outer level if but redundant unlock and lock
                    // in that case
                    stop_or_task_cv_.wait(lock, [&] {
                        return stopping_ || all_queued_tasks_ > 0;
                    });

                    // rem => exist only when no task
                    if (stopping_ && all_queued_tasks_ == 0) {
                        return;
                    }

                    continue;
                }
            }
            // ^ this structure above is a bit ugly rn

            // HAS to be defined at this point due to the cv
            task.value()();
        }
    }

    std::optional<Task> try_steal(std::size_t own_index) {
        bool all_checked = false;
        Task task;
        while (!all_checked) {
            all_checked = true;
            for (std::size_t i = 0; i < threads_.size(); i++) {
                if (i == own_index) continue;
                // obviously the scope inside a while loop is only for THAT
                // iteration
                auto& wk = threads_[i];
                std::unique_lock<std::mutex> lock(wk.local_mutex_,
                                                  std::try_to_lock);

                if (!lock.owns_lock()) {
                    all_checked = false;
                    continue;
                }

                if (!wk.local_tasks_.empty()) {
                    task = std::move(wk.local_tasks_.front());
                    wk.local_tasks_.pop_front();
                    all_queued_tasks_--;
                    return task;
                }
            }
        };

        return std::nullopt;
    }

    // internal wrapper, handles stuff post convertion to no arg void tasks
    void submit_(Task task) {
        {
            std::lock_guard<std::mutex> lock(global_mtx_);
            if (stopping_)
                throw std::runtime_error("can't queue when stopping");

            // rem there is no race on reading this or worker state pointer
            bool is_local_task =
                current_worker_ && current_worker_->owner_ == this;
            if (is_local_task) {
                auto& wk = *current_worker_;
                std::lock_guard<std::mutex> lock(wk.local_mutex_);
                wk.local_tasks_.emplace_back(std::move(task));
                ++all_queued_tasks_;
            } else {
                global_tasks_.emplace_back(std::move(task));
                ++all_queued_tasks_;
            }
            // why increment tasks under mutex?
            // worker                              submitter
            // ------                              ---------
            // locks global_mtx_
            // checks predicate: all_tasks_ == 0
            //                                     ++all_tasks_
            //                                     notify_one()
            //                                     // worker isn't waiting yet
            // calls wait()
            // unlocks mutex and sleeps forever
            // even with atomics treat them as regular ints, the "atomic" is
            // only useful if you use them under different mutexes
        }
        stop_or_task_cv_.notify_one();
    }

   public:
    explicit ThreadPool(std::size_t thread_count) : threads_{thread_count} {
        assert(thread_count > 0);
        // I was getting an error in reserve because reserver needs the
        // elements to be movable in case it needs to move them in memory
        // std::mutex is not movable hence the element as a whole is not
        // movable
        // threads_.reserve(thread_count);
        // an embplace back too would have the same problem
        for (std::size_t i = 0; i < threads_.size(); i++) {
            threads_[i].owner_ = this;
            threads_[i].thread_ = std::thread(&ThreadPool::worker, this, i);
        }
    };

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(global_mtx_);
            stopping_ = true;
            // do I notify while holding the lock or AFTER? AFTER
        }
        stop_or_task_cv_.notify_all();
        for (auto& t : threads_) t.thread_.join();
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
        ThreadPool pool{3};

        auto parent = pool.submit([&pool] {
            auto child = pool.submit([] { return 21; });
            return child.get() * 2;
        });
        assert(parent.get() == 42);
    }
}
