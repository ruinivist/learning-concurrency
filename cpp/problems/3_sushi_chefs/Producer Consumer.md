# Producer Consumer

There are many ways to solve the core problem. Even many ways to implement the approaches too.
I'll try to cover the major ones.

## Bounded Blocking Queue

The idea is simple, producers will get blocked on push and the consumers will get blocked on pop; if
the structure is thread safe, the problem is solved.

### Mutex and CVs

If you remember the high level ideas the rest can be derived.
Here's a chain of thought.

- I definitely need a queue to push and pop
- There's just ^ this one resource to sync so one mutex will do.
- I also need to notify blocked producers and consumers so need a cv too
- ( IMP ) I need two directions of notifications here. What are the event boundaries? Consumers check
  when producer notifies, producer ( if any blocking ) push more when consumer notified.
- So I need to block the same mutex via two cvs.

The rest is just implementation and I can do that always.

### Semas

The idea is to maintain two sets. Refer sema in [cpp.md](cpp.md)

### Spinlocks

These are just the same, you replace mutex with a spinlock the rest is all the same.

## SPSC regime

Single producer single consumer is a precursor to the generic MPMC queues which are much more
complicated.

The key optimisation this allows us is that in a ring buffer, producer and consumer can read
and write the head and tail independently. Right now so far we block both via the same lock as in
the queue as a whole itself is locked.

If you see the "How many atomics" section in [here](<../../libs/Atomics on hardware.md>), it
becomes clear that we have two disjoint set of states here ( head and tail ) so we need two
atomics to have them work independently.

> technically I could split that under two mutexes as well but well, let's not do it now
> it might be "educational" to do that and benchmark though

General SPSCs that are studied and demonstrated claim lock free by not using mutexes and wait
free ( aka non blocing ) by using `try_push` and `try_pop` operations that return without any
blocking.

### A short note on ring buffers

The one that we'll use here will be the "one empty" slot ones. This allows us to distinguish full
vs empty ( as both would be head == tail if we did not have a reserted slot )

With this

- empty => head == tail
- full => tail + 1 == head

### SPSC ring buffer using atomics and "try" semantics

This is the usual "lock-free" spsc.

The key thing that SPSC gives you the guarantee that push will only be called by producer
and pop only by consumer. This allows you to now have any sync on "tail" for push and on "head"
for a pop. Now of course both vars are read so you need to sync the other one.

> correction: no sync is needed on read, but since the other one can be reading what YOU changed
> there you do need a publish even for your owned var

### SPSC ring buffer using atomic waits

I don't quite understand well what's the difference between say using two mutexes to create the
same exclusive state that these atomics make -- but well, in time I guess I will understand.

## MPMPC

This is the "current" state of things though the ideas are nothing new
at best there have been practical variations but that was it mostly.

The very original ( not the first but rather the first popular ) one
was this linkled list based impl in 1996 ( https://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf ) so that was the OG.

What's practically used now and is the current popular version is
the Vyukov MPMC which uses ring buffers ( instead of linked list so
you don't have to bother with memory alloc dealloc ) and uses seq
numbers; though it is a BOUNDED one as opposed to UNBOUNDED of the
original.

There are mainly these two things that change mostly, bound vs
unbounded and blocking/lock-free/wait-free etc.

### Some terms

- Blocking: one stalled thread can stop anyone from progressing
- Lock-free: a failure for one MUST mean that SOME thread has made
  progress
- Lockless ( practical lock free ): while above is the formal defn of
  lock free, what people usually mean is that it uses atomics ops.
- Wait-free: every thread is guaranteed to progress in a finite number
  of steps, independent of other threads.

### Vyukov MPMC

The key idea is that of seq numbers, logical vs physical positions.

Assume, capacity is 4.

Enqueue and dequeu ops always work on ever increasing logical positions
like 0, 1, 2, 3, 4, 5, 6 ....

which map back to a physical position ( "slot" ) as logicaly position % cap

so 0, 4, 8, 12... all map to the 0th index on the queue, and work as generation
counters as to what generation of "reuse" it is for this slot.

For a producer that wants to enq => ask for next logical slot aka enq position
assuming we just started and it maps to slot 0 % 4 = 0

```
producer:
    read enqueue_pos = p
    inspect slot for p
    > NOTE: that you don't compare after remainder as you want generation info too
    claim p by changing enqueue_pos to p+1
    write data
    set slot.sequence = p+1

consumer:
    read dequeue_pos = p
    inspect slot for p
    claim p by changing dequeue_pos to p+1
    read data
    set slot.sequence = p+capacity

Comparisons on seq and p ( for producer )
seq == p => exactly the generation we need; try to claim it
seq < p  => slot hasn't reached WRITABLE state; queue is full here
seq > p  => some other thread updated, retry

Comparisons on seq and p ( for consumer )
seq == p+1 => we are good to read
seq < p+1 => slot hasn't reached READABLE state; q is emtpy
seq > p+1 => some other thread updated, retry

to summarise you can think of seq being the truth on the slot
if it's behind that's a full empty state ( not reached required state )
if it's ahead that's a stale enq/dq pos and we retry
```

Naturally the enq and dq position counters must be atomic as first is shared
among P producers and second among C consumers. What about seq per slot?
At first it looks like it does not but that too can shared across ONE consumer and
ONE producer both looking for that same slot, so that needs to be atomic as well.

The impl does not handle

- counter loop overs
- consumers busy wait as no sleep ( a yield just hints that something else can run but another
  consumer would anyways keep on spinning )

# TODOs

- work stealing
