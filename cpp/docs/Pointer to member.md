# Pointer to member

This is "different" enough in cpp to be it's own category.

`int Widget::* pm = &Widget::actual_data_member;`
notice the type is `int Widget::*` so it's an int pointer to the `actual_data_member` variable
inside of some `Widget` object.

Which `Widget` object though, we've just specified a class here? This is why they are different
from pointers, at the memory location of an ordinary pointer, you'll find the value, here you need
one more level => usage with the actual object.

```cpp
struct Widget {
    int x;
    int y;
};

Widget a{10, 20};
Widget b{30, 40};

int Widget::* pm = &Widget::y;

std::cout << a.*pm; // 20
std::cout << b.*pm; // 40
```

Reiterating
pm => pointer to non static member of Widget, where member is of type int
`.*` => pointer to member accessor operator

### Pointer to member function

`void (X::*)()`, pointer to member => then you add function stuff like return type and the args (empty
here)

## Some cases

### What about inheritance?

```cpp
struct Base {
    int x;
};

struct Derived : Base {
    int y;
};

int Base::* p = &Base::x;

Derived d;
d.*p = 42;   // OK
```

You can use a base class pointer to member on a derived object.

this is valid too

```cpp
int Base::* pb = &Base::x;

int Derived::* pd = pb;
```

Reverse => casting derived to base is trickier as member might not exist in base.

What is there's ambiguity?

```cpp
struct A {
    int x;
};

struct B1 : A {};
struct B2 : A {};

struct D : B1, B2 {};

int A::* p = &A::x;
```

If you use p on an object o D, which A will it use?
A simpler first problem is that what would be the result of?

```cpp
D d;
D.*p

// solution:
// error: ‘A’ is an ambiguous base of ‘D’
// At d.*p, C++ cannot choose between D::B1::A::x and D::B2::A::x.
```

What can we do if I know which one I want? basically we need to be explicit here
both these work

```cpp
B1& b1 = d;
b1.*p = 10;

static_cast<B2&>(d).*p;
```

### Virtual dispatch

This one is a bit obvious but helps to write it down so as not be suprised

```cpp
struct A {
    virtual void f();
};

struct B : A {
    void f() override;
};

void (A::*p)() = &A::f;

B b;
(b.*p)();
```

Which function is called? it's B's f

### Sizes

```cpp
struct X {};

sizeof(int X::*)
sizeof(void (X::*)())
```

can their sizes be defined reliably? no, implementationd defined as such pointers need to account
for any levels of inheritance.
