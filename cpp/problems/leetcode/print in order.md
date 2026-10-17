# Print in order

You have 3 threads that you don't own, each will be called once in unknown order. Ensure print oder.

```cpp
class Foo {
public:
    int turn = 0;
    std::condition_variable cv;
    std::mutex mtx;
    Foo() {}

    void first(function<void()> printFirst) {
        {
            std::unique_lock lock(mtx);
            cv.wait(lock, [&] { return turn == 0; });
            // printFirst() outputs "first". Do not change or remove this line.
            printFirst();
            turn++;
        }
        cv.notify_all();
    }

    void second(function<void()> printSecond) {
        {
            std::unique_lock lock(mtx);
            cv.wait(lock, [&] { return turn == 1; });
            printSecond();
            turn++;
        }
        cv.notify_all();
    }

    void third(function<void()> printThird) {
        {
            std::unique_lock lock(mtx);
            cv.wait(lock, [&] { return turn == 2; });
            printThird();
            turn++;
        }
        cv.notify_all();
    }
};
```

Note

- first does not need that cv wait and check.
- a notify once WILL not work obviously, I made this mistake initially.

Some possible questions ( llm generated )

- What is wrong with such a check?

```cpp
if (turn != 1) {
    cv.wait(lock);
}
```

Spurious wakeups is the subtle one but even in this I'm doing a notify all so that clearly won't work.

a while instead of if here ( under a lock ) would be correct here.

- Why release mutex before calling `notify_all`? What happens if we do it while holding?

**incorrect**: It checks condition and sees lock and goes back to sleep. Can only be salvages by some spurious wakeup but it won't
deadlock forever.

A wakeup MUST check condition so it waits till lock is acquired. All it does is just an improvement as awakened threads
won't immediately contend for a mutex that's locked.

- Why is `std::unique_lock` used instead of `std::lock_guard` with condition variables?

A lock would need to be locked unlocked multiple times. A lock guard does not support that.

## Can it be solved with ...

### Mutex only?

You make a spin lock.

```cpp
class Foo {
public:
    int turn = 0;
    std::mutex mtx;

    void first(function<void()> printFirst) {
        run(0, printFirst);
    }

    void second(function<void()> printSecond) {
        run(1, printSecond);
    }

    void third(function<void()> printThird) {
        run(2, printThird);
    }

private:
    void run(int expected, function<void()> print) {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(mtx);

                if (turn == expected) {
                    print();
                    ++turn;
                    return;
                }
            }

            std::this_thread::yield(); // hint that another thread can run
        }
    }
};
```

### With semaphores

Note that one way it to use semaphores is to use it as a global lock. That is trivial but is same as the mutex only solution.

First needs no signalling, so just make one sema for 2 and 3.

Then this becomes quite shorter

```cpp
class Foo {
public:
    std::binary_semaphore go2{false}, go3{false};
    Foo() {}

    void first(function<void()> printFirst) {
        printFirst();
        go2.release();
    }

    void second(function<void()> printSecond) {
        go2.acquire();
        printSecond();
        go3.release();
    }

    void third(function<void()> printThird) {
        go3.acquire();
        printThird();
    }
};
```

### With atomic spinlock

```cpp
class Foo {
    std::atomic<int> turn{0};

public:
    void first(function<void()> printFirst) {
        printFirst();
        turn.store(1);
    }

    void second(function<void()> printSecond) {
        while (turn.load() != 1) {
            // busy wait
        }

        printSecond();
        turn.store(2);
    }

    void third(function<void()> printThird) {
        while (turn.load() != 2) {
            // busy wait
        }

        printThird();
    }
};
```

### I don't want EVERY thread to wake and check, can we be more targeted?

You can use two cvs as well. Similar idea as the semaphore one, just signal the appropriate
thread.
