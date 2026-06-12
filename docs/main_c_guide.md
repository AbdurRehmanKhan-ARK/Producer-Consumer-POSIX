# Program Entry-Point Implementation and API Reference

This document provides a detailed technical specification of the program entry-point and controller logic implemented in `src/main.c`. It explains initialization, configuration, threading, shutdown, and cleanup flow.

---

## Architectural Overview

`src/main.c` is the orchestrator of the producer-consumer simulation. It:
- Defines global shared state (`g_belt`, `g_cfg`, stats arrays, logging mutex and logfile).
- Collects configuration from user input.
- Initializes the belt and logging systems.
- Spawns producer and consumer threads.
- Coordinates a graceful shutdown via sentinel bags and a SIGINT handler.
- Prints a final report and cleans up resources.

### System Architecture

``` mermaid
flowchart TD
    N_Start(["main()"]) --> Config["Collect user config\n(get_positive_int)"]
    Config --> Init["Initialize globals\nopen logfile\nbelt_init()"]
    Init --> Threads["Spawn producer & consumer threads"]
    Threads --> JoinProducers["Join producer threads"]
    JoinProducers --> Shutdown["Send sentinel to belt"]
    Shutdown --> JoinConsumers["Join consumer threads"]
    JoinConsumers --> Report["print_stats()"]
    Report --> Cleanup["belt_destroy()\nclose logfile"]
    Cleanup --> End([Exit])

    SigInt((SIGINT)) -->|"handle_sigint()"| Shutdown
```

# Full source of `src/main.c`

The following is the literal, unmodified content of `src/main.c` in this workspace:

```c
#include "common.h"

/* ── Global definitions ── */
Belt            g_belt;
Config          g_cfg;
Stats           g_flight_stats[MAX_PRODUCERS];
Stats           g_worker_stats[MAX_CONSUMERS];
volatile int    g_done      = 0;
pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE           *g_logfile   = NULL;

void handle_sigint(int sig)
{
    (void)sig;
    printf("\n\033[31m[AIRPORT] Emergency shutdown! Sending all workers home...\033[0m\n");
    g_done = 1;
    /* Wake any blocked sorters with sentinel bags */
    for (int i = 0; i < g_cfg.num_workers; i++) {
        Bag pill;
        pill.bag_id   = SENTINEL;
        pill.flight_id = -1;
        belt_load(pill);
    }
}


static int get_positive_int(const char *prompt, int min_val, int max_val)
{
    int val;
    while (1) {
        printf("%s (min %d, max %d): ", prompt, min_val, max_val);
        fflush(stdout);
        if (scanf("%d", &val) == 1 && val >= min_val && val <= max_val)
            return val;
        printf("  Invalid input. Please enter a number between %d and %d.\n",
               min_val, max_val);
        /* flush bad input */
        int c;
        while ((c = getchar()) != '\n' && c != EOF);
    }
}


static void print_scenario_hint(void)
{
    printf("\n\033[1;36m--- Bottleneck Scenario Guide ---\033[0m\n");
    printf("  Fast Crew / Slow Workers (belt stays FULL, crews BLOCK):\n");
    printf("    Unload delay: 100000  (0.1s)   Sort delay: 800000 (0.8s)\n\n");
    printf("  Slow Crew / Fast Workers (belt stays EMPTY, workers IDLE):\n");
    printf("    Unload delay: 800000  (0.8s)   Sort delay: 100000 (0.1s)\n\n");
    printf("  Balanced:\n");
    printf("    Unload delay: 400000  (0.4s)   Sort delay: 400000 (0.4s)\n");
    printf("---------------------------------\n\n");
}


int main(void)
{
    /* ── Header ── */
    printf("\033[1;35m");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║     Airport Baggage Handling Simulation          ║\n");
    printf("║     OS Final Project — Producer Consumer         ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf("\033[0m\n");

    print_scenario_hint();

    /* ── User Input ── */
    printf("\033[1;33m=== Configure Your Simulation ===\033[0m\n");

    g_cfg.num_flights  = get_positive_int("  Number of flights  (producers)", 1, MAX_PRODUCERS);
    g_cfg.num_workers  = get_positive_int("  Number of sorters  (consumers)", 1, MAX_CONSUMERS);
    g_cfg.belt_size    = get_positive_int("  Belt capacity      (buffer size)", 2, MAX_BUFFER);
    g_cfg.unload_delay_us = get_positive_int("  Unload delay in microseconds (e.g. 400000)", 10000, 2000000);
    g_cfg.sort_delay_us   = get_positive_int("  Sort delay   in microseconds (e.g. 400000)", 10000, 2000000);

    /* ── Open log file (Lab 10 - filesystem calls) ── */
    g_logfile = fopen("simulation.log", "w");
    if (!g_logfile)
        fprintf(stderr, "Warning: could not open simulation.log\n");

    for (int i = 0; i < g_cfg.num_flights; i++) {
        g_flight_stats[i].id    = i;
        g_flight_stats[i].count = 0;
    }
    for (int i = 0; i < g_cfg.num_workers; i++) {
        g_worker_stats[i].id    = i;
        g_worker_stats[i].count = 0;
    }

    /* ── Install signal handler ── */
    signal(SIGINT, handle_sigint);

    /* ── Set belt capacity from user input, then init ── */
    g_belt.capacity = g_cfg.belt_size;
    belt_init();

    /* ── Print final config summary ── */
    printf("\n\033[1;32m=== Simulation Starting ===\033[0m\n");
    printf("  Flights (producers) : %d\n", g_cfg.num_flights);
    printf("  Sorters (consumers) : %d\n", g_cfg.num_workers);
    printf("  Belt capacity       : %d bags\n", g_cfg.belt_size);
    printf("  Unload delay        : %d µs\n", g_cfg.unload_delay_us);
    printf("  Sort delay          : %d µs\n", g_cfg.sort_delay_us);
    printf("  Bags per flight     : %d\n", BAGS_PER_FLIGHT);
    printf("  Total bags expected : %d\n\n", g_cfg.num_flights * BAGS_PER_FLIGHT);

    /* ── Thread arrays ── */
    pthread_t flight_tid[MAX_PRODUCERS];
    pthread_t worker_tid[MAX_CONSUMERS];
    int       flight_id [MAX_PRODUCERS];
    int       worker_id [MAX_CONSUMERS];

   
    for (int i = 0; i < g_cfg.num_workers; i++) {
        worker_id[i] = i;
        if (pthread_create(&worker_tid[i], NULL, sorter, &worker_id[i]) != 0) {
            perror("pthread_create sorter");
            return 1;
        }
    }

    for (int i = 0; i < g_cfg.num_flights; i++) {
        flight_id[i] = i;
        if (pthread_create(&flight_tid[i], NULL, ground_crew, &flight_id[i]) != 0) {
            perror("pthread_create ground_crew");
            return 1;
        }
    }

    for (int i = 0; i < g_cfg.num_flights; i++)
        pthread_join(flight_tid[i], NULL);


    printf("\n\033[33m[AIRPORT] All flights unloaded. Sending workers home...\033[0m\n");
    Bag sentinel;
    sentinel.bag_id   = SENTINEL;
    sentinel.flight_id = -1;
    belt_load(sentinel);   /* one sentinel, re-posted by each worker */

    /* ── Wait for all sorters to finish ── */
    for (int i = 0; i < g_cfg.num_workers; i++)
        pthread_join(worker_tid[i], NULL);

    /* ── Final report ── */
    print_stats();

    /* ── Cleanup ── */
    belt_destroy();
    pthread_mutex_destroy(&g_log_mutex);
    if (g_logfile) fclose(g_logfile);

    printf("[AIRPORT] Simulation complete. Full log saved to simulation.log\n\n");
    return 0;
}
```


# Known Architectural Notes

> [!NOTE]
> `src/main.c` is the program entry-point and the only file that defines global shared state. All other modules depend on globals declared in `common.h` and instantiated here.

`main.c` is also the only translation unit that installs the SIGINT handler and coordinates shutdown by injecting sentinel bags into the belt.

## Technical Walkthrough & Analysis

This section explains each top-level function and the full runtime flow controlled by `main()`.

### 1. Global Definitions

```c
Belt            g_belt;
Config          g_cfg;
Stats           g_flight_stats[MAX_PRODUCERS];
Stats           g_worker_stats[MAX_CONSUMERS];
volatile int    g_done      = 0;
pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE           *g_logfile   = NULL;
```

- **Purpose**: Define the shared state declared in `common.h`.
- **`g_belt`**: Global conveyor belt instance used by `belt.c` and read by logger/threads.
- **`g_cfg`**: Global configuration structure populated from user input.
- **`g_flight_stats` / `g_worker_stats`**: Per-thread counters used for reporting.
- **`g_done`**: Global shutdown flag (set on SIGINT). Used for signaling purposes.
- **`g_log_mutex`**: Mutex that serializes stdout/logfile output in `logger.c`.
- **`g_logfile`**: Optional file handle for `simulation.log`.

### 2. `handle_sigint`

```c
void handle_sigint(int sig)
```

- **Purpose**: Handle SIGINT (Ctrl+C) by signaling shutdown and waking any blocked workers.
- **Behavior**:
  - Prints an emergency shutdown banner.
  - Sets `g_done = 1`.
  - Injects one sentinel bag per worker into the belt to wake any blocked consumers.
- **Dependencies**: `g_cfg.num_workers`, `SENTINEL`, `belt_load()`.
- **Thread Safety**: Uses `belt_load()` which handles its own synchronization; logging here uses `printf` directly.

### 3. `get_positive_int`

```c
static int get_positive_int(const char *prompt, int min_val, int max_val)
```

- **Purpose**: Robustly read an integer within `[min_val, max_val]` from stdin.
- **Behavior**:
  - Prints a prompt with min/max.
  - Uses `scanf` to parse an integer.
  - On invalid input, prints an error message and flushes the remaining line.
- **Side Effects**: Performs blocking stdin reads; repeatedly prompts until valid input is received.

### 4. `print_scenario_hint`

```c
static void print_scenario_hint(void)
```

- **Purpose**: Display suggested parameter combinations for different bottleneck scenarios.
- **Use**: Called at program start to guide user configuration.

### 5. `main`

```c
int main(void)
```

#### 5.1 Banner & Scenario Hints

- Prints a stylized program header using ANSI color codes and Unicode box characters.
- Calls `print_scenario_hint()` to display performance scenarios.

#### 5.2 Configuration Input

- Prompts user for:
  - `g_cfg.num_flights` (producers)
  - `g_cfg.num_workers` (consumers)
  - `g_cfg.belt_size`
  - `g_cfg.unload_delay_us`
  - `g_cfg.sort_delay_us`
- Each value is validated by `get_positive_int()` within fixed bounds.

#### 5.3 Logfile Initialization

- Opens `simulation.log` in write mode.
- If open fails, prints a warning to stderr but continues execution.

#### 5.4 Stats Initialization

- Initializes `g_flight_stats[i]` and `g_worker_stats[i]` with ids and zero counts.

#### 5.5 Signal Handler Installation

- Installs `handle_sigint` with `signal(SIGINT, handle_sigint)`.

#### 5.6 Belt Initialization

- Sets `g_belt.capacity` from `g_cfg.belt_size`.
- Calls `belt_init()` to initialize the belt mutex and semaphores.

#### 5.7 Startup Summary

- Prints the final configuration summary, including total expected bags.

#### 5.8 Thread Creation

- Allocates arrays of `pthread_t` and thread IDs for producers and consumers.
- Spawns consumer threads first (`sorter`) so they can block on an empty belt.
- Spawns producer threads (`ground_crew`) to begin unloading bags.
- If thread creation fails, prints error and exits with status 1.

#### 5.9 Producer Join

- Waits for all producer threads (`ground_crew`) to finish unloading bags.

#### 5.10 Shutdown Signaling

- Prints a shutdown banner.
- Creates a `sentinel` bag with `bag_id == SENTINEL` and `flight_id == -1`.
- Calls `belt_load(sentinel)` once; each worker re-posts the sentinel to wake the next worker.

#### 5.11 Consumer Join

- Waits for all consumer threads to exit after receiving sentinel.

#### 5.12 Final Report

- Calls `print_stats()` to output a summary of all flights and workers.

#### 5.13 Cleanup and Exit

- Calls `belt_destroy()` to release belt synchronization primitives.
- Destroys `g_log_mutex`.
- Closes `g_logfile` if open.
- Prints a completion message and returns 0.

## Cross-File Dependencies

Usage and callers:

```plaintext
[src/main.c]       -- defines global shared state
                    calls belt_init(), belt_destroy()
                    spawns ground_crew() and sorter() threads
                    calls print_stats(), print_scenario_hint(), get_positive_int()
                    installs handle_sigint()

[src/common.h]     -- declares globals, types, and prototypes used here

[src/belt.c]       -- provides belt_init(), belt_load(), belt_destroy()

[src/ground_crew.c] -- provides ground_crew() producer function

[src/sorter.c]     -- provides sorter() consumer function

[src/logger.c]     -- provides print_stats() and log utilities
```

**Required Globals**:
- `g_belt`, `g_cfg`, `g_flight_stats`, `g_worker_stats`, `g_log_mutex`, `g_logfile`, `g_done` (all defined here, declared in `common.h`).

**Threading & Synchronization**:
- Producer threads call `belt_load()` and update `g_flight_stats`.
- Consumer threads call `belt_unload()` / `belt_load()` and update `g_worker_stats`.
- Logging uses `g_log_mutex` to serialize stdout/logfile output.

**Shutdown Protocol**:
- `handle_sigint()` sets `g_done` and injects one sentinel per worker to unblock consumers on SIGINT.
- Normal shutdown injects one sentinel that is re-posted by each worker for graceful termination.