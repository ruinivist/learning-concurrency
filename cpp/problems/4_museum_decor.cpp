#include <chrono>
#include <condition_variable>
#include <mutex>
#include <print>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

template <typename T>
class RWLock {
   private:
    T data_;
    std::mutex mtx_;
    std::condition_variable wait_to_read_, wait_to_write_;
    int readers_ = 0, writers_ = 0, waiting_writers_ = 0;

   public:
    RWLock() = default;
    RWLock(T data) : data_(std::move(data)) {}

    T read() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            // block even if waiting writer is there, write biased
            wait_to_read_.wait(lock, [&] {
                // continue when
                return !waiting_writers_ && !writers_;
            });

            readers_++;
        }

        T ret = data_;

        {
            std::unique_lock<std::mutex> lock(mtx_);
            if (--readers_ == 0) {
                // wake up ONE writer, only one can writer anyways
                wait_to_write_.notify_one();
            }
        }

        return ret;
    }

    void write(T data) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            waiting_writers_++;

            wait_to_write_.wait(lock,
                                [&] { return readers_ == 0 && writers_ == 0; });

            waiting_writers_--;
            writers_++;

            data_ = std::move(data);

            writers_--;  // a writer is ONLY 0 or 1 by design so 0 writers now
            if (waiting_writers_) {
                wait_to_write_.notify_one();
            } else {
                wait_to_read_.notify_all();
            }
        }
    }
};

// ========= usage wiring =================

int random_int(int low, int high) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(low, high);
    return dist(gen);
}

void reader(int id, RWLock<std::string>& rw) {
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(random_int(10, 50)));
        std::string val = rw.read();
        std::println("Reader {} read: {}", id, val);
    }
}

void writer(int id, RWLock<std::string>& rw) {
    const std::vector<std::string> words = {"alpha", "beta", "gamma", "delta"};
    for (int i = 0; i < 2; ++i) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(random_int(20, 60)));
        std::string word =
            words[random_int(0, words.size() - 1)] + "_W" + std::to_string(id);
        rw.write(word);
        std::println("Writer {} wrote: {}", id, word);
    }
}

int main() {
    RWLock<std::string> rw("init");
    const int NUM_READERS = 6;
    const int NUM_WRITERS = 2;

    std::vector<std::thread> threads;
    threads.reserve(NUM_READERS + NUM_WRITERS);

    for (int i = 1; i <= NUM_READERS; ++i) {
        threads.emplace_back(reader, i, std::ref(rw));
    }
    for (int i = 1; i <= NUM_WRITERS; ++i) {
        threads.emplace_back(writer, i, std::ref(rw));
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}