# Essentials of concurrency in maybe an hour

## cpp syntax

### mutex (raii)

```cpp
#include<mutex>

std::mutex mtx;

std::unique_lock lck(mtx); // with cv as can relock
std::lock_guard lock(mtx); // for one off raii locking, slighly lighter
```

### cv

```cpp
#include<condition_variable>

std::condition_variable cv;

cv.wait(lock, cond)
```

> note: it's not `wait_for`, that is time based waiting

- Naming? Do no use role, instead the condition being waiter for.
- avoid side effects in the wait condition, as it can be spuriously check. **ensure** code works under spurious wake up of the cv.
- modify all the state needed only then notify outside lock

### semas

```cpp
#include<semaphore>

// 5 = max count, 3 = initial
std::counting_semaphore<5> sem(3);

sem.acquire() // -1
sem.release() // +1
```

The dual "free" to "used" move and back is the most important.

### How to do 2 way signalling with semas?

```cpp
// Thread A
doA();
aSignal.release();
bSignal.acquire();

// Thread B
doB();
bSignal.release();
aSignal.acquire();
```

### atomics

```cpp
#include<atomic>
```

- `relaxed`: only need the atomic value / operation, no other state synced.
- `release`: publish earlier state changes.
- `acquire`: see those changes when observing their release.
- default: `seq_cst`.

Acquire/release do not themselves lock. The state being synced often needn't be atomic.

**counter**

```cpp
std::atomic<int> completed{0};
completed.fetch_add(1, std::memory_order_relaxed);
int count = completed.load(std::memory_order_relaxed);
```

**publish state**

```cpp
int data;
std::atomic<bool> ready{false};

// producer
data = 42;
ready.store(true, std::memory_order_release);

// consumer
while (!ready.load(std::memory_order_acquire)) {}
use(data);
```

**CAS** for conditional updates. Using atomics as a lock = spinklock.

```cpp
std::atomic<bool> busy{false};
int balance = 100; // all access must use this lock

void deposit(int amount) {
    bool expected = false;
    while (!busy.compare_exchange_weak(
        expected, true,
        std::memory_order_acquire,    // success: see previous holder's writes
        std::memory_order_relaxed)) { // failure: no access to balance
        expected = false;
    }

    balance += amount;

    busy.store(false, std::memory_order_release); // publish for next holder
}
```

Reset expected after failure, else a true -> true CAS could succeed and enter incorrectly.
This builds a spinlock; a mutex is usually preferable in application code.
