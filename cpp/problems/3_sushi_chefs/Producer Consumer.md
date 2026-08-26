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
