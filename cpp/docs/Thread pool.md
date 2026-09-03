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
