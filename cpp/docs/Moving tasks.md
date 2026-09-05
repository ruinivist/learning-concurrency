# Moving tasks

covers cpp stl stuff for packaging and moving callables, args and return values.

## Promise / Future

A promise produces a value, a future thread can wait for and receive the value.
There is no notion of any "callable" or how that value is computed yet.

these are wrappers around a shared state that you interact via promise and it's
generated future. the api is just

```cpp
#include <future>

void work(std::promise<int> p) {
    std::this_thread::sleep_for(std::chrono::seconds(2));

    p.set_value(100);
}

int main() {
    std::promise<int> p;
    std::future<int> f = p.get_future();

    std::thread t(work, std::move(p));

    std::cout << "Waiting...\n";

    int result = f.get();

    std::cout << "Result = " << result << '\n';

    t.join();
}
```

To note

- move on promise, it's move only
- get and set can ONLY be called once
- if you need to wait but not get, you can use `f.wait()`
- `promise<void>` to just signal with no rv
- errors? `p.set_exception()`
- future outliving the promise itself **without promise setting a value or error** throws a
  `std::future_error`. The shared state will still live even if the promise is out of scope but
  future remains

## Async

Another higher level wrapper over a promise / future

```cpp
#include <future>
#include <iostream>

int add(int a, int b) {
    return a + b;
}

int main() {
    std::future<int> f =
        std::async(std::launch::async, add, 10, 20);

    std::cout << f.get() << '\n';
}
```

Launch modes

- `std::launch:async` => launch thread now and run async
- `std::launch::deferred` => delay exec until get or wait called, will not make another thread and just use
  the one that called the wait / get
- default => impl can choose any ( gcc, clang etc )

Exceptions? They are stored inside of async and rethrown as is when you call get.

### Dealing with reference arguments

async may copy or move arguments based on their value category

```cpp
std::async(std::launch::async, work, s);            // lvalue -> copied
std::async(std::launch::async, work, std::move(s)); // rvalue -> moved
std::async(std::launch::async, work, std::string("hi")); // rvalue -> moved
```

Now this causes a problem if you pass a reference as references are not a separate category,
they store named values and are simply lvalues.

```cpp
int x = 10;
int& r = x;

r   // lvalue expression
```

So what async does is try to copy it like `stored = r` which makes a copy of the VALUE, now modifying
stored does not modify r.

What does `std::ref(x)` wrapper do?
It creates a copyable wrapper on that int that refers to the same object in mem
`std::reference_wrapper<int>` will refer to x ( original int ).
so how do usual int ops work with what is a wrapper?
It has implicit conversion to `T&` and `const T&`. I naively thought it has all the overloads which would be
QUITE some work, even impossible given it's a generic.

## Packaged task

So far promises have no notion of callables, you must handle it yourself.
Async does not have much flexibility on WHEN the call is made.
This gets use to packaged tasks which is almost like async in the sense just returns you a future and lets
you manually call the task when you want to.

```cpp
#include <future>
#include <iostream>
#include <thread>

int add(int a, int b) {
    return a + b;
}

int main() {
    // pass in the call to wrap it into a task, ARGS are NOT part of this step
    std::packaged_task<int(int, int)> task(add);

    // get a FUTURE
    auto future = task.get_future();

    // move it to a thread, the THREAD will call it FOR YOU, so at this step you need the args
    // note that task is the CALLABLE so you can do
    // task(10, 20) as well
    std::thread t(std::move(task), 10, 20);

    // block on the val
    std::cout << future.get() << '\n';

    t.join();
}
```

The ref semantics all apply just the same, but only with threads.
So for packaged task itself it has info on all the args directly from the specialisation you use,
so there is no need of a `std::ref`.
This is problem for `std::thread` and the args you pass there as it is THREAD that does the same
decay copying as async, so there you need ref handling, but the blame lies with thread here not packaged task.

## Perfect forwarding

To illustrate the problem with a toy example

```cpp
#include <iostream>
#include <utility>

// you can omit names if you don't use them
void process(int&) {
    std::cout << "lvalue\n";
}

void process(int&&) {
    std::cout << "rvalue\n";
}

template <class T>
void wrapper(T&& x) {
    process(x);   // naive
}

int main() {
    int a = 10;

    wrapper(a);             // lvalue
    wrapper(std::move(a));  // you'd expect rvalue but it's still lvalue
}
```

### Some pre-requisites

What is really T&&?
there are two different variations => rvalues vs forwarding reference

```cpp
// 10 is an rvalue, temporary. by binding this wya to a named rvalue ref
// the lifetime of that temporary extends to that of x
// this is lifetime extension
int&& x = 10
// so far this is just using and binding rvalue references

// but in templates the && is re-used to mean a forwarding reference
// this is a new lang feature from cpp11, that lets the function remember
// it's caller's passed value categories
template<class T>
void f(T&& x) {

}

// Example
int a = 10
f(a); // T = int& ( why? explained later... )

f(10); // T = int
```

How does it decide what type to pick for `T`?
Rules per standard used in deduction when using forwarding references:

- original arg is lvalue → `T = int&` <- THIS(1)
- original arg is rvalue → `T = int`

"Reference collapsing" then explains what happens when substitution produces references on top of references.

The first reference comes from what is written in the template function, here `f(T&& x)`
The `&&` is part of the function parameter type.

The second reference, if there is one, comes from what `T` was deduced to based on what the caller passed
( the THIS(1) above )

Reference collapsing rules:

- if there is any `&`, the result is `&`
- only `&&` + `&&` results in `&&`

So for our perfect-forwarding case:

```text
caller passes lvalue:
T = int&
T&& = int& && = int&

caller passes rvalue:
T = int
T&& = int&&
```

So, in REAL short, as just this is what's mostly needed

```text
lvalue → T = int& → parameter becomes int&
rvalue → T = int  → parameter becomes int&&
```

Note that `std::forward` is not involved so far, it's correct usage
in fact comes later, inside f.

### Back to what went wrong?

Now we can explain, what went wrong in our original example.

```cpp
template<class T>
void f(T&& x) {
    g(x);                  // passes x as an lvalue
    g(std::forward<T>(x)); // restores caller's original category
}
```

when called as `f(10)`, since caller is an rvalue, T deduced to `int`
then `f(T&& x)` becomes `f(int&& x)` correctly so far but INSIDE when
g is called, since x is named storage just like any other variable x becomes
lvalue. To fix that you need `std::forward`

What does this forward do?
Internally it is roughly

```cpp
template<class T>
T&& forward(remove_reference_t<T>& x) {
    return static_cast<T&&>(x);
}
```

read that `remove_reference_t<T>&` as two parts

- `remove_reference_t<T>` => pick any T, T&&, T&& and just make it T
- take above and add an & after

so effectively this converts it to an lvalue reference for any arg
then based on what T was you have an lvalue or an rvalue cast.

```cpp
take f(x)
T in template => int&
T in forward => int&
x in forward arg => int&
in static cast => int& && => int& via collapsing rules
return type is T&& => int& && => int&

now take f(10)
T in template => int
T in forward => int
x in forward arg => int&
in static cast => int&&
ret val is similarly int&&
```

This way the value category remains preserved.

### Ok but where does it all matter?

Well, if your'e doing heavy generics work...
Mostly would matter for pasing arguments and the copy / move behavior involved
there.
