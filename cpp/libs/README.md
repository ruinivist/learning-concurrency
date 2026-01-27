# mutex

for manual locking and unlocking of critical sections

Have raii wrappers.

# semaphore

a wrapper around a mutex, int ctr, and condition variable

# cv

`cv.wait(lk, cond)` is read as wait until condition holds ( and signalled )

wait until signaled and condition holds

by far very easy to get wrong. some idioms

- Always check condition first before waiting ( can just use the lambda wait )
- ( Perf ) Notify after unlocking
- MUST hold mutex when using (rw) state variables that are also used in the condition ( lost wakeup fix )

# rwlock

uses `std::shared_mutex` and `std::shared_lock` for reads and `std::unique_lock` for writes

multiple readers allowed, single writer only

for read heavy workloads

# latch

a one time barrier

# barrier

a reusable barrier. when counter reaches zero, all threads are released and counter reset

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
