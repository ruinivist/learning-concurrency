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
  encapsulated in one. For example in sol 3, I have 6 cvs and 6 int counters for each,
  one to one.
- it's a bit non trivial to make an array or vec of semaphores as not copyable or movable,
  what you can do is wrap them in an optional
- I should use semas more, they naturally avoid unique locks which often ends up locking
  global mutexes unless you are careful
- one less line when modifying based on indexes and values

```cpp
for (auto [idx, val] : table | std::views::enumerate) {
    val -= (idx != have);
}
```

## Cigarette smokers ( harder variant )

What this on changes is that there is no requirement for the agent to wait for
a smoker to complete and hence there can be multiple instances of ingredients
available for the smokers to use.

I'll modify just the semapore based pusher solution for this harder variant.

Here's the chain of thought

- do I need an agent sema now? there's no need of sync, agent can KEEP on adding so no
- when anyone can proceed, do I need to be smart about it as to who I wake up? say matches
  are in abundance and I keep on waking the smoker that already has matches; I don't think it's
  a problem as we are trading one smoker for another, as long as the matches guy DOES NOT end
  use using agent supplied matches instead of their own; no guaranteed on fair order among
  smokers but that's not a requirement
- In my current solution for the simple case, I did not like the idea of pushers consuming
  table counts on behalf of the smoker and then waking them up but here we have NO other choice,
  see my big comment in the smoker function.

I do not really like the book's solution here, even more so as we did not want the agent ( os )
to know about each application's resources but now we just moved that off to pushers, and not just
signal but the acquiring part as well, I don't see how this scales well to an os.

## Cigarette smokers ( harder variant )

A really nice trick for this 3 ingredient version is to not store the full table AT all but
just the one ingredient we have, as soon as we get to two ingredients we clear them but if
there's just one and even if it's repeated by agent to be match:1 to match:2, all we need is
that one ingredient for state.

By reducing state this way, I can get state that is atomically lock free on my machine; so we
can get to "some" level of lock-freeness. Due to the waits this is not "lock-free" in
the formal sense still.

### An always mistake on atomic memory orders

This is my bias, I see an atomic release acquire as the atomic values themselves being re-ordered.
What they re-order is the surrounding state, the atomic itself is visible as if serialised, even
if relaxed. These fences are ONLY for other variables. In my impl, saying that all I use are atomics so I don't need ANY sync at all is wrong, what I do not do is observer one
atomic and rely on seeing an update to another different atomic which is why I can use
relaxed all the way. Think here the impl is similar to a "turnstile" due to pushers.

### Back to the problem

here is what the flow looks like

helpers
post -> add one to permit
take -> wait till permit it's not 0 then subtract one from permit

agent loop
post two random permits in a loop ( pusher pending array of 3 atomic counters )

pusher loop
take that permit
do a cas on the scoreboard to find the smoker
then post to smoker atomic counter

smoker loop
take from smoker
