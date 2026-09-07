# Cigarette smokers problem (4.5)

There are three smokers and one agent. A cigarette needs three ingredients:
tobacco, paper, and a match.

Each smoker has an unlimited supply of one ingredient:

- one has tobacco
- one has paper
- one has matches

The agent repeatedly places two different ingredients on the table. The smoker
who owns the missing third ingredient takes them, makes a cigarette, and smokes
it. When finished, that smoker tells the agent to place the next pair.

For example, if the agent places tobacco and paper, the smoker with matches
should proceed.

The goal is to synchronize the threads so that:

- only the correct smoker proceeds
- the agent waits until that smoker finishes
- the system keeps running without deadlock

Restrictions

- agent cannot KNOW of smokers internal state as in agent does not decide what
  exact smoker to wake up based on items.
- agent code cannot be modified
  - with gen counters I could have the smokers wait on two cvs and reset if
    generation changed to avoid partial consumption ( solution 2 ), this way
    we don't need to wake ALL smokers

## Solution

an obviously wrong solution is to try to individually lock the two items, that's the same issue
as dining philosophers but you cannot make a global order and use that because agent owns
one of the resource ( consider that the agent has "locked" the missing one and freed the other
two ).

### Wake all ( solution 1 )

this is simple if you just wake all smokers and let them decide but the whole idea of
this problem was to relate this to agent being the kernel and the smokers being the user
applications. Kernel can't just release some resources and ask every thread to fight for
it

### Queue like setup ( solution 3 )

Instead of letting each smoker decide, we make queues for each ingredient and notify
on the status of that change => this is the "pushers" idea from the book. "pusher"
as a name makes more sense since there's the queue will actively push out the event
instead of passively waiting.

This fits the contraints however I feel this just shifted the smoker selection to the
pushers; from an os perspective, we still can't expect pushers to select exactly the
next process.

## Learnings

- prints are also part of data race
- cvs CAN wake up spuriously so stateless blocks are just plain wrong
- I did the cv based solution on 3 first but that ende up needing individual state to be
  coupled with each cv; you always need a state due to ^ but if the state ends up mapping
  one to one, you would def want to use a semaphore instead as it's just a cv + counter
  encapsulated in one
