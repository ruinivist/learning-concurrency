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

# Tasks

## 1. Fixed-size fire-and-forget pool

No args, RAII based stopping. Remember to execute tasks without mutex, mutex
is just for metadata like a rw lock. Stopping semantics is graceful shutdown =>
stop taking new tasks, but finish all queued ones.

## 2. Generic future returning pool

Generic `submit(f, args...)`, perfect forward to eventual invocation and return a future
where caller gets rv or error. Use packaged task, but rem that it is move only so as hint
task queue needs `std::move_only_function`; think of void, error, references, move only args

# cpp bits

what happens if the dtor is private?
compile time throw
