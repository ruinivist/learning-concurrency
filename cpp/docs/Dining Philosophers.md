# Dining Philosophers

This is one of the classics. This was framed by Djikstra for an exam ( not in this philosopher format ) but the idea
was to teach the 4 coffman conditions for a deadlock.

## Coffman conditions for a deadlock

A deadlock can ONLY happen if ALL 4 of these conditions hold at the same time, if not it's mathematicaly impossible.
So all you need to do is break JUST one

- mutual exclusion ( of resource ) => make data shareable to avoid
- hold and wait => this means a thread holds some resource A while waiting to acquire another resource B. fix is
  usually an all or nothing hold, so no partials
- no pre-emption => if there is stealing or force release, then too there won't be any deadlock
- circular wait => A waits for B waits for C waits for A

## The problem

Simulate `N` philosophers sitting around a circular table with `N` chopsticks. Each philosopher alternates between thinking and eating.
A philosopher needs both adjacent chopsticks (left and right) to eat. Prevent deadlock and philosopher starvation.

## Incorrect solutions

- naive double locking => hold and wait problem as all can be stuck with just one fork
- non blocking try lock => try to lock one and then try locking the next and if failed unlock both, in a loop. problem is no progress
  guarantee
- a table level mutex => no concurrency ( one of the classis naive mistakes, to use threads and mutex so wide that nothing is concurrent
  anymore )

## Correct solutions

### Resource heirarchy => Targets condition 4, circular wait

If you have a GLOBAL order then there is no deadlock. Note that a LEFT first then RIGHT is not a global order.

**pick the min numbered fork first**

Initially I did a first lock -> try lock on second.
This isn't needed since now we have an acyclicity on locks so
the seond lock is EVENTUALLY guaranteed to succeed.

```cpp
void eat(int id) {
    int left = id;
    int right = (id + 1) % NUM_PHILOSOPHERS;

    // global resource heirarchy based solution to avoid deadlock
    // targets condition 4 to avoid circular wait
    // min first THEN right
    auto &first = forks[std::min(left, right)],
         &second = forks[std::max(left, right)];

    first.lock();
    second.lock();
    std::println("Philosopher {} ate", id);
    second.unlock();
    first.unlock();
}
```

Can also use `std::lock_guard<std::mutex> guard1(first)` and then guard2.
Diff over unique lock is that it is more barebones so lighter and is solely
for a raii lock and unlock, cannot move, does not work with cvs as no relock.

### Arbiter => again targets circular wait but by limiting

virtually capacity is one less, so even in worse case of naive left then
right, ONE will always succeed.

you add a counting semaphore at the top
=> acquire seating token from arbiter

```cpp
std::counting_semaphore<NUM_PHILOSOPHERS - 1> arbiter(NUM_PHILOSOPHERS - 1);

void eat(int id) {
    int left = id;
    int right = (id + 1) % NUM_PHILOSOPHERS;

    arbiter.acquire();
    {
        std::lock_guard<std::mutex> lock_left(forks[left]);
        std::lock_guard<std::mutex> lock_right(forks[right]);
        std::println("Philosopher {} ate", id);
    }
    arbiter.release();
}
```

### State monitor => Tanenbaum's solution, targets hold and wait

The idea is similar to that of a reader writer lock where you read and write
the states under a mutex but don't EAT / consume actual resource under a mutex.

You need a similar cv notif system here of course, one for each philosopher.
Also, note that now that the state is ONE GLOBAL, there is just one mutex
but many cvs to notify.

cpp details => enum class vs an enum, use class always, it has that scoped
resolution and does not pollute globals

```cpp
void eat(int id) {
    int left = left_neighbor(id);
    int right = right_neighbor(id);

    // take forks
    {
        std::unique_lock<std::mutex> lock(table_mtx);
        cvs[id].wait(lock, [&] { return !eating[left] && !eating[right]; });
        eating[id] = true;
    }

    // eat with no mutex so it's concurrent
    std::println("--> Philosopher {} is EATING", id);
    std::this_thread::sleep_for(std::chrono::milliseconds(random_int(20, 80)));

    // put forks and wait neighbors for them to continue if they need to eat
    {
        std::lock_guard<std::mutex> lock(table_mtx);
        eating[id] = false;
        cvs[left].notify_one();
        cvs[right].notify_one();
    }
}
```

There is a longer version more in line with Tanenbaum's original that removes
any superflous wakes ( you wake ony when guaranteed to eat ).
Current impl is also not fair but this state one CAN be made to be fair, the only
solution that's versatile enough ( easily ) if we need fairness guarantees.

### Scoped lock => this targets hold and wait but via STL

Livelock => when threads are running and changing states but make 0 progress. This is
also a stall condition like a deadlock but CPU cycles will be consumed.

How shared lock works internally is slightly similar to the native try lock impl but with
a crucial fix

```
candidate = lock1

loop:
    candidate.lock()                    // 1. Block on candidate

    for each other lock:
        if (!lock.try_lock()):          // 2. Failed to get another lock?
            unlock(all_acquired_so_far) // 3. Drop EVERYTHING immediately
            candidate = failed_lock     // 4. Next time, block on the failed one
            repeat loop

    break                               // 5. Acquired all
```

Example

````
T1 locks A
T2 lock B
one of them tries to lock the other and drop all
fsay T2 try locks A and then drops off B and then waits for A lock
which was help by A already
now A can progress freely

what can happen is a SWAP => A's candidate becomes B and B's becomes A
but rare
```
Note that a generally easier solution is to use a sorted ordering of the mutex
addresses, cpp committee did not use that ( for reasons, idk ).


```cpp
void eat(int id) {
    int left = id;
    int right = (id + 1) % NUM_PHILOSOPHERS;
    std::scoped_lock lock(forks[left], forks[right]);
    std::this_thread::sleep_for(std::chrono::milliseconds(random_int(20, 80)));
}
````
