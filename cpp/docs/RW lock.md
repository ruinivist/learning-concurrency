# RW Lock

## Chain of thought

I need both readers and writers so I need two way signalling aka 2 cvs and a
common mutex ( as the resource is the same ).

There must not a writer starvation so a writer "signal" must stop admission, note that there CAN be multiple of these so again
you need a count, this gets you to a reader count, active writer
count and waiting writer count.
for new readers. When readers reach 0 and writer signalled ( aka
waiting writer ), cv notify the writer.

Note that there can be different impls

- reader biased ( readers at top of q )
- writer biased ( writers at top of q )
- FIFO fair ( arrival order respected )

Some implementational problems I had

- if I lock the mutex for using cvs, how is it reader concurrent?
  You must use mutex only for counter states and must unlock it before
  reading so reads are parallel.

A reader loop is
lock mutex to WAIT on stuff
inc active readers
unlock
READ ( unlocked reads )
lock to inc active readers
unlock

So the whole idea is you do not hold a lock when reading
