# mutex ( section locking )

for manual locking and unlocking of critical sections

Usage would almost always be a `unique_lock`

```cpp
{
    uniq lock

    locked section
}

other stuff
```

# semaphore ( a composite )

a wrapper around a mutex, int ctr, and condition variable

# cv

Wait on a lock so you need a lock. This is for signalling purposes
where you want => wait till cond holds

- `cv.wait(lk)` where you check the condition but that is error-prone
- `cv.wait(lk, cond)` ALWAYS prefer this; read as "continue when condition holds ( and signalled )"
  - internally this is just `while(!cond) cv.wait(lk)`
  - this above is not a spin lock, cv has a lot of CLEVER logic within ( TODO )

by far very easy to get wrong. some idioms

- Always check condition first before waiting ( use the lambda as condition to avoid wait )
- Notify after unlocking and modifying ALL the state you need to modify

Something like this is almost always correct.

```cpp
{
    lock
    cv.wait(lock, cond)

    do stuff
}

cv.notify
```

# rwlock

uses `std::shared_mutex` and `std::shared_lock` for reads and `std::unique_lock` for writes

multiple readers allowed, single writer only

for read heavy workloads

# latch

a one time barrier

# barrier

- reusable
- expects N arrivals per "phase" ( which is reset and hence the reusable part )

```cpp
std::barrier b(3); // 3 arrivals
barrive_and_wait(); // mark this arrival and wait for others

// LESSER USED
arrive() // only mark arrival
wait() // only wait, used when ^ is used, kind of go in pair

arrive_and_drop() // mark arrival and permanently decrease the init arrival
// count that was set
```

# future and promise

move only.
for returning values from threads

# call once

for singleton initialization

# async (future helper)

create fut and threads easily. fire and forget

# packaged task (future helper)

created task wrapper but execution is manual (unlike async)

# shared future

10 threads waiting on a single future value ( like config load )
copyable future
