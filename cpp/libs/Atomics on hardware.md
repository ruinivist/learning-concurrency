# Atomics

Assumption here is we have two threads running on separate physical cores and we need
sync between them.

## CPU and cache layers

```
CPU requests array[0]
        ↓
Check L1 → miss
        ↓
Check L2 → miss
        ↓
Check L3 → miss
        ↓
Fetch from RAM
        ↓
Bring an entire cache line (e.g. 64 bytes) into cache
        ↓
CPU gets array[0]
```

A "cache line" is not actual hardware line, it's the unit of data transfer in this flow above.
L1/L2/L3 are the real caches.

So the data is then stored in multiple cache lines according to the size of the cache.

## Invalidating a cache line

Invalidation is needed when data is "replicated". This setup is typical

```
Core 0                    Core 1
┌──────────┐              ┌──────────┐
│ L1 cache │              │ L1 cache │   usually private
├──────────┤              ├──────────┤
│ L2 cache │              │ L2 cache │   often private
└────┬─────┘              └────┬─────┘
     │                         │
     └──────────┬──────────────┘
                │
          ┌───────────┐
          │ L3 cache  │              often shared
          └─────┬─────┘
                │
               RAM
```

If the variables are in L1 cache, an atomic synchronisation would need to block other cores
from reading their invalidated copies. This whole thing is the "hardware cache coherence" system.

What "memory order" handles in cpp ALSO uses cache coherence along with other stuff to guarantee
visiblity orders on memory.

## Cache coherence

This sytem allows synchronisation between caches ( including read and write ). Operations here are
again on the entire cache line.

An example sequence of operations for case where both CPU 0 and 1 have x in their L1 caches.

- CPU 0 needs to update X
- requests an exclusive ownership via cache coherence
  - invaidate other CPU's caches ( across the whole line )
- CPU 0 performs its operations, its L1 Cache is updated with the new.
  - a RAM sync is not done immediately
  - both CPU 1's L1 and RAM have older vaues.
- CPU 1 wants to read and RAM is older
  - CPU 1 sees that's copy is invalid and asks for the newer values via tha cache coherence system
  - new value from CPU 0's L1 cache is copied over

## False sharing

This is a side effect of all operations being done at a cache line level rather than a variable
level.

```cpp
struct Stuff {
    std::atomic_flag lock;
    int unrelated_counter;
};
```

If both of these happen to live in the same cache line, invalidation on the lock also clears
the counter. This can happen whether counter is modified or not so loses ALL perf from it being
in the L1 cache entirely and effecively follows the same lock sematics as the lock itself due
to sharing cache line. That counted could be atomic, non-atomic, anything.

This is why there is padded alignment to 64 bytes when using these atomic vars.

## `atomic_flag` vs `atomic<bool>`

In general these would be the same. Though `atomic_flag` had the guarantee of always being lock free
aka using atomic cpu instructions vs `atomic<bool>` might not be lock free based on the platform.

`x.is_lock_free()` tell wheter it's truly atomic at CPU level or just uses an internal mutex on this
platform.

## atomicity vs memory ordering

There are two places where things can get re-ordered

- at the compiler level based on optimisation inferences etc
- at the CPU level where CPU too can re-order execution order

This 2nd one is a major CPU trick today. Out of order execution accepts the tradeoff of
"guessing" compute and some of it being wasted for an overall higher througput.
How CPUs maintain the illusion of "as-if-sequential" exec even when running out of order
is a different topic entirely for now.

```cpp
// Thread A
value = 123;
flag.clear(std::memory_order_release);

// Thread B
while (flag.test_and_set(std::memory_order_acquire)) {
}

std::cout << value;
```

Atomicity can only guarantee behavior for `flag` is atomic, there's no sync at all for `value`.
It can very much get re-ordered. This is where the `std::memory_order` is used so ensure
that memory is "synced" around the "atomic" aka when this atomic changes so and so about values
must be true.

So in the end we use atomic as the synchronisation primitve rather than wrapping every needed data
in an atomic.

These memory flags ensure a "happens before" behavior on value.

## cpp memory orders

`release` and `acquire` are the important ones. The naming only makes sense if you think in
terms of a lock.

`release` = I've made my changes, release the lock. This is usally on writer's end and PUBLISHES
to "memory" ( not exactly but ensure behavior is same )

`acquire` = I wanna read, take the lock so I know I see all that is there to see. Usually on reader
end. ENSURES that what WAS published is visible; hence these two always go in pairs.

`relaxed` = don't care about sync or publish, I only care about the atomic operation here not
value or anything else.

`acq_rel` = don't think used commonly. this is acquire + release as one

`seq_cst` = default and slowest, serialises instructions to one global timeline.

In general, if you want to see the chage made by a by some release in another thread, you NEED
to have acquire when reading.

### Comparison to my MPMC

Even though I wrote above, I still had trouble understandin what to use when in my MPMC.
I think this is clearer with an example though.

> Do I just need atomicity by CAS or am I trying to use this atomic as a sync fence?

For enq_pos, there is not state tied to that variable, all I need is just the CAS guarantee
that only one producer can proceed; so there I can use a relaxed order.

But for the seq numbers, there is data tied to that atomic to be written by prod and read by
cons; I need the re-ordering guarantee there so that once my prod atomic is published, the
data read MUST be visible by reader. Think of writer -> reader ( in this order ) as an acquire
must be tied to a previous release.

Again remember that that atomics are atomic regarless of acquire release, but the operation
re-ordering is ALL that we do here, it's not needed for any "staleness" on the atomic itself.

## Cache line ping pong

It's not exactly false sharing but the reason is same. Here there is just one variable ( as opposed
to two in false sharing ) and the effect is that the same atomic variable keeps getting bounced
along cache lines for different cores. This hurts performance under contention.

Note that this is not a ERROR like false sharing that can be fixed, it's just a consequence of
using atomics, though we can limit the invalidations a bit.

> `load` and `test` are same. Just that `std::atomic_flag` does not have `load`

A read does not invalidate cache so this is sometime used - test and test and set

```cpp
while (true) {
    while (flag.test(std::memory_order_relaxed)) {
        // spin using ordinary atomic reads
        // no cache invalidation
    }

    // only wen it's highly probably that this will work
    // try to acquire
    if (!flag.test_and_set(std::memory_order_acquire))
        break;
}

// actual critical section

flag.clear(std::memory_order_release);
```

This above you can also write as which optimises for the uncontended path more
by avoiding a possibly redundant load.

```cpp
while (true) {
    // Fast path:
    // try to acquire immediately.
    if (!flag.test_and_set(std::memory_order_acquire))
        break;

    // Failed, so the lock is held.
    // Spin using cheap read-only atomic loads.
    while (flag.test(std::memory_order_relaxed)) {
        // no RMW / no ownership fighting here
    }

    // It now looks free.
    // Loop back and try test_and_set() again.
}

// actual critical section

flag.clear(std::memory_order_release);
```

A little more depth on this is needed. A `test_and_set` is an RMW operation.
It makes more sense as a `fetch_add(1)` which is also an RMW operation on an atomic int.

```
Read:    old = x
Modify:  new = old + 1
Write:   x = new
```

In the case of an `atomic_flag`, it looks like

```
Read:    old = flag
Modify:  new = true ( this looks redundant here but look at prev one )
Write:   flag = new
```

Because the entire thing is to be done atomically, you cannot intrnally optimise to do the
test and test and set thing, else it won't be atomic as a whole + it's again a HINT, not a
guarantee. This is why a `test_and_set` CAN invalidate even if the value is already set; it may
not if it is on the SAME cache line as core and the whole coherence thing does not kick in.

## How many atomics?

Atomics are for sync the other data ( "stuff" being synced ) does not need to be atomic.

The idea is often that you use one atomic to map to onse set of state changes, this allows for a
higher concurrency. Using just one atomic for ALL of state is bad idea as then you're just asking
a thread to take exclusive lock of the whole state before changing anything.

## Skipped ( for now )

ABA is interesting but I'll cover those in time.

MESI state transitions
store buffers
invalidate queues
speculative execution
memory fences at the instruction level
NUMA coherence
LL/SC forward-progress guarantees
ABA
sequential consistency proofs

# CAS

> compare exchange swap is just not a term at all, don't know
> where I picked it from

CAS - compare and swap

The whole idea of CAS is that the oddly specific pattern of
compare, exchange and swap can be done in ONE atomic operation
on the cpu and are directly supported by cpu itself as an opcode.

There are two types a weak and a strong, difference is that a weak
one can spuriously fail even when there is no lock while a strong
one does not. Why do we care? Most of the time you will be using
CAS on a loop so it won't matter, as on a failure you NEED to read
again so you NEED retry, in which case whether the retry actually reads
something new or not does not matter much; and perf is better as strong
won't have to internally do a retry.

CAS( address, expected, new )

```
if *address == expected:
    *address = new
    return success
else:
    return failure
```

The atomicity allows this loop to exist

- I check value of x to be A
- take some time to compute f(A)
- now atomically I want to do, if x is still A which means my compute
  is valid and not stale, set x to be f(A)

# ABA

ABA is a classic problem with CAS, by the very nature of the algo.

If a change was A -> B -> A then CAS will just ignore the history
of changes and have no idea that a modification was made in between.

This may or may not be fine depending on what we are trying to do.

For example, for a lock free stack, ABA can lead it to an invalid
state.

top
A -> B -> C

Thread A:
read top to be A
compute f(A) as: set top to next of A which is B
PAUSED

thread B:
pop A
pop B
push A
PAUSED

thread A:
try CAS( \*A, A, B ), which suceeds as top is still A by a
value comparison and set top as B which is an invalid state
for the stack, could be freed etc.

Note that the key problem is WHAT we are comparing in CAS
if we used some globally unique ids for example instead of values
then this is not a problem; so very much depends on what we are
trying to do and with what data.

Note that tagged pointers / versioning is a natural solution.
