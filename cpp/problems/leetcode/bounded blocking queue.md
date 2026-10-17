# Bounded blocking queue

https://leetcode.doocs.org/en/lc/1188/

It's MPMC queue. No constraints on lockness ofc.

## Solution

Since even the Vyukov mpmc is not lock free ( technically ), using atomics is an
overkill, easiest is the two sema version.

This is the sushi chef problem basically.

Key elements include a mutex and two semas for free and taken tickets.
