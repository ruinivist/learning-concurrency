# Thread Pool

## SMT

Simultaneous multi-threading
Hyperthreading is just intel's smt impl

At the hardware level, it logically divides one core into
multiple, usually two.

## How many threads do I need

Say that each task on average takes $S\ ms$ of CPU time and then waits
for $W\ ms$, then fraction of time waitng is $S/(S+W)$. So per unit time
this is the actual cpu usage fraction => you would want to spawn $N$ of those
such that all $C$ cores are consumes ( basically each job takes a fraction
of core ).

So

$$
N \times S/(S+W) = C
$$

## The "almost good" pool ( task 2 )

It can takes args, preserve their value types, returns you futures, all
the needed bits.

Now what changes from the initial impl in task 1.
Here's a rough list of what I definitely need

- perfect forwarding
- packaged tasks
- templating over submit so it can take a variety just like thread
- `std::move_only_function`

### Why `std::move_only_function`?

This was added in cpp23, previously the workaround was to live with a more
complicated but STL impl or make the equivalent of a move only function
yourself.

The problems starts with `std::function`. What is done is that you use
packaged task ( it's use is only to get a wrapper around a future that you can
then return ), nothing more. Then you use a lambda to bind the args on that packaged
task so what you get for each worker is a consitent no arg function with no need to
do anything other than just call it ( the packaged task will handle the future ).

Now problem is, packaged task is move only but `std::function` is copyable too. so you
cannot use a lambda that has captured a move only variable with `std::function` ( lambdas
inherit the copy move semantics of the intersection of what they captured ). So the hack
earlir was the wrap the pacakged in a shared pointer and then capture that shared pointer
as that is copyable.

Now we don't need that as `std::move_only_function` exists. This simplies some of the wiring.

### How do I take take varargs templated and perfect forward them?

This is a bi of cpp generics monstrosity.

#### Some pre-requisites

1. `decltype`

this is a cpp keyword, what it does is substitute the compile time type where it's used.

```cpp
int x = 10;
decltype(x) y = 20;
```

is valid as that whole decltype becomes `int`.

One of the quirks of decltype is type of expression vs variable.

```cpp
int x = 10;
decltype(x) => int
decltype((x)) => int&
look at (x) as an expression, it returns an lvalue ref to x so type
becomes int&
```

2. `auto` return types

Generally there are these three ways you can use auto, the first two are mostly
the same, third one is interesting but idk where you'll ever use it.

- `auto fn(int x) { return x*x; }` => you let compiler infer
- `auto fn(int x) -> int { return x*x }` auto with trailing returns? But if you're going
  to specify the type why use auto at all? This is only used for cases where you want to depend
  on type of the params and since wen you specify return value you don't have them you muse use
  this syntax. Very rare.
- `decltype(auto) f() { return ((x)); }` => auto will never infer a reference type like decltype
  can do for expressions, this just means infer not from the value but the returning expression.

#### First we start with no args templated callable

```cpp
template <typename F>
requires std::invocable<F>
std::future<std::invoke_result_t<F>> submit(F&& f) {
    using R = std::invoke_result_t<F>;

    std::packaged_task<R()> task(std::forward<F>(f));
    auto future = task.get_future();

    submit_(std::move(task));
    return future;
}
```

- that concept will just make the failure clearer if it's not invocable, nothing else really
- `std::invoke_result_t<F>` is just what type do I get when I invoke F.

what does that `std::forward` give me at THIS stage?
Say f was a lambda that was move only because it captured a unique pointer, and also it's an
rvalue lambda, constructed inline. I've already using forwarding reference in the arg ( F&& ),
what if I don't use forward here? It'll just try to use f as it's inferred now, inside the func
it's an lvalue reference so it'll try to copy it ( rem lvalue = copy, rvalues = move are the
usual semantics ). The `std::forward` served to cast that f to rvalue if the arg was originally
rvalue.

The rest is straightforward.

#### Extending to args

Refer cpp variadics, the rest here is just an application of it.

The ugly part is the bound as it has a lot of things going. Here's a line of reasoning
of the different bits.

- we need to bind capture both the callable and the args so we what while usine perfect
  forwarding to retain the original types.
- the `...expanded_args = std::forward<Type here = Args>` is just cpp syntax of it
- that decltype auto is then to RETAIN the returned type as it is from the expression.
- invoke is simple but why move? Well we've already got our captured args in the lambda
  copied or moved depending on their original types, now we just want to pass them to invoke
  and there's no point in copying again for invoke, so just move it all.
- why the mutable? our captures store values; by default the lambda's operator() is const,
  so mutable makes it non-const and lets me modify what I captured in `[]`
- note that in this case, we are capturing it all AS A COPY ( hence the move ), without mutable
  that move will not work as well. Why capture as copy? so the task remains valid even if the
  caller that submitted it goes out of scope.
- how would references work? submitter would need to be aware of using correct `std::ref` or `std::cref`
- value capture drops top-level const when deducing the stored type so in this case
  - if my f was const, do I lose that? no, we only modified the lambda scope not f
  - if some arg was originally const do I lose that? doesn't matter since we copied
  - what if that some arg above was reference? A reference is still copied, it's the whole
    `std::ref` when using threads problem and the reason is the same, a mutable or anything we
    do here cannot drop constness from `std::cref` at all.
- in invoke call, we are just doing a pack expandion
- `using Result = std::invoke_result_t<decltype(job)&>;` is how it should be.
  that & at the end does decltype IF the callable is invoked as an lvalue. Needed for very exotic cases so I'll just drop it for now.

## Backpressure ( task 3 )

In general such thread pools are expected to run some significant enough workload that
actually running the task non-locked is what takes the most time, contention on the lock
is expected to be minimal and not the main blocker so improvements like using ring-buffers
or atomics offer little gain unless a proven hot path or large number of very quick tasks ( in which case, do we really need a thread pool as the whole might just be more overhead than the task itself, but again, always test ).

Here, we just use deque and cvs. Here's a line of reasoning,

- the pool max queue size is fixed taken at input.
- I need submitters to be blocked if we're full so the submit itself needs a cv to
  continue when the queue is not full OR a stop is sent. In case someone is blocked while
  a stop is sent, throw for that submitter.
- any notification changes? just the above, right now my submit just uses a lock guard, no cv based wait.
- new cvs? remember that the current cv is "stop*or*(pick)\_task" and depends on
  workers ( thread count being free ), what we need is another one "stop_or_put_task" which depends on the capacity being free even if all workers are running.

### Templating vs ctor args

I noticed that I've been biased to just templatng members like sizes and capacity for no
real reason. For my thread pool impl so far, I have the thread pool size templated and
then use a `std::array`. Remember how arrays NEED the sizes compile time and that is being
restrictive, for possibly no real gain ( compiler might have more info for optim but vecs
are pretty optimised ).
Avoid the bias of templating and just use ctor args.

## Work stealing ( task 4 )

The generic task pool so far is almost good and what you would reach for usually. This is for specific parallel heavy workloads that spawn parallel tasks again
that can get asymmetric fast. Because you see the whole point of "work stealing" is to FIX a problem that isn't even a problem so far. We introduce local queues
to do quick worker level task scheduling and then fix the asymmetry by work stealing.

Here's a line of reasoning.

- each worker will need some local state so we mak a private struct for that
- an idle worker would prefer own local queue ( newest ) then steal ( oldest ) first ( so that the external submitted unit of task is done first ) and only
  then pick a task from global queue. Reason being if it's my local, the newest is likely cache hot, if I'm stealing I should help by stealing oldest.
- stop semantics are all the same, complete existing, stop taking new ( throw ); unbounded to make it easier.
- a clean shape then is just to have all local state including the thread itself
  in a worker struct.
- the worker loop should use a separate `try_steal` to make the stealing logic cleaner.
  a `try_steal` is meant to be conservative in the stealing ( hence the try ), intention is to
  use try lock semantics and only inspect where you can actually get a lock instead of waiting
  to get a lock and checking. In this case, finding nothing to steal will not mean there was
  nothing to steal but that's fine, hence if some were skipped due to not being able to lock we
  should keep trying, this can become spin lock like busy wait.
  - Another option is to drop that try semantics and for each worker, block on lock to check.
  - The tradeoffs I feel can only be reasonably estimated for the workload and through benchmarks.

### submit needs (pool, worker) identification

There are 3 cases we need to be able o distinguish

- a submit called from external ( main )
- a submit called by a worker from this pool
- a submit called by a worker from ANOTHER pool

the idiomatic way to go about that is to store thread local vars.
`inline static thread_local WorkerState* current_worker_ = nullptr;`
when a worker runs it'll set that ptr to it's own worker state.
This is enough to distinguish the first two cases.

The last two need one more state tied to each worker, it's owning pool.
The ctor of the thread pool will set that state, then workers can check
if that current worker pointer is set ( it's inside some worker ) and the
owner is this ( the task is originating from THIS pool's worker not some
other ), then we do a local queue else global.

`inline static thread_local`, the `thread_local` is obvious, the first is
cpp reusing same keywords again to make things confusing.
A class level thread local NEEDS a static, a static inside a class then
NEEDS an inline ( else you would have to define it outside ).
I need to come up with some coherent model of this ( provided there is one ).

For impl,

- set owner ptr in ctor
- set worker state ptr in worker start
- submit needs both current worker being set AND owner being this to use
  the local queue

Can it happen that a submit vs a worker start setting that state pointer have a race?
No, because then the task would just go to the global queue and when the worker does
start eventually it'll pick that up.

### task count handling

this is another bit that gets tricky in this one. stoping for examle is global but
needs to be handled local too. If you are waiting on a global task ( 3rd step in worker
loop ), a local task won't wake you up.

So the waiting cv needs to wait both a global task count, a local task count and wake
up on either. A simple way is to just keep sum of both as global list of tasks across
the whole pool and then you wait on that, this way a local block does not prevent you
from seeing a global addition and the other way around as well.
An atomic int is IDEAL here as a counter for.

impl details

- atomic counter as not all usages are under a global lock
- stopping behavior NEEDS you to read stopping under a global lock
  and THEN take a local lock as well, this is wrong
  - global stopping check needs the whole of submit to be under stopping not
    changing, else this can happen
- the final wait changes to `stopping or all tasks > 0`

```cpp
submitter: checks stopping_ == false
submitted paused
destructor: sets stopping_ = true, wakes workers
workers:   finish and exit
submitter resumed
submitter: puts task into local queue
```

### Reflections

This work stealing impl is a toy one but it leans towards more toy than real.
Problem being we added local queues to have have fast local submit but ended
up locking the whole of submit under a global mutex. The local deque op is
not under a global mutex but unsure how much that really buys us.
There is also this buy retry loop in my `try_steal`.

In any case, I think the objective of "learning" what it is and some of the
details on impl for it is satisfied. Apparently, the more idiomatic real impls
use what's a Chase–Lev-style deque but let's just stop for now; I don't think
exploring this branch more serves me much.

# Tasks

## 1. Fixed-size fire-and-forget pool

No args, RAII based stopping. Remember to execute tasks without mutex, mutex
is just for metadata like a rw lock. Stopping semantics is graceful shutdown =>
stop taking new tasks, but finish all queued ones.

## 2. Generic future returning pool

Generic `submit(f, args...)`, perfect forward to eventual invocation and return a future
where caller gets rv or error. Use packaged task, but rem that it is move only so as hint
task queue needs `std::move_only_function`; think of void, error, references, move only args

## 3. Bounded queue / backpressure pool

Give task queue a capacity. submit blocks while full, workers needs to wake a blocked
submitter when it pops; also on shutdown wake them so they throw and don't sleep forever.

## 4. Work stealing pool

Give every worker a local deque: it runs latest local task for cache locality, idle workers
steal oldest tasks from others. Worker spawned tasks go local; external submits stay global.
The point of work strealing is for handling tasks that themselves spawn tasks.
Drop boundedness to make impl easier again.

# cpp bits

what happens if the dtor is private?
compile time throw
