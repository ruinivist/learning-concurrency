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
    generation changed to avoid partial consumption ( solution 2 )

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
on the status of that change => this is the "pushers" idea from the book

So the agent will notify on the status change of the two ingredients it added.

## Learnings

- prints are also part of data race
