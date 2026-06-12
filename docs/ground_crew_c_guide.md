# Ground Crew Producer Implementation and API Reference

This document provides a technical specification of the producer thread function implemented in `src/ground_crew.c` and explains the ground crew's role in the producer-consumer simulation.

---

## Architectural Overview

`src/ground_crew.c` implements the producer thread function that simulates ground crew workers unloading bags from aircraft onto the conveyor belt. Each ground crew thread creates `BAGS_PER_FLIGHT` `Bag` instances and calls `belt_load()` to place them on the shared belt, with simulated delays to represent real-world unloading work.

### System Architecture

``` mermaid
flowchart LR
    subgraph Producers ["Producer Threads"]
        GC1["Ground Crew 0<br/>(Flight 101)"]
        GC2["Ground Crew 1<br/>(Flight 202)"]
        GCN["Ground Crew N<br/>(Flight ...)"]
    end

    subgraph Conveyor_Belt["Conveyor Belt (g_belt Shared State)"]
        direction TB
        Slots(("slots[MAX_BUFFER]"))
        SemEmpty[(sem: empty)]
        SemFull[(sem: full)]
    end

    subgraph Consumers ["Consumers"]
        SW[Sorting Workers]
    end

    GC1 -->|"belt_load()"| Conveyor_Belt
    GC2 -->|"belt_load()"| Conveyor_Belt
    GCN -->|"belt_load()"| Conveyor_Belt
    Conveyor_Belt -->|"belt_unload()"| SW

    classDef belt fill:#1f2937,stroke:#3b82f6,stroke-width:1px,color:#fff;
    class Conveyor_Belt belt
```

# Full source of `src/ground_crew.c`

The following is the literal, unmodified content of `src/ground_crew.c` in this workspace:

```c
#include "common.h"

void *ground_crew(void *arg)
{
    int id        = *(int *)arg;
    int flight_no = (id + 1) * 100 + id + 1;  /* e.g. 101, 202, 303 */
    char msg[128];

    snprintf(msg, sizeof(msg),
             "[Flight %d] Ground crew started. Unloading %d bags...",
             flight_no, BAGS_PER_FLIGHT);
    log_msg(msg);

    for (int i = 1; i <= BAGS_PER_FLIGHT; i++) {

        usleep(g_cfg.unload_delay_us);   /* simulate unloading time */

        Bag bag;
        bag.bag_id   = id * 100 + i;    /* unique bag ID            */
        bag.flight_id = flight_no;

       
        belt_load(bag);

        g_flight_stats[id].count++;

        snprintf(msg, sizeof(msg),
                 "[Flight %d] Unloaded Bag ID %-4d  --> Belt Status: %d/%d occupied",
                 flight_no, bag.bag_id,
                 g_belt.count, g_belt.capacity);
        log_msg(msg);

        print_belt();
    }

    snprintf(msg, sizeof(msg),
             "[Flight %d] All bags unloaded. Total: %d bags.",
             flight_no, g_flight_stats[id].count);
    log_msg(msg);

    return NULL;
}
```


# Known Architectural Notes

> [!NOTE]
> `src/ground_crew.c` defines the producer thread function (`ground_crew`) which is called via `pthread_create()` from `main.c`. Each instance runs independently as a producer, populating the shared conveyor belt with `Bag` items at a configurable rate.

This file contains no data structure definitions or global state; it is a pure thread function that uses externally-defined types and globals.

## Technical Walkthrough & Analysis

This section explains the `ground_crew()` function, its parameters, thread lifecycle, and interactions with the shared state and synchronization primitives.

### Function Signature & Parameter

**`void *ground_crew(void *arg)`**

- **Parameter**: `void *arg` — a pointer to an `int` (thread ID). Cast to `int *` and dereferenced to extract the ground crew index (e.g., 0, 1, 2...).
- **Return Value**: Always `NULL`. Required by `pthread_create()` contract.
- **Thread Semantics**: This function runs as an independent thread; multiple instances execute concurrently.

### Execution Flow

#### 1. Thread Initialization

```c
int id        = *(int *)arg;
int flight_no = (id + 1) * 100 + id + 1;  /* e.g. 101, 202, 303 */
char msg[128];
```

- **`id`**: The ground crew thread index (0 to `g_cfg.num_flights - 1`).
- **`flight_no`**: Derived flight number for logging. Formula: `(id + 1) * 100 + id + 1` produces unique values (e.g., id=0 → flight_no=101, id=1 → flight_no=202).
- **`msg`**: Local buffer for formatted logging messages (128 bytes).

#### 2. Start Message

```c
snprintf(msg, sizeof(msg),
         "[Flight %d] Ground crew started. Unloading %d bags...",
         flight_no, BAGS_PER_FLIGHT);
log_msg(msg);
```

- Formats an initialization message and logs it via `log_msg()`.
- **Dependencies**: `BAGS_PER_FLIGHT` (macro from `common.h`), `log_msg()` (function from `logger.c`).

#### 3. Main Unloading Loop

```c
for (int i = 1; i <= BAGS_PER_FLIGHT; i++) {

    usleep(g_cfg.unload_delay_us);   /* simulate unloading time */

    Bag bag;
    bag.bag_id   = id * 100 + i;    /* unique bag ID            */
    bag.flight_id = flight_no;

   
    belt_load(bag);

    g_flight_stats[id].count++;

    snprintf(msg, sizeof(msg),
             "[Flight %d] Unloaded Bag ID %-4d  --> Belt Status: %d/%d occupied",
             flight_no, bag.bag_id,
             g_belt.count, g_belt.capacity);
    log_msg(msg);

    print_belt();
}
```

- **Loop Count**: `BAGS_PER_FLIGHT` (typically 10) iterations. Each iteration represents unloading one bag.
- **Delay Simulation**: `usleep(g_cfg.unload_delay_us)` pauses to simulate work time. The delay (in microseconds) is configured interactively in `main.c`.
- **Bag Creation**: Constructs a `Bag` struct with:
  - `bag_id`: Unique across all bags, formula: `id * 100 + i`. Example: ground crew 0 unloads bags 1, 2, ..., 10; crew 1 unloads 101, 102, ..., 110.
  - `flight_id`: The derived flight number.
- **Producer Action**: `belt_load(bag)` adds the bag to the shared conveyor belt. This is a blocking call if the belt is full (waiting on `empty` semaphore).
- **Statistics**: Increments `g_flight_stats[id].count` to track the number of bags unloaded by this crew.
- **Status Reporting**: Logs a message and calls `print_belt()` to display the current belt occupancy on the terminal.

**Synchronization Notes**:
- `g_belt.count` and `g_belt.capacity` are read inside the critical section of `print_belt()` (which acquires `g_belt.mutex`).
- `g_flight_stats[id]` is written to without a lock; this works because each `id` is unique to one thread (no thread accesses another thread's stats).

#### 4. Completion Message

```c
snprintf(msg, sizeof(msg),
         "[Flight %d] All bags unloaded. Total: %d bags.",
         flight_no, g_flight_stats[id].count);
log_msg(msg);

return NULL;
```

- Logs a completion message and returns `NULL` to satisfy the `pthread_create()` contract.
- The thread then terminates and can be joined in `main.c`.

## Cross-File Dependencies

Usage and callers:

```plaintext
[src/main.c]     -- spawns num_flights ground_crew threads via pthread_create()
                    passes &flight_id[i] as arg
                    calls pthread_join() to wait for completion

[src/ground_crew.c] -- implements ground_crew() function
                       includes common.h

[src/common.h]   -- declares ground_crew prototype
                    declares Bag, Belt, Config, Stats types
                    declares g_belt, g_cfg, g_flight_stats globals
                    declares log_msg, print_belt functions
                    defines BAGS_PER_FLIGHT, MAX_PRODUCERS macros

[src/belt.c]     -- implements belt_load() called by ground_crew()

[src/logger.c]   -- implements log_msg() and print_belt() called by ground_crew()
```

**Required Globals**:
- `g_cfg` (Config): specifically `g_cfg.unload_delay_us` and implicitly `g_cfg.num_flights`.
- `g_belt` (Belt): read for `g_belt.count` and `g_belt.capacity` in logging.
- `g_flight_stats` (Stats[]): write `g_flight_stats[id].count` for this thread.

**Required Function Calls**:
- `usleep()` (from `<unistd.h>`): simulates work delay.
- `snprintf()` (from `<stdio.h>`): formats messages.
- `log_msg()` (from `logger.c`): thread-safe logging.
- `belt_load()` (from `belt.c`): producer operation; blocks if belt full.
- `print_belt()` (from `logger.c`): displays belt status.

**Thread Safety**:
- Each thread has a unique `id` (0 to `num_flights - 1`) passed at creation time, so `g_flight_stats[id]` writes are non-overlapping.
- `log_msg()` and `print_belt()` use internal mutexes (`g_log_mutex`, `g_belt.mutex`) for thread safety.
- `belt_load()` manages its own synchronization with semaphores and mutex.