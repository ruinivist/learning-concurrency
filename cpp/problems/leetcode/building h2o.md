# Building h2o

> this is also a problem in little book of semas.

## The ideal solution

is to let oxygen thread be the driver as only one is needed.

If I get an "O" first, I wait for 2 H's. If instead I get an H release an H signal,
same as the empty-full signalling common to semas.

O thread waits for 2 such signals. Note that this is not symmetric, O acts as the driver here.

```cpp
class H2O {
public:
    binary_semaphore o{1};
    counting_semaphore<2> h{2}, hdone{0};

    H2O() {}

    void hydrogen(function<void()> releaseHydrogen) {
        h.acquire();
        releaseHydrogen();
        hdone.release();
    }

    void oxygen(function<void()> releaseOxygen) {
        o.acquire();

        hdone.acquire();
        hdone.acquire();

        releaseOxygen();

        h.release(2);
        o.release();
    }
};
```

## Two way sync

I was initially trying this, to do something more along the lines of the usual
two way sync aka reinventing barrier with semas.

Note that this is a "stricter" solution but in here ALL three arrive and then run
at once. Previous one has no restriction of all 3 running together.

But then this is a true 2 way sync.

```cpp
class H2O {
public:
    binary_semaphore o{1};
    counting_semaphore<2> h{2};

    counting_semaphore<2> hsig{0};
    counting_semaphore<2> osig{0};
    counting_semaphore<2> hdone{0};

    void hydrogen(function<void()> releaseHydrogen) {
        h.acquire();

        hsig.release();      // I've arrived
        osig.acquire();      // Wait for oxygen

        releaseHydrogen();
        hdone.release();     // I've finished
    }

    void oxygen(function<void()> releaseOxygen) {
        o.acquire();

        hsig.acquire();      // Wait for H1
        hsig.acquire();      // Wait for H2

        osig.release(2);     // Let both H proceed
        releaseOxygen();

        hdone.acquire();     // Wait for H1 to finish
        hdone.acquire();     // Wait for H2 to finish

        h.release(2);
        o.release();
    }
};
```

## Barrier

Since the problem is essentially a barrier problem.

```cpp
#include <barrier>
#include <semaphore>

class H2O {
public:
    counting_semaphore<2> h{2};
    binary_semaphore o{1};
    barrier sync{3};

    void hydrogen(function<void()> releaseHydrogen) {
        h.acquire();

        releaseHydrogen();

        sync.arrive_and_wait();

        h.release();
    }

    void oxygen(function<void()> releaseOxygen) {
        o.acquire();

        releaseOxygen();

        sync.arrive_and_wait();

        o.release();
    }
};
```

## Mutex cvs? Atomics?

This becomes more involved as you have to account for generations aka the fast
runner problem ( this would be aproblem with ANY resuable cv based barrier ).

An atomics solution also suffers from the generation issue.
