# Barrier

for a reusable barrier impl, the main problem to worry about is the fast runner
problem

```
thread 1 arrives and waits
thread 2 arrives and waits
barriers open
thread 2 notifies all, resets count and sets opn false
thread 1 arrive, open is false
```

So the problem is that by the time the thread 1 is woken up by notify
all, generation might have changed.
So you need gen counters and continue if you are in some older generation.

If I am nth thread, no need to wait, just reset states.
If I'm not the nth thread, do a cv wait.

Some problems
"torn read" bug
reading a value that was never written

this can happen due to partial updates to the registers
I assumed I'm just reading with no itention to modify, you must
lock still

Another is that you can avoid the wait for the nth thead

```cpp
bool move_on = current_ == total_;
cv_.wait(lock, [&] {
    // nth thread || this one is on an older gen
    return move_on || orig_gen < gen_ctr_;
});
```

This move on if true, you should just skip the wait
