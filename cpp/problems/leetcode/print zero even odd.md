# Print zero even odd

Again trivial if you make each a sema state.

```cpp
class ZeroEvenOdd {
private:
    int n;
    binary_semaphore zeroSem{1}, evenSem{0}, oddSem{0};

public:
    ZeroEvenOdd(int n) : n(n) {}

    void zero(function<void(int)> printNumber) {
        for (int i = 1; i <= n; i++) {
            zeroSem.acquire();
            printNumber(0);

            if (i & 1) {
                oddSem.release();
            } else {
                evenSem.release();
            }
        }
    }

    void even(function<void(int)> printNumber) {
        for (int i = 2; i <= n; i += 2) {
            evenSem.acquire();
            printNumber(i);
            zeroSem.release();
        }
    }

    void odd(function<void(int)> printNumber) {
        for (int i = 1; i <= n; i += 2) {
            oddSem.acquire();
            printNumber(i);
            zeroSem.release();
        }
    }
};
```
