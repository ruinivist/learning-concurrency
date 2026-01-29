/*
Reader writer lock (manual impl)

There's a museum and V visitors and C curators who
visit randomly. Visistors can go together but since
curators are in charge of renovation. Only when all visitors
are out can curator go in and no one else is allowed at the time.

Simulate with threads.

Uses: mutex, cv
*/

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <random>
#include <thread>

// readers read concurrencly, writers lock exclusively
template <typename T>
class ReaderWriteLock {
   private:
    T data_;
    int readers_ = 0;
    bool writing_ = false;  // bool as only 1 will be there

    std::mutex mtx_;
    std::condition_variable cv_readers_, cv_writers_;

    using read_func = std::function<void(const T&)>;

   public:
    ReaderWriteLock(T data) : data_(data) {}

    void read(read_func fn) {
        // reading state update
        {
            std::unique_lock<std::mutex> lck(mtx_);
            cv_readers_.wait(lck, [&]() { return !writing_; });
            readers_++;
        }

        fn(data_);

        // cleanup
        {
            std::unique_lock<std::mutex> lck(mtx_);
            readers_--;
            if (readers_ == 0) {
                cv_writers_.notify_one();
            }
        }
    }

    void write(T val) {
        std::unique_lock<std::mutex> lck(mtx_);
        cv_writers_.wait(lck, [&]() { return readers_ == 0 && !writing_; });
        writing_ = true;

        data_ = std::move(val);

        writing_ = false;
        cv_readers_.notify_all();
        cv_writers_.notify_one();
    }
};

std::vector<std::string> words = {"hello", "and", "goodbye"};

int random_int(int low, int high) {
    thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(low, high);
    return dist(gen);
}

ReaderWriteLock<std::string> rw_lock("");

void visitor(int visitor_index) {
    // jitter
    std::this_thread::sleep_for(std::chrono::milliseconds(random_int(0, 100)));
    rw_lock.read([&](std::string read) {
        std::printf("%d visitor read %s\n", visitor_index + 1, read.c_str());
    });
}

void writer(int writer_index) {
    // jitter
    std::this_thread::sleep_for(std::chrono::milliseconds(random_int(0, 100)));
    int write_index = random_int(0, words.size() - 1);
    std::printf("%d curator is writing %s\n", writer_index + 1,
                words[write_index].c_str());
    rw_lock.write(words[write_index]);
}

int main() {
    const int V = 10, C = 2;

    std::vector<std::thread> visitors, curators;
    visitors.reserve(V);
    curators.reserve(C);
    for (int i = 0; i < V; i++) {
        visitors.emplace_back(visitor, i);
    }

    for (int i = 0; i < C; i++) {
        curators.emplace_back(writer, i);
    }

    for (auto& t : visitors) t.join();
    for (auto& t : curators) t.join();
}