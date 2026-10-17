# Print foo bar alternately

I've done most of the analysis in print in order. This just applies those again.

```cpp
class FooBar {
private:
    int n;
    bool fooTurn = true;
    mutex mtx;
    condition_variable fooTurnCv, barTurnCv;

public:
    FooBar(int n) { this->n = n; }

    void foo(function<void()> printFoo) {

        for (int i = 0; i < n; i++) {
            {
                unique_lock lock(mtx);
                fooTurnCv.wait(lock, [&] { return fooTurn; });
                printFoo();
                fooTurn = !fooTurn;
            }
            barTurnCv.notify_one();
        }
    }

    void bar(function<void()> printBar) {

        for (int i = 0; i < n; i++) {
            {
                unique_lock lock(mtx);
                barTurnCv.wait(lock, [&] { return !fooTurn; });
                printBar();
                fooTurn = !fooTurn;
            }
            fooTurnCv.notify_one();
        }
    }
};
```

- Do you need turn? Why not just notify?

Yes, cvs must be guarded by states to avoid spurious wake issues.

## Sema impl

Once again, for such cases where the condition for sync is non-trivial, semas work quite well.

```cpp
class FooBar {
private:
    int n;
    binary_semaphore fooSem{1}, barSem{0};

public:
    FooBar(int n) { this->n = n; }

    void foo(function<void()> printFoo) {

        for (int i = 0; i < n; i++) {
            fooSem.acquire();
            printFoo();
            barSem.release();
        }
    }

    void bar(function<void()> printBar) {

        for (int i = 0; i < n; i++) {
            barSem.acquire();
            printBar();
            fooSem.release();
        }
    }
};
```
