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
