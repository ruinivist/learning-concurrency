# Fizzbuzz multithreaded

This is a problem where cvs shine. If you notify all and let the cvs handle
the complex conditions then the problem is really triival.

I intially wrote this monstrosity

```cpp
class FizzBuzz {
private:
    int n;
    int cur = 1;
    mutex mtx;
    condition_variable cv;
    bool stop = false;

public:
    FizzBuzz(int n) { this->n = n; }

    // printFizz() outputs "fizz".
    void fizz(function<void()> printFizz) {
        while (!stop) {
            {
                unique_lock lock(mtx);
                cv.wait(lock,
                        [&] { return cur % 3 == 0 && cur % 5 != 0 || stop; });
                if (stop) {
                    return;
                }
                cur++;
                printFizz();
                if (cur > n) {
                    stop = true;
                }
            }
            cv.notify_all();
        }
    }

    // printBuzz() outputs "buzz".
    void buzz(function<void()> printBuzz) {
        while (!stop) {
            {
                unique_lock lock(mtx);
                cv.wait(lock,
                        [&] { return cur % 3 != 0 && cur % 5 == 0 || stop; });
                if (stop) {
                    return;
                }
                cur++;
                printBuzz();
                if (cur > n) {
                    stop = true;
                }
            }
            cv.notify_all();
        }
    }

    // printFizzBuzz() outputs "fizzbuzz".
    void fizzbuzz(function<void()> printFizzBuzz) {
        while (!stop) {
            {
                unique_lock lock(mtx);
                cv.wait(lock,
                        [&] { return cur % 3 == 0 && cur % 5 == 0 || stop; });
                if (stop) {
                    return;
                }
                cur++;
                printFizzBuzz();
                if (cur > n) {
                    stop = true;
                }
            }
            cv.notify_all();
        }
    }

    // printNumber(x) outputs "x", where x is an integer.
    void number(function<void(int)> printNumber) {
        while (!stop) {
            {
                unique_lock lock(mtx);
                cv.wait(lock,
                        [&] { return cur % 3 != 0 && cur % 5 != 0 || stop; });
                if (stop) {
                    return;
                }
                printNumber(cur++);
                if (cur > n) {
                    stop = true;
                }
            }
            cv.notify_all();
        }
    }
};
```

Note that this passes and looks correct but there is a data race on stop.

| Thread A (`number`)               | Thread B (`fizz`)      |
| --------------------------------- | ---------------------- |
| Prints `2`, increments `cur` to 3 | Waiting                |
| Releases mutex                    | Acquires mutex         |
| Calls `notify_all()`              | Prints `fizz`          |
| Evaluates `while (!stop)`         | Executes `stop = true` |

Solution is to ofc use a while true and only read under the mutex.

## better impl

```cpp
class FizzBuzz {
private:
    int n;
    int cur = 1;

    mutex mtx;
    condition_variable cv;

    template <typename Pred, typename Func> void run(Pred pred, Func print) {
        while (true) {
            {
                unique_lock<mutex> lock(mtx);

                cv.wait(lock, [&] { return cur > n || pred(cur); });

                if (cur > n)
                    return;

                print(cur);
                ++cur;
            }
            cv.notify_all();
        }
    }

public:
    FizzBuzz(int n) : n(n) {}

    void fizz(function<void()> printFizz) {
        run([](int x) { return x % 3 == 0 && x % 5 != 0; },
            [&](int) { printFizz(); });
    }

    void buzz(function<void()> printBuzz) {
        run([](int x) { return x % 3 != 0 && x % 5 == 0; },
            [&](int) { printBuzz(); });
    }

    void fizzbuzz(function<void()> printFizzBuzz) {
        run([](int x) { return x % 15 == 0; }, [&](int) { printFizzBuzz(); });
    }

    void number(function<void(int)> printNumber) {
        run([](int x) { return x % 3 != 0 && x % 5 != 0; }, printNumber);
    }
};
```
