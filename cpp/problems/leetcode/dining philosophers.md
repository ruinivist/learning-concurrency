# Dining philosophers

Rather refer to my detailed writeup on the problem.

```cpp
class DiningPhilosophers {
    mutex forks[5];

public:
    DiningPhilosophers() {}

    void wantsToEat(int philosopher, function<void()> pickLeftFork,
                    function<void()> pickRightFork, function<void()> eat,
                    function<void()> putLeftFork,
                    function<void()> putRightFork) {

        int left = philosopher;
        int right = (philosopher + 1) % 5;

        auto &first = forks[min(left, right)],
             &second = forks[max(left, right)];

        first.lock();
        second.lock();

        pickLeftFork();
        pickRightFork();
        eat();
        putRightFork();
        putLeftFork();

        second.unlock();
        first.unlock();
    }
};
```
