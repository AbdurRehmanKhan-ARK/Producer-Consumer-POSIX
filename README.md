<div align="center">

<img src="https://readme-typing-svg.demolab.com?font=JetBrains+Mono&weight=500&size=20&duration=4000&pause=1500&color=00C8FF&center=true&vCenter=true&width=700&lines=Producer-Consumer-POSIX;Concurrency+from+first+principles.;C+%7C+POSIX+Threads+%7C+Mutexes+%7C+Semaphores" alt="Typing SVG" />

<br/>

**OS Final Project — FAST-NUCES Karachi, Semester 4**
_Airport baggage handling simulation — the producer-consumer problem implemented at the systems level._

<br/>

![C](https://img.shields.io/badge/C-A8B9CC?style=flat-square&logo=c&logoColor=black)
![POSIX](https://img.shields.io/badge/POSIX_Threads-333333?style=flat-square)
![Semaphores](https://img.shields.io/badge/Semaphores-Mutexes-555555?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20WSL%20%7C%20MSYS2-lightgrey?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-blue?style=flat-square)
![Status](https://img.shields.io/badge/Status-Complete-brightgreen?style=flat-square)

</div>

---

## What This Is

The producer-consumer problem is one of the foundational problems in concurrent systems — what happens when work is generated faster or slower than it can be consumed, and both sides share a fixed-size buffer?

This project answers that question at the machine level. No high-level concurrency abstractions. No libraries wrapping the hard parts. Just C, POSIX threads, a mutex, and two semaphores coordinating multiple producers and consumers over a bounded circular buffer in shared RAM.

The simulation wraps the problem in a concrete scenario: an airport baggage belt. Ground crew threads load bags onto a conveyor. Sorter threads pull them off. The belt has a fixed capacity. When it fills, crews block. When it empties, sorters wait. The OS scheduler decides who runs next — and the synchronization primitives make sure that never corrupts the shared state.

---

## Scenario

```
  Ground Crew 0 ──┐
  Ground Crew 1 ──┤──► [ belt_load() ] ──► g_belt.slots[] ──► [ belt_unload() ] ──► Sorter 0
  Ground Crew 2 ──┘         │                                        │               Sorter 1
                         sem_wait(empty)                        sem_wait(full)
                         mutex_lock()                           mutex_lock()
                         write slot, advance tail               read slot, advance head
                         mutex_unlock()                         mutex_unlock()
                         sem_post(full)                         sem_post(empty)
```

Each producer unloads `BAGS_PER_FLIGHT = 10` bags. Each consumer pulls bags until it receives a sentinel (`bag_id == -1`). The sentinel propagates through the pool — each worker that sees it relays it forward and exits cleanly.

---

## Source Structure

| File                | What it does                                                                                                                           |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| `src/main.c`        | Entry point — reads config, initializes globals, spawns threads, handles `SIGINT`, joins threads, prints final report, frees resources |
| `src/common.h`      | Shared types — `Bag`, `Belt`, `Config`, `Stats` structs — macros, globals, and all function prototypes                                 |
| `src/belt.c`        | The bounded buffer — `belt_init()`, `belt_load()`, `belt_unload()`, `belt_destroy()` — the only file that touches `g_belt` directly    |
| `src/ground_crew.c` | Producer thread — loads bags, simulates unload delay via `usleep()`, updates per-flight stats                                          |
| `src/sorter.c`      | Consumer thread — unloads bags, simulates sort delay, relays sentinel, updates per-worker stats                                        |
| `src/logger.c`      | Synchronized output — `log_msg()`, `print_belt()`, `print_stats()` — all guarded by `g_log_mutex`                                      |

---

## The Synchronization Core

The belt (`g_belt`) is a circular array in shared RAM. Without protection, two threads writing simultaneously would corrupt `head`, `tail`, or `count`. The fix is three primitives working together:

**`sem_t empty`** — initialized to `belt_capacity`. A producer calls `sem_wait(&g_belt.empty)` before writing. If the belt is full, this blocks the producer until a consumer frees a slot.

**`sem_t full`** — initialized to 0. A consumer calls `sem_wait(&g_belt.full)` before reading. If the belt is empty, this blocks the consumer until a producer adds a bag.

**`pthread_mutex_t mutex`** — guards the actual read/write of `slots[]`, `head`, `tail`, and `count`. Held for the smallest possible window — just the index update and the slot access. Released immediately after.

```c
// Producer side — belt.c
void belt_load(Bag bag) {
    sem_wait(&g_belt.empty);          // block if belt is full
    pthread_mutex_lock(&g_belt.mutex);
    g_belt.slots[g_belt.tail] = bag;
    g_belt.tail = (g_belt.tail + 1) % g_belt.capacity;
    g_belt.count++;
    pthread_mutex_unlock(&g_belt.mutex);
    sem_post(&g_belt.full);           // signal: one more bag ready
}

// Consumer side — belt.c
Bag belt_unload(void) {
    sem_wait(&g_belt.full);           // block if belt is empty
    pthread_mutex_lock(&g_belt.mutex);
    Bag bag = g_belt.slots[g_belt.head];
    g_belt.head = (g_belt.head + 1) % g_belt.capacity;
    g_belt.count--;
    pthread_mutex_unlock(&g_belt.mutex);
    sem_post(&g_belt.empty);          // signal: one slot freed
    return bag;
}
```

This is the exact sequence that prevents buffer overflow, underflow, and torn updates.

---

## Build & Run

> ⚠️ Requires a POSIX-compatible environment. On Windows, use **WSL** or **MSYS2**.

```bash
# Clone
git clone https://github.com/AbdurRehmanKhan-ARK/Producer-Consumer-POSIX.git
cd Producer-Consumer-POSIX/src

# Build
make

# Run
./airport_sim

# Clean
make clean
```

From the repo root: `make -C src` and `make -C src clean`.

---

## Runtime Configuration

The program prompts for five values at startup:

| Input               | Range              | What it controls                          |
| ------------------- | ------------------ | ----------------------------------------- |
| Flights (producers) | 1 – 8              | Number of ground crew threads             |
| Sorters (consumers) | 1 – 8              | Number of sorting worker threads          |
| Belt capacity       | 2 – 20             | Size of the shared circular buffer        |
| Unload delay (µs)   | 10,000 – 2,000,000 | How long each producer waits between bags |
| Sort delay (µs)     | 10,000 – 2,000,000 | How long each consumer takes per bag      |

**Three scenarios worth trying:**

| Scenario            | Unload delay | Sort delay | What you observe                                 |
| ------------------- | ------------ | ---------- | ------------------------------------------------ |
| Producer bottleneck | 100,000 µs   | 800,000 µs | Belt stays full — crews block waiting for space  |
| Consumer bottleneck | 800,000 µs   | 100,000 µs | Belt stays empty — sorters idle waiting for bags |
| Balanced            | 400,000 µs   | 400,000 µs | Belt fluctuates — stable throughput              |

---

## What the Program Produces

- Colored console output — live belt occupancy meter, per-thread log lines
- `simulation.log` — full run log mirrored from stdout
- Final report — bags per producer and per consumer
- Clean shutdown — `SIGINT` handled gracefully via sentinel propagation

---

## Author

**Abdur Rehman Khan**
BSCS — FAST-NUCES Karachi
[abdurrehmankhan0909@gmail.com](mailto:abdurrehmankhan0909@gmail.com)
[GitHub](https://github.com/AbdurRehmanKhan-ARK)

---

<div align="center">

_The buffer is only safe because every update has a clearly defined owner,_
_a protected critical section, and a wake-up rule for the other side._

</div>
