# Logging and Reporting Implementation and API Reference

This document provides a technical specification of the logging and reporting functions implemented in `src/logger.c` and explains how they support console and file output in the producer-consumer simulation.

---

## Architectural Overview

`src/logger.c` contains the logging utilities used by producers, consumers, and the main orchestration thread. It provides:
- A synchronized log function (`log_msg`) that writes to stdout and an optional logfile.
- A belt visualization (`print_belt`) that renders the current occupancy of the shared conveyor belt.
- A final report function (`print_stats`) that summarizes simulation statistics.

### System Architecture

``` mermaid
flowchart LR
    subgraph Producers ["Producers"]
        GC[Ground Crew Threads]
    end

    subgraph Consumers ["Consumers"]
        SW[Sorting Workers]
    end

    subgraph Logger ["Logging & Reporting"]
        N_LM["log_msg()"]
        N_PB["print_belt()"]
        N_PS["print_stats()"]
    end

    subgraph Shared_State ["Shared State"]
        Belt[g_belt]
        Stats["g_flight_stats / g_worker_stats"]
        Log[g_logfile]
    end

    GC -->|"log_msg()"| N_LM
    SW -->|"log_msg()"| N_LM
    GC -->|"print_belt()"| N_PB
    SW -->|"print_belt()"| N_PB
    N_PS -->|reads stats| Stats
    N_LM -->|writes| Log
    N_PB -->|reads| Belt

    classDef logger fill:#1f2937,stroke:#3b82f6,stroke-width:1px,color:#fff;
    class Logger logger
```

# Full source of `src/logger.c`

The following is the literal, unmodified content of `src/logger.c` in this workspace:

```c
#include "common.h"

void log_msg(const char *msg)
{
    pthread_mutex_lock(&g_log_mutex);

    printf("%s\n", msg);
    fflush(stdout);

    if (g_logfile) {
        fprintf(g_logfile, "%s\n", msg);
        fflush(g_logfile);
    }

    pthread_mutex_unlock(&g_log_mutex);
}


void print_belt(void)
{
    pthread_mutex_lock(&g_log_mutex);
    pthread_mutex_lock(&g_belt.mutex);

    int cap   = g_belt.capacity;
    int count = g_belt.count;

    pthread_mutex_unlock(&g_belt.mutex);

    
    char bar[MAX_BUFFER + 1];
    int i;
    for (i = 0; i < cap; i++)
        bar[i] = (i < count) ? '=' : ' ';
    bar[cap] = '\0';

    int pct = (cap > 0) ? (count * 100 / cap) : 0;
    const char *color = "\033[32m";   /* green  */
    if      (pct >= 80) color = "\033[31m";   /* red    */
    else if (pct >= 50) color = "\033[33m";   /* yellow */

    printf("%s  [Conveyor Belt: |%-*s| %d/%d bags ]  %s\n",
           color, cap, bar, count, cap, "\033[0m");
    fflush(stdout);

    pthread_mutex_unlock(&g_log_mutex);
}


void print_stats(void)
{
    printf("\n\033[1m======= AIRPORT SIMULATION REPORT =======\033[0m\n");

    int total_unloaded = 0, total_sorted = 0;
    int flight_no;

    for (int i = 0; i < g_cfg.num_flights; i++) {
        flight_no = (i + 1) * 100 + i + 1;
        printf("  Flight %d (Ground Crew %d) : %d bags unloaded\n",
               flight_no, i, g_flight_stats[i].count);
        total_unloaded += g_flight_stats[i].count;
    }

    printf("  -----------------------------------------\n");

    for (int i = 0; i < g_cfg.num_workers; i++) {
        printf("  Sorting Worker W%d         : %d bags sorted\n",
               i, g_worker_stats[i].count);
        total_sorted += g_worker_stats[i].count;
    }

    printf("  -----------------------------------------\n");
    printf("  Total bags unloaded : %d\n", total_unloaded);
    printf("  Total bags sorted   : %d\n", total_sorted);
    printf("\033[1m=========================================\033[0m\n\n");

    if (g_logfile) {
        fprintf(g_logfile, "\n======= AIRPORT SIMULATION REPORT =======\n");
        for (int i = 0; i < g_cfg.num_flights; i++) {
            flight_no = (i + 1) * 100 + i + 1;
            fprintf(g_logfile, "  Flight %d : %d bags unloaded\n",
                    flight_no, g_flight_stats[i].count);
        }
        for (int i = 0; i < g_cfg.num_workers; i++)
            fprintf(g_logfile, "  Worker W%d : %d bags sorted\n",
                    i, g_worker_stats[i].count);
        fprintf(g_logfile, "  Total unloaded: %d   sorted: %d\n",
                total_unloaded, total_sorted);
    }
}
```


# Known Architectural Notes

> [!NOTE]
> `src/logger.c` provides thread-safe output and reporting helpers used by both producers and consumers. `log_msg()` serializes access to stdout and the optional logfile via `g_log_mutex`.

This file contains no data structure definitions or global state; it relies on globals declared in `common.h` and initialized in `main.c`.

## Technical Walkthrough & Analysis

This section explains each logging utility in `src/logger.c`, its thread-safety guarantees, and how it interacts with shared state.

### 1. `log_msg`

```c
void log_msg(const char *msg)
```

- **Purpose**: Write a single log line to stdout and, if enabled, to `g_logfile`.
- **Thread Safety**: Uses `pthread_mutex_lock(&g_log_mutex)` to serialize logging across threads.
- **Side Effects**:
  - Always prints to stdout and flushes it.
  - If `g_logfile` is non-NULL, prints to the log file and flushes it.
- **Dependencies**: `g_log_mutex` and `g_logfile` are defined in `main.c` and declared in `common.h`.

### 2. `print_belt`

```c
void print_belt(void)
```

- **Purpose**: Render a live ASCII visualization of the belt occupancy, with color-coded thresholds.
- **Synchronization**:
  - Locks `g_log_mutex` to serialize output with other logging.
  - Locks `g_belt.mutex` to safely read `g_belt.capacity` and `g_belt.count`.
- **Rendering Details**:
  - Builds a `bar` string of length `cap` with `=` for filled slots and spaces for empty slots.
  - Computes `pct = count * 100 / cap` to select a color:
    - Green (<50%), Yellow (>=50%), Red (>=80%).
  - Prints a formatted line with the bar and occupancy values.
- **Dependencies**: Uses `MAX_BUFFER` for the bar buffer size; relies on `g_belt` and `g_log_mutex`.

### 3. `print_stats`

```c
void print_stats(void)
```

- **Purpose**: Print a final summary of all flights and workers, and totals of unloaded/sorted bags.
- **Data Sources**:
  - `g_cfg.num_flights`, `g_cfg.num_workers`
  - `g_flight_stats[i].count`, `g_worker_stats[i].count`
- **Console Output**: Uses ANSI escape codes for bold formatting and separators.
- **Log File Output**: Writes the same summary to `g_logfile` if it is open.
- **Dependencies**: `g_cfg`, `g_flight_stats`, `g_worker_stats`, and `g_logfile` are initialized in `main.c`.

## Cross-File Dependencies

Usage and callers:

```plaintext
[src/main.c]       -- initializes g_log_mutex and g_logfile
                    calls print_stats() after threads finish

[src/ground_crew.c] -- calls log_msg() and print_belt()

[src/sorter.c]     -- calls log_msg() and print_belt()

[src/common.h]     -- declares log_msg(), print_belt(), print_stats()
                    declares g_log_mutex, g_logfile, g_belt, g_cfg, stats arrays
```

**Required Globals**:
- `g_log_mutex` (pthread_mutex_t): protects stdout/logfile output.
- `g_logfile` (FILE *): optional log file handle.
- `g_belt` (Belt): read for capacity and count in `print_belt()`.
- `g_cfg`, `g_flight_stats`, `g_worker_stats`: read for report generation.

**Thread Safety**:
- `log_msg()` serializes output with `g_log_mutex`.
- `print_belt()` serializes output and safely reads belt fields with `g_belt.mutex`.
- `print_stats()` is called after worker threads join, so it does not require locks.