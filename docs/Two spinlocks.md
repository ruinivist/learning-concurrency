# Two spinlocks

Spin"lock" is still very much a lock. Thread repeatedly check until lock becomes available.
Two way you can make it, one with mutex and another with atomics.

Just spinning is a plain wrong impl.

## With mutex

```cpp
#include <mutex>

class SpinLock {
private:
    std::mutex mtx;

public:
    void lock() {
        while (!mtx.try_lock()) {
            // Busy wait (spin)
        }
    }

    void unlock() {
        mtx.unlock();
    }
};
```

## With atomics

```cpp
#include <atomic>

class SpinLock {
private:
    std::atomic<bool> flag{false};

public:
    void lock() {
        while (true) {
            bool locked = false;

            if (flag.compare_exchange_weak(
                    locked,
                    true,
                    std::memory_order_acquire, // success ( modify acquire )
                    std::memory_order_relaxed)) { // failed ( read relaxed )
                break;
            }
        }
    }

    void unlock() {
        flag.store(false, std::memory_order_release);
    }
};
```

## Tradeoffs

Generally, mutexes internally use atomics + futex, so are a heavier wrapper on atomics basically,
aka more overhead. An atomic spinlock then is generally faster.
