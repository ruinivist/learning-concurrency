# Concurrency Problems

## 1. Two-thread ping-pong

Print `ping` and `pong` alternately 10 times via two threads.

## 2. Horse race

Simulate a horse race with three threads. Each horse advances by a random 1–4 positions toward a finish line 25 positions away. Print the race after every round and announce the winner when the first horse finishes.

## 3. Sushi Chefs: producer consumer

Simulate `P` chefs and `C` customers sharing a conveyor belt with `N` slots. Chefs prepare sushi and customers eat it. A chef waits when the belt is full, and a customer waits when it is empty. Infinite simulation.

## 4. Museum decorators: reader-writer lock

Simulate a museum with `V` visitors and `C` curators using threads. Multiple visitors may visit at the same time. Only one curator may renovate at a time, and renovation can begin only after all visitors have left. No visitor may enter while renovation is in progress. Stop after all visitors and curators have finished.

Have "writer" preference as in visitors that keep pn arriving, must not starve.
Use "blocking" apis.

## 5. Game lobby: reusable barrier

Create `N` player threads that repeatedly join a game lobby. Each player arrives after a random delay. Print `Game found` when all players have arrived, then reset the lobby and repeat forever.
