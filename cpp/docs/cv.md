# cv

Wait on a lock so you need a lock. This is for signalling purposes
where you want => wait till cond holds

- `cv.wait(lk)` where you check the condition but that is error-prone
- `cv.wait(lk, cond)` ALWAYS prefer this; read as "continue when condition holds ( and signalled )"
  - internally this is just `while(!cond) cv.wait(lk)`
  - this above is not a spin lock, cv has a lot of CLEVER logic within ( TODO )

by far very easy to get wrong. some idioms

- Always check condition first before waiting ( use the lambda as condition to avoid wait )
- Notify after unlocking and modifying ALL the state you need to modify

Something like this is almost always correct.

```cpp
{
    lock
    cv.wait(lock, cond)

    do stuff
}

cv.notify
```

How to name cvs?

- Do not use by role as in reader_cv or producer_cv
- Use by condition being waited for in the cv

Side effects in cv wait predicate?

- generally avoid it, makes it harder to reason exactly what
  the wait condition is. Also it MUST be idempotent and a read only
  no side effect predicate is easier to get right.

think of notify as a check condition again case, it's fine to have some
superflous ones if the code is cleaner.
